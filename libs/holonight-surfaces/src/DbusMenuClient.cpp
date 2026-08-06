#include "DbusMenuClient.h"

#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>
#include <QDateTime>
#include <QLoggingCategory>
#include <QSharedPointer>
#include <QTimer>
#include <QVariantMap>

Q_LOGGING_CATEGORY(lcDbusMenu, "holonight.tray.dbusmenu")

static constexpr auto kDbusMenuIface = "com.canonical.dbusmenu";

DbusMenuClient::DbusMenuClient(QObject* parent) : QObject(parent), bus_(QDBusConnection::sessionBus()) {}

void DbusMenuClient::open(const QString& service, const QString& object_path, int screen_x, int screen_y) {
  service_ = service;
  path_ = object_path;
  pending_x_ = screen_x;
  pending_y_ = screen_y;
  menu_open_ = true;
  doGetLayout();
}

void DbusMenuClient::close() {
  if (!menu_open_) {
    return;
  }
  menu_open_ = false;
  service_.clear();
  path_.clear();
  Q_EMIT menuClosed();
}

void DbusMenuClient::activateItem(int item_id) {
  if (service_.isEmpty()) {
    return;
  }
  auto msg = QDBusMessage::createMethodCall(service_, path_, QLatin1String(kDbusMenuIface), QStringLiteral("Event"));
  msg << item_id << QStringLiteral("clicked") << QVariant::fromValue(QDBusVariant(QString{}))
      << static_cast<uint32_t>(QDateTime::currentSecsSinceEpoch());
  bus_.call(msg, QDBus::NoBlock);
  QTimer::singleShot(0, this, &DbusMenuClient::close);
  qCInfo(lcDbusMenu) << "Event activated item" << item_id;
}

void DbusMenuClient::doGetLayout() {
  qCInfo(lcDbusMenu) << "GetLayout for" << service_ << path_;
  auto msg =
      QDBusMessage::createMethodCall(service_, path_, QLatin1String(kDbusMenuIface), QStringLiteral("GetLayout"));
  msg << 0 << -1 << QStringList{};
  auto* pending = new QDBusPendingCallWatcher(bus_.asyncCall(msg, 5000), this);
  connect(pending, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* ptr) {
    ptr->deleteLater();
    if (ptr->isError()) {
      qCWarning(lcDbusMenu) << "GetLayout failed:" << ptr->error().message();
      Q_EMIT menuFailed();
      return;
    }

    // Access arguments directly — GetLayout returns u(ia{sv}av), not uv, so
    // QDBusPendingReply<uint, QDBusVariant> would reject it on signature check.
    const QList<QVariant> args = ptr->reply().arguments();
    if (args.size() < 2) {
      qCWarning(lcDbusMenu) << "GetLayout reply has too few arguments";
      Q_EMIT menuFailed();
      return;
    }
    const QVariant& root_variant = args.at(1);
    QDBusArgument root_arg;
    if (root_variant.canConvert<QDBusVariant>()) {
      root_arg = root_variant.value<QDBusVariant>().variant().value<QDBusArgument>();
    } else if (root_variant.canConvert<QDBusArgument>()) {
      root_arg = root_variant.value<QDBusArgument>();
    } else {
      qCWarning(lcDbusMenu) << "GetLayout root item has unexpected type" << root_variant.typeName();
      Q_EMIT menuFailed();
      return;
    }

    DbusMenuItem root = parseItem(root_arg);
    qCInfo(lcDbusMenu) << "GetLayout parsed" << root.children.size() << "root items";

    auto* model = new DbusMenuModel(root.children, this);
    QList<int> root_ids;
    root_ids.reserve(root.children.size());
    for (const DbusMenuItem& child : root.children) {
      root_ids.append(child.id);
    }
    doAboutToShow(root_ids, model, pending_x_, pending_y_);
  });
}

void DbusMenuClient::doAboutToShow(const QList<int>& root_ids, DbusMenuModel* model, int screen_x, int screen_y) {
  if (root_ids.isEmpty()) {
    Q_EMIT menuReady(model, screen_x, screen_y);
    return;
  }

  auto pending_count = QSharedPointer<int>::create(root_ids.size());
  auto finished = QSharedPointer<bool>::create(false);
  auto* timeout = new QTimer(this);
  timeout->setSingleShot(true);
  timeout->setInterval(kAboutToShowTimeoutMs);

  auto finish = [this, model, screen_x, screen_y, timeout, finished]() {
    if (*finished) {
      return;
    }
    *finished = true;
    timeout->stop();
    timeout->deleteLater();
    Q_EMIT menuReady(model, screen_x, screen_y);
  };

  connect(timeout, &QTimer::timeout, this, finish);
  timeout->start();

  for (int root_id : root_ids) {
    auto msg =
        QDBusMessage::createMethodCall(service_, path_, QLatin1String(kDbusMenuIface), QStringLiteral("AboutToShow"));
    msg << root_id;
    auto* pending = new QDBusPendingCallWatcher(bus_.asyncCall(msg, 5000), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this, [pending_count, finish](QDBusPendingCallWatcher* ptr) {
      ptr->deleteLater();
      if (--(*pending_count) == 0) {
        finish();
      }
    });
  }
}

DbusMenuItem DbusMenuClient::makeItem(int item_id, const QVariantMap& props, QList<DbusMenuItem> children) {
  DbusMenuItem item;
  item.id = item_id;
  item.label = props.value(QStringLiteral("label")).toString();
  item.type = props.value(QStringLiteral("type"), QStringLiteral("standard")).toString();
  item.icon_name = props.value(QStringLiteral("icon-name")).toString();
  item.enabled = props.value(QStringLiteral("enabled"), true).toBool();
  item.visible = props.value(QStringLiteral("visible"), true).toBool();
  item.toggle_type = props.value(QStringLiteral("toggle-type")).toString();
  item.toggle_state = props.value(QStringLiteral("toggle-state"), -1).toInt();
  item.children = std::move(children);
  return item;
}

DbusMenuItem DbusMenuClient::parseItem(const QDBusArgument& arg, int depth) {
  int item_id = 0;
  QVariantMap props;
  QList<DbusMenuItem> children;

  arg.beginStructure();
  arg >> item_id;
  arg >> props;

  if (depth < kMaxDepth) {
    arg.beginArray();
    while (!arg.atEnd()) {
      QVariant child_variant;
      arg >> child_variant;
      QDBusArgument child_arg;
      if (child_variant.canConvert<QDBusArgument>()) {
        child_arg = child_variant.value<QDBusArgument>();
      } else if (child_variant.canConvert<QDBusVariant>()) {
        child_arg = child_variant.value<QDBusVariant>().variant().value<QDBusArgument>();
      } else {
        continue;
      }
      children.append(parseItem(child_arg, depth + 1));
    }
    arg.endArray();
  } else {
    // Consume but discard children beyond max depth.
    QVariant ignored;
    arg >> ignored;
  }

  arg.endStructure();
  return makeItem(item_id, props, std::move(children));
}

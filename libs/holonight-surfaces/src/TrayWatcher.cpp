#include "TrayWatcher.h"

#include "TrayItem.h"
#include "TrayItemProperties.h"
#include "TrayModel.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusVariant>
#include <QLoggingCategory>
#include <QVariant>

#include <memory>

Q_LOGGING_CATEGORY(lcTrayWatcher, "holonight.tray.watcher")

// Per-item PropertiesChanged receiver — stores the item key so the signal
// can route back to TrayWatcher::updateItemProperties without needing QDBusContext.
class ItemPropWatcher : public QObject {
  Q_OBJECT
 public:
  ItemPropWatcher(QString item_key, TrayWatcher* owner, QObject* parent)
      : QObject(parent), key_(std::move(item_key)), owner_(owner) {}

 public Q_SLOTS:
  void onPropertiesChanged(const QString& /*iface*/, const QVariantMap& changed, const QStringList& /*invalidated*/) {
    owner_->onItemPropertiesChanged(key_, changed);
  }

 private:
  QString key_;
  TrayWatcher* owner_;
};

// Per-item receiver for direct SNI signals (NewIcon, NewStatus, etc.).
// Constructed alongside ItemPropWatcher; uses separate named slots so each
// signal can be individually connected.
class ItemSignalWatcher : public QObject {
  Q_OBJECT
 public:
  ItemSignalWatcher(QString item_key, QString service, QString path, TrayWatcher* owner, QObject* parent)
      : QObject(parent),
        key_(std::move(item_key)),
        service_(std::move(service)),
        path_(std::move(path)),
        owner_(owner) {}

 public Q_SLOTS:
  void onNewIcon() {
    owner_->onItemNeedsPropertyFetch(key_, QStringLiteral("IconName"));
    owner_->onItemNeedsPropertyFetch(key_, QStringLiteral("IconPixmap"));
  }
  void onNewAttentionIcon() {
    owner_->onItemNeedsPropertyFetch(key_, QStringLiteral("AttentionIconName"));
    owner_->onItemNeedsPropertyFetch(key_, QStringLiteral("AttentionIconPixmap"));
  }
  void onNewStatus(const QString& status) {
    owner_->onItemDirectSignal(key_, QStringLiteral("Status"), QVariant(status));
  }
  void onNewTitle() { owner_->onItemNeedsPropertyFetch(key_, QStringLiteral("Title")); }
  void onNewToolTip() { owner_->onItemNeedsToolTipFetch(key_); }

  const QString& service() const { return service_; }  // NOLINT(modernize-use-nodiscard): exposed as a Qt slot.
  const QString& path() const { return path_; }        // NOLINT(modernize-use-nodiscard): exposed as a Qt slot.

 private:
  QString key_;
  QString service_;
  QString path_;
  TrayWatcher* owner_;
};

static constexpr auto kWatcherService = "org.kde.StatusNotifierWatcher";
static constexpr auto kFreedesktopWatcherService = "org.freedesktop.StatusNotifierWatcher";
static constexpr auto kWatcherPath = "/StatusNotifierWatcher";
static constexpr auto kWatcherIface = "org.kde.StatusNotifierWatcher";
static constexpr auto kSniIface = "org.kde.StatusNotifierItem";
static constexpr auto kPropsIface = "org.freedesktop.DBus.Properties";

class QtTrayRegistrationClient final : public TrayRegistrationClient {
 public:
  bool registerHost(const QString& service) override { return QDBusConnection::sessionBus().registerService(service); }

  TrayWatcherRegistrationResult registerWatcher(QObject* watcher) override {
    auto* iface = QDBusConnection::sessionBus().interface();
    if (iface == nullptr) {
      qCWarning(lcTrayWatcher) << "no session bus interface";
      return TrayWatcherRegistrationResult::Failed;
    }

    QDBusReply<QDBusConnectionInterface::RegisterServiceReply> reply =
        iface->registerService(kWatcherService, QDBusConnectionInterface::ReplaceExistingService,
                               QDBusConnectionInterface::DontAllowReplacement);
    if (!reply.isValid()) {
      qCWarning(lcTrayWatcher) << "registerService failed:" << reply.error().message();
      return TrayWatcherRegistrationResult::Failed;
    }

    if (reply.value() == QDBusConnectionInterface::ServiceRegistered) {
      QDBusConnection::sessionBus().registerObject(
          kWatcherPath, watcher,
          QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties);
      QDBusReply<QDBusConnectionInterface::RegisterServiceReply> alias_reply =
          iface->registerService(kFreedesktopWatcherService, QDBusConnectionInterface::ReplaceExistingService,
                                 QDBusConnectionInterface::DontAllowReplacement);
      if (!alias_reply.isValid() || alias_reply.value() != QDBusConnectionInterface::ServiceRegistered) {
        qCWarning(lcTrayWatcher) << "failed to register freedesktop watcher alias:"
                                 << (alias_reply.isValid() ? QStringLiteral("service already owned")
                                                           : alias_reply.error().message());
      }
      return TrayWatcherRegistrationResult::Registered;
    }

    return TrayWatcherRegistrationResult::AlreadyQueued;
  }

  QStringList registeredServices() override {
    auto* iface = QDBusConnection::sessionBus().interface();
    if (iface == nullptr) {
      return {};
    }

    const QDBusReply<QStringList> reply = iface->registeredServiceNames();
    return reply.isValid() ? reply.value() : QStringList{};
  }
};

static QStringList sortedKeys(const QVariantMap& values) {
  QStringList keys = values.keys();
  keys.sort();
  return keys;
}

QString normaliseTrayItemKey(const QString& raw) {
  if (raw.contains(QLatin1Char('/'))) {
    return raw;
  }
  return raw + QStringLiteral(":/StatusNotifierItem");
}

TrayItemAddress splitTrayItemKey(const QString& key) {
  qsizetype sep = key.indexOf(QStringLiteral(":/"));
  if (sep == -1) {
    return TrayItemAddress{.service = key, .path = QStringLiteral("/StatusNotifierItem")};
  }
  return TrayItemAddress{.service = key.left(sep), .path = key.sliced(sep + 1)};
}

bool registerTrayItemKey(QStringList& registered_items, QSet<QString>& registered_item_keys,
                         QHash<QString, int>& service_item_counts, const QString& raw) {
  const QString key = normaliseTrayItemKey(raw);
  if (registered_item_keys.contains(key)) {
    return false;
  }

  registered_items.append(key);
  registered_item_keys.insert(key);
  const TrayItemAddress address = splitTrayItemKey(key);
  service_item_counts.insert(address.service, service_item_counts.value(address.service) + 1);
  return true;
}

bool unregisterTrayItemKey(QStringList& registered_items, QSet<QString>& registered_item_keys,
                           QHash<QString, int>& service_item_counts, const QString& raw) {
  const QString key = normaliseTrayItemKey(raw);
  if (!registered_item_keys.remove(key)) {
    return false;
  }
  registered_items.removeOne(key);

  const TrayItemAddress address = splitTrayItemKey(key);
  auto count_pos = service_item_counts.find(address.service);
  if (count_pos != service_item_counts.end()) {
    --count_pos.value();
    if (count_pos.value() <= 0) {
      service_item_counts.erase(count_pos);
    }
  }
  return true;
}

QStringList unregisterTrayServiceItems(QStringList& registered_items, QSet<QString>& registered_item_keys,
                                       QHash<QString, int>& service_item_counts, const QString& service) {
  QStringList removed;
  const QStringList snapshot = registered_items;
  for (const QString& key : snapshot) {
    if (splitTrayItemKey(key).service == service &&
        unregisterTrayItemKey(registered_items, registered_item_keys, service_item_counts, key)) {
      removed.append(key);
    }
  }
  return removed;
}

bool isOptionalTrayPropertyMissing(const QString& prop_name, const QString& error_name, const QString& error_message) {
  const bool optional_property = prop_name == QLatin1String("IconName") || prop_name == QLatin1String("IconPixmap") ||
                                 prop_name == QLatin1String("AttentionIconName") ||
                                 prop_name == QLatin1String("AttentionIconPixmap") ||
                                 prop_name == QLatin1String("ToolTip");
  if (!optional_property) {
    return false;
  }

  return error_name == QDBusError::errorString(QDBusError::InvalidArgs) ||
         error_message.contains(QStringLiteral("No such property"), Qt::CaseInsensitive) ||
         error_message == QLatin1String("error occurred in Get");
}

TrayWatcher::TrayWatcher(TrayModel* model, QObject* parent)
    : TrayWatcher(model, std::make_unique<QtTrayRegistrationClient>(), parent) {}

TrayWatcher::TrayWatcher(TrayModel* model, TrayRegistrationClientPtr registration, QObject* parent)
    : QObject(parent), bus_(QDBusConnection::sessionBus()), model_(model), registration_(std::move(registration)) {
  registerTrayMetaTypes();
}

void TrayWatcher::start() {
  if (started_) {
    return;
  }
  started_ = true;

  registerHost();
  tryRegisterWatcher();
}

void TrayWatcher::registerHost() {
  QString host_service =
      QStringLiteral("org.kde.StatusNotifierHost-holonight-%1").arg(QCoreApplication::applicationPid());
  bool registered = registration_->registerHost(host_service);
  if (!registered) {
    qCWarning(lcTrayWatcher) << "failed to register host service" << host_service;
  } else {
    qCInfo(lcTrayWatcher) << "registered host" << host_service;
  }
}

void TrayWatcher::tryRegisterWatcher() {
  const TrayWatcherRegistrationResult result = registration_->registerWatcher(this);
  if (result == TrayWatcherRegistrationResult::Failed) {
    fallbackReadItems();
    return;
  }

  if (result == TrayWatcherRegistrationResult::Registered) {
    is_watcher_ = true;
    qCInfo(lcTrayWatcher) << "registered as StatusNotifierWatcher";
    // Register our own host with the watcher we just became.
    RegisterStatusNotifierHost(
        QStringLiteral("org.kde.StatusNotifierHost-holonight-%1")  // NOLINT(readability-identifier-naming)
            .arg(QCoreApplication::applicationPid()));

    // Set up service watcher to handle disappearing SNI services.
    service_watcher_ = new QDBusServiceWatcher(this);
    service_watcher_->setConnection(bus_);
    service_watcher_->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(service_watcher_, &QDBusServiceWatcher::serviceUnregistered, this, &TrayWatcher::onServiceUnregistered);
    discoverExistingItems();
    return;
  }

  qCInfo(lcTrayWatcher) << "watcher already owned (queued), falling back";
  fallbackReadItems();
}

void TrayWatcher::discoverExistingItems() {
  const QStringList services = registration_->registeredServices();
  qCInfo(lcTrayWatcher) << "scanning" << services.size() << "registered D-Bus services for existing tray items";

  for (const QString& service : services) {
    if (service.contains(QStringLiteral("StatusNotifierItem"), Qt::CaseInsensitive) &&
        !service.contains(QStringLiteral("Watcher"), Qt::CaseInsensitive) &&
        !service.contains(QStringLiteral("Host"), Qt::CaseInsensitive)) {
      qCInfo(lcTrayWatcher) << "discovered existing StatusNotifierItem service:" << service;
      addItem(service);
    }
  }
}

void TrayWatcher::fallbackReadItems() {
  qCInfo(lcTrayWatcher) << "reading tray items from existing StatusNotifierWatcher";
  // Subscribe to the existing watcher's item registered/unregistered signals.
  bus_.connect(kWatcherService, kWatcherPath, kWatcherIface, QStringLiteral("StatusNotifierItemRegistered"), this,
               SLOT(onItemRegisteredSignal(QString)));
  bus_.connect(kWatcherService, kWatcherPath, kWatcherIface, QStringLiteral("StatusNotifierItemUnregistered"), this,
               SLOT(onItemUnregisteredSignal(QString)));

  // Read existing items.
  auto msg = QDBusMessage::createMethodCall(kWatcherService, kWatcherPath, kPropsIface, QStringLiteral("Get"));
  msg << kWatcherIface << QStringLiteral("RegisteredStatusNotifierItems");
  QDBusPendingCall call = bus_.asyncCall(msg, 5000);
  auto* pending = new QDBusPendingCallWatcher(call, this);
  connect(pending, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* ptr) {
    ptr->deleteLater();
    QDBusPendingReply<QDBusVariant> reply = *ptr;
    if (reply.isError()) {
      qCWarning(lcTrayWatcher) << "fallback Get failed:" << reply.error().message();
      return;
    }
    QStringList items = reply.value().variant().toStringList();
    qCInfo(lcTrayWatcher) << "fallback watcher returned" << items.size() << "items" << items;
    for (const QString& raw : items) {
      addItem(raw);
    }
  });
}

// NOLINT(readability-identifier-naming) — PascalCase required by SNI D-Bus protocol
void TrayWatcher::RegisterStatusNotifierItem(const QString& service_or_path) {  // NOLINT(readability-identifier-naming)
  QString key;
  if (service_or_path.startsWith(QLatin1Char('/'))) {
    // Path-only registration: caller identifies itself by its unique bus name.
    QString sender = calledFromDBus() ? message().service() : QString{};
    if (sender.isEmpty()) {
      qCWarning(lcTrayWatcher) << "path-only registration with no sender:" << service_or_path;
      return;
    }
    key = sender + QLatin1Char(':') + service_or_path;
  } else {
    key = normaliseTrayItemKey(service_or_path);
  }
  if (registered_item_keys_.contains(key)) {
    return;
  }
  addItem(key);
  Q_EMIT StatusNotifierItemRegistered(key);  // NOLINT(readability-identifier-naming)
  qCInfo(lcTrayWatcher) << "RegisterStatusNotifierItem" << key;
}

// NOLINT(readability-identifier-naming)
void TrayWatcher::RegisterStatusNotifierHost(const QString& service) {  // NOLINT(readability-identifier-naming)
  host_registered_ = true;
  Q_EMIT StatusNotifierHostRegistered();  // NOLINT(readability-identifier-naming)
  qCInfo(lcTrayWatcher) << "RegisterStatusNotifierHost" << service;
}

void TrayWatcher::onServiceUnregistered(const QString& service) {
  qCInfo(lcTrayWatcher) << "tray service unregistered" << service;
  // Remove all items belonging to this service.
  QStringList to_remove;
  for (const QString& key : registered_items_) {
    if (splitTrayItemKey(key).service == service) {
      to_remove.append(key);
    }
  }
  for (const QString& key : to_remove) {
    removeItem(key);
  }
}

void TrayWatcher::onItemRegisteredSignal(const QString& service) {
  qCInfo(lcTrayWatcher) << "StatusNotifierItemRegistered signal" << service;
  addItem(service);
}

void TrayWatcher::onItemUnregisteredSignal(const QString& service) {
  qCInfo(lcTrayWatcher) << "StatusNotifierItemUnregistered signal" << service;
  removeItem(normaliseTrayItemKey(service));
}

void TrayWatcher::addItem(const QString& service_and_path) {
  QString key = normaliseTrayItemKey(service_and_path);
  const TrayItemAddress address = splitTrayItemKey(key);

  qCInfo(lcTrayWatcher) << "addItem requested" << service_and_path << "normalized" << key << "service"
                        << address.service << "path" << address.path;

  bool is_new_item = registerTrayItemKey(registered_items_, registered_item_keys_, service_item_counts_, key);

  if (is_new_item && service_watcher_ != nullptr) {
    service_watcher_->addWatchedService(address.service);
  }

  fetchItemProperties(key, address.service, address.path);
}

void TrayWatcher::removeItem(const QString& service_and_path) {
  QString key = normaliseTrayItemKey(service_and_path);
  const TrayItemAddress address = splitTrayItemKey(key);

  qCInfo(lcTrayWatcher) << "removeItem requested" << service_and_path << "normalized" << key << "service"
                        << address.service << "path" << address.path;

  if (auto* prop_watcher = property_watchers_.take(key); prop_watcher != nullptr) {
    bus_.disconnect(address.service, address.path, kPropsIface, QStringLiteral("PropertiesChanged"), prop_watcher,
                    SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
    prop_watcher->deleteLater();
  }
  if (auto* sig_watcher = signal_watchers_.take(key); sig_watcher != nullptr) {
    sig_watcher->deleteLater();
  }

  const bool removed = unregisterTrayItemKey(registered_items_, registered_item_keys_, service_item_counts_, key);
  if (removed && service_watcher_ != nullptr && !service_item_counts_.contains(address.service)) {
    service_watcher_->removeWatchedService(address.service);
  }

  model_->removeItem(key);
}

void TrayWatcher::subscribeItemSignals(const QString& key, const QString& service, const QString& path) {
  if (signal_watchers_.contains(key)) {
    return;
  }
  auto* watcher = new ItemSignalWatcher(key, service, path, this, this);

  static constexpr auto kSniIface_ = "org.kde.StatusNotifierItem";
  auto connectSignal = [&](const char* signal_name, const char* slot) {
    if (!bus_.connect(service, path, kSniIface_, QLatin1String(signal_name), watcher, slot)) {
      qCWarning(lcTrayWatcher) << "failed to connect" << signal_name << "for" << key;
    }
  };

  connectSignal("NewIcon", SLOT(onNewIcon()));
  connectSignal("NewAttentionIcon", SLOT(onNewAttentionIcon()));
  bus_.connect(service, path, kSniIface_, QStringLiteral("NewStatus"), watcher, SLOT(onNewStatus(QString)));
  connectSignal("NewTitle", SLOT(onNewTitle()));
  connectSignal("NewToolTip", SLOT(onNewToolTip()));

  signal_watchers_.insert(key, watcher);
  qCInfo(lcTrayWatcher) << "subscribed to direct SNI signals for" << key;
}

void TrayWatcher::fetchSingleProperty(const QString& key, const QString& service, const QString& path,
                                      const QString& prop_name) {
  qCInfo(lcTrayWatcher) << "fetching single property" << prop_name << "for" << key;
  auto msg = QDBusMessage::createMethodCall(service, path, kPropsIface, QStringLiteral("Get"));
  msg << kSniIface << prop_name;
  auto* pending = new QDBusPendingCallWatcher(bus_.asyncCall(msg, 5000), this);
  connect(pending, &QDBusPendingCallWatcher::finished, this, [this, key, prop_name](QDBusPendingCallWatcher* ptr) {
    ptr->deleteLater();
    if (!registered_item_keys_.contains(key)) {
      return;
    }
    QDBusPendingReply<QDBusVariant> reply = *ptr;
    if (reply.isError()) {
      if (isOptionalTrayPropertyMissing(prop_name, reply.error().name(), reply.error().message())) {
        qCInfo(lcTrayWatcher) << "optional" << prop_name << "missing for" << key;
      } else {
        qCWarning(lcTrayWatcher) << "Get" << prop_name << "failed for" << key << reply.error().message();
      }
      return;
    }
    updateItemProperties(key, {{prop_name, reply.value().variant()}});
  });
}

// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
void TrayWatcher::fetchToolTip(const QString& key, const QString& service, const QString& path) {
  qCInfo(lcTrayWatcher) << "fetching ToolTip for" << key;
  auto msg = QDBusMessage::createMethodCall(service, path, kPropsIface, QStringLiteral("Get"));
  msg << kSniIface << QStringLiteral("ToolTip");
  auto* pending = new QDBusPendingCallWatcher(bus_.asyncCall(msg, 5000), this);
  connect(pending, &QDBusPendingCallWatcher::finished, this, [this, key](QDBusPendingCallWatcher* ptr) {
    ptr->deleteLater();
    if (!registered_item_keys_.contains(key)) {
      return;
    }
    QDBusPendingReply<QDBusVariant> reply = *ptr;
    if (reply.isError()) {
      if (isOptionalTrayPropertyMissing(QStringLiteral("ToolTip"), reply.error().name(), reply.error().message())) {
        qCInfo(lcTrayWatcher) << "optional ToolTip missing for" << key;
      } else {
        qCWarning(lcTrayWatcher) << "Get ToolTip failed for" << key << reply.error().message();
      }
      return;
    }
    updateItemProperties(key, {{QStringLiteral("ToolTip"), reply.value().variant()}});
  });
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

void TrayWatcher::onItemDirectSignal(const QString& key, const QString& prop_name, const QVariant& value) {
  qCInfo(lcTrayWatcher) << "direct signal" << prop_name << "for" << key << value;
  updateItemProperties(key, {{prop_name, value}});
}

void TrayWatcher::onItemNeedsPropertyFetch(const QString& key, const QString& prop_name) {
  auto* watcher = signal_watchers_.value(key, nullptr);
  if (watcher == nullptr) {
    return;
  }
  fetchSingleProperty(key, watcher->service(), watcher->path(), prop_name);
}

void TrayWatcher::onItemNeedsToolTipFetch(const QString& key) {
  auto* watcher = signal_watchers_.value(key, nullptr);
  if (watcher == nullptr) {
    return;
  }
  fetchToolTip(key, watcher->service(), watcher->path());
}

void TrayWatcher::fetchItemProperties(const QString& key, const QString& service, const QString& path) {
  qCInfo(lcTrayWatcher) << "fetching SNI properties for" << key << "service" << service << "path" << path;
  auto msg = QDBusMessage::createMethodCall(service, path, kPropsIface, QStringLiteral("GetAll"));
  msg << kSniIface;
  QDBusPendingCall call = bus_.asyncCall(msg, 5000);
  auto* pending = new QDBusPendingCallWatcher(call, this);
  connect(pending, &QDBusPendingCallWatcher::finished, this, [this, key](QDBusPendingCallWatcher* ptr) {
    ptr->deleteLater();
    if (!registered_item_keys_.contains(key)) {
      return;
    }
    QDBusPendingReply<QVariantMap> reply = *ptr;
    if (reply.isError()) {
      qCWarning(lcTrayWatcher) << "GetAll failed for" << key << reply.error().message();
      return;
    }
    qCInfo(lcTrayWatcher) << "GetAll returned properties for" << key << sortedKeys(reply.value());
    updateItemProperties(key, reply.value());
  });

  // Per-item PropertiesChanged subscription — routes updates back via key.
  if (property_watchers_.contains(key)) {
    return;
  }
  auto* prop_watcher = new ItemPropWatcher(key, this, this);
  if (!bus_.connect(service, path, kPropsIface, QStringLiteral("PropertiesChanged"), prop_watcher,
                    SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)))) {
    qCWarning(lcTrayWatcher) << "failed to watch item properties for" << key;
    prop_watcher->deleteLater();
    return;
  }
  property_watchers_.insert(key, prop_watcher);

  // Also subscribe to direct SNI signals (NewIcon, NewStatus, etc.).
  subscribeItemSignals(key, service, path);
  // Fetch initial ToolTip as part of item registration.
  fetchToolTip(key, service, path);
}

void TrayWatcher::updateItemProperties(const QString& key, const QVariantMap& changed) {
  qCInfo(lcTrayWatcher) << "updating item properties for" << key << "changed keys" << sortedKeys(changed);
  const TrayItemAddress address = splitTrayItemKey(key);
  TrayItem item = mergeTrayItemProperties(model_->itemForKey(key), address.service, address.path, changed);
  model_->addItem(item, key);
  qCInfo(lcTrayWatcher) << "updated properties for" << key;
}

void TrayWatcher::onItemPropertiesChanged(const QString& key, const QVariantMap& changed) {
  updateItemProperties(key, changed);
}

#include "TrayWatcher.moc"

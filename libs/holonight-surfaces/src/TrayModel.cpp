#include "TrayModel.h"

#include "TrayMenuSurface.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QIcon>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcTrayModel, "holonight.tray.model")

namespace {

bool overrideMatches(const TrayIconOverrideConfig& override, const TrayItem& item) {
  if (!override.id.isEmpty()) {
    return override.id == item.id;
  }
  if (!override.service.isEmpty() && override.service != item.service) {
    return false;
  }
  if (!override.object_path.isEmpty() && override.object_path != item.object_path) {
    return false;
  }
  if (!override.title.isEmpty() && override.title != item.title && override.title != item.tooltip.title) {
    return false;
  }
  return !override.service.isEmpty() || !override.object_path.isEmpty() || !override.title.isEmpty();
}

const TrayIconOverrideConfig* matchingOverride(const TrayIconOverridesConfig& overrides, const TrayItem& item) {
  for (const TrayIconOverrideConfig& override : overrides.items) {
    if (overrideMatches(override, item)) {
      return &override;
    }
  }
  return nullptr;
}

bool themeIconExists(const QString& icon_name) { return !icon_name.isEmpty() && QIcon::hasThemeIcon(icon_name); }

}  // namespace

TrayModel::TrayModel(ConfigService* config, QObject* parent)
    : QAbstractListModel(parent),
      max_visible_(config != nullptr ? config->barSystemTray().max_items : BarSystemTrayConfig{}.max_items),
      tray_icon_overrides_(config != nullptr ? config->trayIconOverrides() : TrayIconOverridesConfig{}) {
  if (config != nullptr) {
    connect(config, &ConfigService::barSystemTrayChanged, this, [this, config]() {
      const int updated = config->barSystemTray().max_items;
      if (max_visible_ != updated) {
        max_visible_ = updated;
        emit maxVisibleChanged();
      }
    });
    connect(config, &ConfigService::trayIconOverridesChanged, this,
            [this, config]() { updateTrayIconOverrides(config->trayIconOverrides()); });
  }
}

int TrayModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(rows_.size());
}

QVariant TrayModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
    return {};
  }
  const auto& [key, item] = rows_.at(index.row());

  switch (static_cast<Roles>(role)) {
    case ServiceRole:
      return item.service;
    case ObjectPathRole:
      return item.object_path;
    case IconNameRole:
      return resolvedIconName(item);
    case AttentionIconNameRole:
      return resolvedIconName(item, true);
    case StatusRole:
      return item.status;
    case IconPixmapUrlRole:
      return hasValidIconOverride(item) || item.icon_pixmap_cache.isNull()
                 ? QString{}
                 : QStringLiteral("image://tray/%1?v=%2").arg(key).arg(revision_);
    case AttentionPixmapUrlRole:
      return hasValidIconOverride(item) || hasValidIconOverride(item, true) || item.attention_pixmap_cache.isNull()
                 ? QString{}
                 : QStringLiteral("image://tray/attn/%1?v=%2").arg(key).arg(revision_);
    case TitleRole:
      return item.title;
    case ItemKeyRole:
      return key;
    case TooltipTitleRole:
      return item.tooltip.title;
    case TooltipDescriptionRole:
      return item.tooltip.description;
    case TooltipIconNameRole:
      return item.tooltip.icon_name;
    case HasUnreadRole:
      return item.has_unread;
  }
  return {};
}

QHash<int, QByteArray> TrayModel::roleNames() const {
  static const QHash<int, QByteArray> kRoles{
      {ServiceRole, "service"},
      {ObjectPathRole, "objectPath"},
      {IconNameRole, "iconName"},
      {AttentionIconNameRole, "attentionIconName"},
      {StatusRole, "status"},
      {IconPixmapUrlRole, "iconPixmapUrl"},
      {AttentionPixmapUrlRole, "attentionPixmapUrl"},
      {TitleRole, "title"},
      {ItemKeyRole, "itemKey"},
      {TooltipTitleRole, "tooltipTitle"},
      {TooltipDescriptionRole, "tooltipDescription"},
      {TooltipIconNameRole, "tooltipIconName"},
      {HasUnreadRole, "hasUnread"},
  };
  return kRoles;
}

void TrayModel::updateTrayIconOverrides(const TrayIconOverridesConfig& overrides) {
  if (tray_icon_overrides_ == overrides) {
    return;
  }
  tray_icon_overrides_ = overrides;
  if (rows_.isEmpty()) {
    return;
  }
  ++revision_;
  Q_EMIT dataChanged(index(0), index(static_cast<int>(rows_.size()) - 1));
  Q_EMIT revisionChanged();
}

QString TrayModel::resolvedIconName(const TrayItem& item, bool attention) const {
  const TrayIconOverrideConfig* override = matchingOverride(tray_icon_overrides_, item);
  if (override == nullptr) {
    return attention ? item.attention_icon_name : item.icon_name;
  }

  if (attention && themeIconExists(override->attention_icon)) {
    return override->attention_icon;
  }
  if (themeIconExists(override->icon)) {
    return attention ? QString{} : override->icon;
  }

  qCWarning(lcTrayModel) << "tray icon override" << override->name << "for" << item.service << item.object_path
                         << "matched but icon does not resolve:" << override->icon;
  return attention ? item.attention_icon_name : item.icon_name;
}

bool TrayModel::hasValidIconOverride(const TrayItem& item, bool attention) const {
  const TrayIconOverrideConfig* override = matchingOverride(tray_icon_overrides_, item);
  if (override == nullptr) {
    return false;
  }
  return attention ? themeIconExists(override->attention_icon) : themeIconExists(override->icon);
}

void TrayModel::addItem(const TrayItem& item, const QString& key) {
  {
    QReadLocker locker(&image_state_lock_);
    if (!index_by_key_.contains(key)) {
      locker.unlock();
    } else {
      locker.unlock();
      updateItem(key, item);
      return;
    }
  }
  const int row = rowCount();
  beginInsertRows({}, row, row);
  {
    QWriteLocker locker(&image_state_lock_);
    rows_.append({key, item});
    index_by_key_.insert(key, row);
  }
  endInsertRows();
  ++revision_;
  Q_EMIT revisionChanged();
  qCInfo(lcTrayModel) << "added" << key << "service" << item.service << "path" << item.object_path << "id" << item.id
                      << "title" << item.title << "status" << item.status << "iconName" << item.icon_name
                      << "iconPixmap" << item.icon_pixmap_cache.size() << "attentionIconName"
                      << item.attention_icon_name << "attentionPixmap" << item.attention_pixmap_cache.size()
                      << "hasUnread" << item.has_unread;
}

void TrayModel::removeItem(const QString& key) {
  int row = -1;
  {
    QReadLocker locker(&image_state_lock_);
    const auto pos = index_by_key_.constFind(key);
    if (pos == index_by_key_.constEnd()) {
      return;
    }
    row = pos.value();
  }
  beginRemoveRows({}, row, row);
  {
    QWriteLocker locker(&image_state_lock_);
    const auto pos = index_by_key_.find(key);
    if (pos == index_by_key_.end()) {
      return;
    }
    rows_.remove(row);
    index_by_key_.erase(pos);
    for (int& idx : index_by_key_) {
      if (idx > row) {
        --idx;
      }
    }
  }
  endRemoveRows();
  ++revision_;
  Q_EMIT revisionChanged();
  qCInfo(lcTrayModel) << "removed" << key;
}

void TrayModel::updateItem(const QString& key, const TrayItem& updated) {
  int row = -1;
  {
    QWriteLocker locker(&image_state_lock_);
    const auto pos = index_by_key_.find(key);
    if (pos == index_by_key_.end()) {
      return;
    }
    row = pos.value();
    rows_.replace(row, {rows_.at(row).first, updated});
  }
  QModelIndex idx = index(row);
  ++revision_;
  Q_EMIT dataChanged(idx, idx);
  Q_EMIT revisionChanged();
  qCInfo(lcTrayModel) << "updated" << key << "service" << updated.service << "path" << updated.object_path << "id"
                      << updated.id << "title" << updated.title << "status" << updated.status << "iconName"
                      << updated.icon_name << "iconPixmap" << updated.icon_pixmap_cache.size() << "attentionIconName"
                      << updated.attention_icon_name << "attentionPixmap" << updated.attention_pixmap_cache.size()
                      << "hasUnread" << updated.has_unread;
}

const TrayItem* TrayModel::itemForKey(const QString& key) const {
  auto pos = index_by_key_.constFind(key);
  if (pos == index_by_key_.constEnd()) {
    return nullptr;
  }
  return &rows_.at(pos.value()).second;
}

QImage TrayModel::imageForKey(const QString& key, bool attention) const {
  QImage image;
  {
    QReadLocker locker(&image_state_lock_);
    const auto pos = index_by_key_.constFind(key);
    if (pos == index_by_key_.constEnd()) {
      qCWarning(lcTrayModel) << "image requested for unknown tray key" << key << "attention" << attention;
      return {};
    }
    const TrayItem& item = rows_.at(pos.value()).second;
    image = attention ? item.attention_pixmap_cache : item.icon_pixmap_cache;
  }
  qCInfo(lcTrayModel) << "imageForKey" << key << "attention" << attention << "size" << image.size() << "isNull"
                      << image.isNull();
  return image;
}

void TrayModel::activate(const QString& key, int screen_x, int screen_y) {
  auto pos = index_by_key_.find(key);
  if (pos == index_by_key_.end()) {
    return;
  }
  const TrayItem& item = rows_.at(pos.value()).second;
  auto msg = QDBusMessage::createMethodCall(item.service, item.object_path,
                                            QStringLiteral("org.kde.StatusNotifierItem"), QStringLiteral("Activate"));
  msg << screen_x << screen_y;
  QDBusConnection::sessionBus().asyncCall(msg, 5000);
  qCInfo(lcTrayModel) << "activate" << key << screen_x << screen_y;
}

void TrayModel::secondaryActivate(const QString& key, int screen_x, int screen_y) {
  auto pos = index_by_key_.find(key);
  if (pos == index_by_key_.end()) {
    return;
  }
  const TrayItem& item = rows_.at(pos.value()).second;
  auto msg =
      QDBusMessage::createMethodCall(item.service, item.object_path, QStringLiteral("org.kde.StatusNotifierItem"),
                                     QStringLiteral("SecondaryActivate"));
  msg << screen_x << screen_y;
  QDBusConnection::sessionBus().asyncCall(msg, 5000);
  qCInfo(lcTrayModel) << "secondaryActivate" << key << screen_x << screen_y;
}

void TrayModel::scroll(const QString& key, int delta, const QString& orientation) {
  auto pos = index_by_key_.find(key);
  if (pos == index_by_key_.end()) {
    return;
  }
  const TrayItem& item = rows_.at(pos.value()).second;
  auto msg = QDBusMessage::createMethodCall(item.service, item.object_path,
                                            QStringLiteral("org.kde.StatusNotifierItem"), QStringLiteral("Scroll"));
  msg << delta << orientation;
  QDBusConnection::sessionBus().asyncCall(msg, 5000);
  qCInfo(lcTrayModel) << "scroll" << key << delta << orientation;
}

void TrayModel::openContextMenu(const QString& key, int screen_x, int screen_y) {
  auto pos = index_by_key_.find(key);
  if (pos == index_by_key_.end()) {
    return;
  }
  qCInfo(lcTrayModel) << "openContextMenu" << key << screen_x << screen_y;
  const TrayItem& item = rows_.at(pos.value()).second;
  fetchMenuPath(key, item, screen_x, screen_y);
}

void TrayModel::setMenuSurface(TrayMenuSurface* surface) { menu_surface_ = surface; }

void TrayModel::showContextMenu(DbusMenuModel* model, int screen_x, int screen_y) {
  if (menu_surface_ != nullptr) {
    menu_surface_->show(QString{}, screen_x, screen_y, model, menu_client_);
  }
}

static void callContextMenuFallback(const QString& service, const QString& path, int screen_x, int screen_y) {
  auto msg = QDBusMessage::createMethodCall(service, path, QStringLiteral("org.kde.StatusNotifierItem"),
                                            QStringLiteral("ContextMenu"));
  msg << screen_x << screen_y;
  QDBusConnection::sessionBus().asyncCall(msg, 2000);
}

void TrayModel::fetchMenuPath(const QString& key, const TrayItem& item, int screen_x, int screen_y) {
  static constexpr auto kPropsIface = "org.freedesktop.DBus.Properties";
  static constexpr auto kSniIface = "org.kde.StatusNotifierItem";

  auto msg =
      QDBusMessage::createMethodCall(item.service, item.object_path, QLatin1String(kPropsIface), QStringLiteral("Get"));
  msg << QLatin1String(kSniIface) << QStringLiteral("Menu");
  auto* pending = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg, 5000), this);
  connect(pending, &QDBusPendingCallWatcher::finished, this,
          [this, key, service = item.service, object_path = item.object_path, screen_x,
           screen_y](QDBusPendingCallWatcher* ptr) {
            ptr->deleteLater();
            QDBusPendingReply<QDBusVariant> reply = *ptr;
            if (reply.isError()) {
              qCInfo(lcTrayModel) << "Get Menu failed for" << key << "— ContextMenu fallback";
              callContextMenuFallback(service, object_path, screen_x, screen_y);
              return;
            }

            QVariant var = reply.value().variant();
            QString menu_path =
                var.canConvert<QDBusObjectPath>() ? var.value<QDBusObjectPath>().path() : var.toString();

            if (menu_path.isEmpty() || menu_path == QLatin1String("/")) {
              qCInfo(lcTrayModel) << "No Menu for" << key << "— ContextMenu fallback";
              callContextMenuFallback(service, object_path, screen_x, screen_y);
              return;
            }

            qCInfo(lcTrayModel) << "opening DBusMenu for" << key << menu_path;
            if (menu_surface_ != nullptr) {
              menu_surface_->hide();
            }
            delete menu_client_;
            menu_client_ = new DbusMenuClient(this);
            connect(menu_client_, &DbusMenuClient::menuReady, this, &TrayModel::showContextMenu);
            if (menu_surface_ != nullptr) {
              connect(menu_client_, &DbusMenuClient::menuClosed, menu_surface_, &TrayMenuSurface::hide);
            }
            connect(menu_client_, &DbusMenuClient::menuFailed, this,
                    []() { qCWarning(lcTrayModel) << "DBusMenu fetch failed"; });
            menu_client_->open(service, menu_path, screen_x, screen_y);
          });
}

// ---------------------------------------------------------------------------

TrayImageProvider::TrayImageProvider(TrayModel* model)
    : QQuickImageProvider(QQuickImageProvider::Image), model_(model) {}

QImage TrayImageProvider::requestImage(const QString& image_id, QSize* size, const QSize& /*requested_size*/) {
  // image_id format: "<key>" (regular) or "attn/<key>" (attention icon)
  bool attention = image_id.startsWith(QLatin1String("attn/"));
  QString key = attention ? image_id.sliced(5) : image_id;
  // Strip optional ?v=<n> cache-busting suffix.
  qsizetype sep_pos = key.indexOf(QLatin1Char('?'));
  if (sep_pos != -1) {
    key = key.left(sep_pos);
  }

  QImage img = model_->imageForKey(key, attention);
  if (size != nullptr) {
    *size = img.size();
  }
  qCInfo(lcTrayModel) << "TrayImageProvider request" << image_id << "resolved key" << key << "attention" << attention
                      << "size" << img.size() << "isNull" << img.isNull();
  return img;
}

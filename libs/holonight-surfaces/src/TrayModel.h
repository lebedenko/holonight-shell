#pragma once

#include "ConfigService.h"
#include "DbusMenuClient.h"
#include "TrayItem.h"

class TrayMenuSurface;

#include <QAbstractListModel>
#include <QHash>
#include <QImage>
#include <QList>
#include <QPair>
#include <QQuickImageProvider>
#include <QReadWriteLock>
#include <QString>
#include <QtQml/qqml.h>

class TrayModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
  Q_PROPERTY(int maxVisible READ maxVisible NOTIFY maxVisibleChanged)

 public:
  enum Roles : uint16_t {  // NOLINT(cppcoreguidelines-use-enum-class): Qt model roles are int-compatible.
    ServiceRole = Qt::UserRole + 1,
    ObjectPathRole,
    IconNameRole,
    AttentionIconNameRole,
    StatusRole,
    IconPixmapUrlRole,
    AttentionPixmapUrlRole,
    TitleRole,
    ItemKeyRole,
    TooltipTitleRole,
    TooltipDescriptionRole,
    TooltipIconNameRole,
    HasUnreadRole,
  };
  Q_ENUM(Roles)

  explicit TrayModel(ConfigService* config = nullptr, QObject* parent = nullptr);

  [[nodiscard]] int revision() const { return revision_; }
  [[nodiscard]] int maxVisible() const { return max_visible_; }

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  void addItem(const TrayItem& item, const QString& key);
  void removeItem(const QString& key);
  void updateItem(const QString& key, const TrayItem& updated);

  void setMenuSurface(TrayMenuSurface* surface);
  void updateTrayIconOverrides(const HoloNight::ShellConfig::TrayIconOverridesConfig& overrides);

  [[nodiscard]] const TrayItem* itemForKey(const QString& key) const;
  [[nodiscard]] QImage imageForKey(const QString& key, bool attention = false) const;

  Q_INVOKABLE void activate(const QString& key, int screen_x, int screen_y);
  Q_INVOKABLE void secondaryActivate(const QString& key, int screen_x, int screen_y);
  Q_INVOKABLE void scroll(const QString& key, int delta, const QString& orientation);
  Q_INVOKABLE void openContextMenu(const QString& key, int screen_x, int screen_y);

 Q_SIGNALS:
  void revisionChanged();
  void maxVisibleChanged();

 private:
  void fetchMenuPath(const QString& key, const TrayItem& item, int screen_x, int screen_y);
  void showContextMenu(DbusMenuModel* model, int screen_x, int screen_y);
  [[nodiscard]] QString resolvedIconName(const TrayItem& item, bool attention = false) const;
  [[nodiscard]] bool hasValidIconOverride(const TrayItem& item, bool attention = false) const;

  QList<QPair<QString, TrayItem>> rows_;
  QHash<QString, int> index_by_key_;
  mutable QReadWriteLock image_state_lock_;
  int revision_{0};
  int max_visible_{3};
  HoloNight::ShellConfig::TrayIconOverridesConfig tray_icon_overrides_;
  DbusMenuClient* menu_client_{nullptr};
  TrayMenuSurface* menu_surface_{nullptr};
};

class TrayImageProvider : public QQuickImageProvider {
 public:
  explicit TrayImageProvider(TrayModel* model);

  QImage requestImage(const QString& image_id, QSize* size, const QSize& requested_size) override;

 private:
  TrayModel* model_;
};

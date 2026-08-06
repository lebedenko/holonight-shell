#pragma once

#include "DbusMenuItem.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QList>
#include <QObject>
#include <QString>

class DbusMenuClient : public QObject {
  Q_OBJECT

 public:
  explicit DbusMenuClient(QObject* parent = nullptr);

  void open(const QString& service, const QString& object_path, int screen_x, int screen_y);
  void close();

  Q_INVOKABLE void activateItem(int item_id);

  // Assemble a DbusMenuItem from already-extracted components (testable without D-Bus).
  static DbusMenuItem makeItem(int item_id, const QVariantMap& props, QList<DbusMenuItem> children = {});
  // Parse from a live D-Bus argument (delegates to makeItem after extraction).
  static DbusMenuItem parseItem(const QDBusArgument& arg, int depth = 0);
  static constexpr int kMaxDepth = 5;

 Q_SIGNALS:
  void menuReady(DbusMenuModel* model, int screen_x, int screen_y);
  void menuFailed();
  void menuClosed();

 private:
  void doGetLayout();
  void doAboutToShow(const QList<int>& root_ids, DbusMenuModel* model, int screen_x, int screen_y);
  static constexpr int kAboutToShowTimeoutMs = 1000;

  QDBusConnection bus_;
  QString service_;
  QString path_;
  int pending_x_{0};
  int pending_y_{0};
  bool menu_open_{false};
};

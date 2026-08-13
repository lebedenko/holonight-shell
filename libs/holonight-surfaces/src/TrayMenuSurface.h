#pragma once

#include "DbusMenuClient.h"
#include "DbusMenuItem.h"
#include "PairedTransientSurfaceHost.h"
#include "TrayMenuSurfacePolicy.h"

#include <QQmlEngine>
#include <QRect>

class QScreen;

class TrayMenuSurface : public PairedTransientSurfaceHost {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool menuVisible READ isMenuVisible NOTIFY menuVisibleChanged)
  Q_PROPERTY(int paddingLeft READ paddingLeft NOTIFY geometryChanged)
  Q_PROPERTY(int paddingRight READ paddingRight NOTIFY geometryChanged)
  Q_PROPERTY(int paddingTop READ paddingTop NOTIFY geometryChanged)
  Q_PROPERTY(int paddingBottom READ paddingBottom NOTIFY geometryChanged)
  Q_PROPERTY(int columnCount READ columnCount NOTIFY geometryChanged)
  Q_PROPERTY(int columnIndex READ columnIndex NOTIFY geometryChanged)
  Q_PROPERTY(int maxPanelHeight READ maxPanelHeight NOTIFY geometryChanged)

 public:
  explicit TrayMenuSurface(QObject* parent = nullptr);
  ~TrayMenuSurface() override;
  TrayMenuSurface(const TrayMenuSurface&) = delete;
  TrayMenuSurface& operator=(const TrayMenuSurface&) = delete;
  TrayMenuSurface(TrayMenuSurface&&) = delete;
  TrayMenuSurface& operator=(TrayMenuSurface&&) = delete;

  [[nodiscard]] bool isMenuVisible() const { return menu_visible_; }
  [[nodiscard]] int paddingLeft() const { return padding_left_; }
  [[nodiscard]] int paddingRight() const { return padding_right_; }
  [[nodiscard]] int paddingTop() const { return padding_top_; }
  [[nodiscard]] int paddingBottom() const { return padding_bottom_; }
  [[nodiscard]] int columnCount() const { return column_count_; }
  [[nodiscard]] int columnIndex() const { return column_index_; }
  [[nodiscard]] int maxPanelHeight() const { return max_panel_height_; }

  [[nodiscard]] PairedLayerSurfaceSpec surfaceSpec(QScreen* screen, int screen_x, int screen_y, DbusMenuModel* model);
  Q_INVOKABLE void show(const QString& screen_name, int screen_x, int screen_y, DbusMenuModel* model,
                        DbusMenuClient* client);
  Q_INVOKABLE void hide();
  Q_INVOKABLE void updateActiveGeometry(int count, int panel_height);
  Q_INVOKABLE void setSubmenuModel(DbusMenuModel* submenu_model);
  Q_INVOKABLE void activateItem(int item_id);
  Q_INVOKABLE void close();

 Q_SIGNALS:
  void menuVisibleChanged();
  void geometryChanged();

 protected:
  void onPairOpened() override;
  void onPairTerminated() override;

 private:
  [[nodiscard]] bool ensureSurface(const QString& screen_name, int screen_x, int screen_y, DbusMenuModel* model);
  void destroySurface();
  void applyGeometry(const TrayMenuActiveGeometry& geometry);
  void setMenuVisible(bool visible);

  DbusMenuClient* active_client_{nullptr};
  DbusMenuModel* active_model_{nullptr};
  TrayMenuPlacement current_placement_;
  QRect current_screen_geometry_;
  bool menu_visible_{false};
  int padding_left_{0};
  int padding_right_{0};
  int padding_top_{0};
  int padding_bottom_{0};
  int column_count_{1};
  int column_index_{0};
  int max_panel_height_{480};
};

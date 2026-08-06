#pragma once

#include "DbusMenuClient.h"
#include "DbusMenuItem.h"
#include "LayerShell.h"
#include "LayerSurface.h"

#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QQuickView>
#include <QRect>
#include <QString>

struct wl_surface;

class TrayMenuSurface : public QObject {
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
  struct MenuPlacement {
    int x{0};
    int y{0};
    int width{244};
    int height{40};
    int column_count{1};
    int top_level_column{0};
    int column_step{1};
  };

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

  // The model is owned by the surface while displayed; the client remains owned by TrayModel.
  Q_INVOKABLE void show(const QString& screen_name, int screen_x, int screen_y, DbusMenuModel* model,
                        DbusMenuClient* client);
  Q_INVOKABLE void hide();
  Q_INVOKABLE void updateActiveGeometry(int count, int panel_height);
  Q_INVOKABLE void setSubmenuModel(DbusMenuModel* submenu_model);

  // Called from QML via menuClient context: forwards to the active DbusMenuClient.
  Q_INVOKABLE void activateItem(int item_id);
  Q_INVOKABLE void close();

 Q_SIGNALS:
  void menuVisibleChanged();
  void geometryChanged();

 private:
  [[nodiscard]] bool ensureSurface(const QString& screen_name, int screen_x, int screen_y, DbusMenuModel* model);
  void destroySurface();
  void setMenuVisible(bool visible);

  LayerShell shell_;
  QQuickView* dismiss_view_{nullptr};
  LayerSurface* dismiss_surface_{nullptr};
  QQuickView* view_{nullptr};
  LayerSurface* surface_{nullptr};
  DbusMenuClient* active_client_{nullptr};
  DbusMenuModel* active_model_{nullptr};
  bool menu_visible_{false};
  bool pending_show_{false};
  QString pending_screen_;
  int pending_x_{0};
  int pending_y_{0};
  DbusMenuModel* pending_model_{nullptr};
  DbusMenuClient* pending_client_{nullptr};

  wl_surface* wl_surface_{nullptr};
  MenuPlacement current_placement_;
  QRect current_screen_geometry_;

  int padding_left_{0};
  int padding_right_{0};
  int padding_top_{0};
  int padding_bottom_{0};
  int column_count_{1};
  int column_index_{0};
  int max_panel_height_{480};
};

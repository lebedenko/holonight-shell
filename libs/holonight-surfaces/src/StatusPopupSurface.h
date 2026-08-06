#pragma once

#include "LayerShell.h"
#include "LayerSurface.h"

#include <QObject>
#include <QQmlEngine>
#include <QQuickView>
#include <QSize>
#include <QString>

// Owns a single status popup (a layer-shell surface anchored below the bar) plus a
// fullscreen dismiss overlay that closes the popup on any outside click. Only one
// status popup is ever shown at a time, which is guaranteed by single-surface ownership.
class StatusPopupSurface : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool popupVisible READ isPopupVisible NOTIFY popupVisibleChanged)
  Q_PROPERTY(QString activePopupId READ activePopupId NOTIFY activePopupChanged)
  Q_PROPERTY(int pointerX READ pointerX NOTIFY geometryChanged)

 public:
  explicit StatusPopupSurface(QObject* parent = nullptr);
  ~StatusPopupSurface() override;

  StatusPopupSurface(const StatusPopupSurface&) = delete;
  StatusPopupSurface& operator=(const StatusPopupSurface&) = delete;
  StatusPopupSurface(StatusPopupSurface&&) = delete;
  StatusPopupSurface& operator=(StatusPopupSurface&&) = delete;

  [[nodiscard]] bool isPopupVisible() const { return popup_visible_; }
  [[nodiscard]] QString activePopupId() const { return active_popup_id_; }
  [[nodiscard]] int pointerX() const { return pointer_x_; }

  Q_INVOKABLE void toggle(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width);
  Q_INVOKABLE void show(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width);
  Q_INVOKABLE void hide();

 Q_SIGNALS:
  void popupVisibleChanged();
  void activePopupChanged();
  void geometryChanged();

 private:
  [[nodiscard]] bool ensureSurface(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width);
  void destroySurface();
  void setPopupVisible(bool visible);
  void setActivePopupId(const QString& popup_id);
  void setPointerX(int value);

  LayerShell shell_;
  QQuickView* view_ = nullptr;
  LayerSurface* surface_ = nullptr;
  QQuickView* dismiss_view_ = nullptr;
  LayerSurface* dismiss_surface_ = nullptr;
  QString active_popup_id_;
  int pointer_x_ = 0;
  bool popup_visible_ = false;
  bool pending_show_ = false;
  QString pending_popup_id_;
  QString pending_screen_;
  int pending_anchor_x_ = 0;
  int pending_anchor_width_ = 0;
};

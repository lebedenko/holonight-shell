#pragma once

#include "PairedTransientSurfaceHost.h"

#include <QQmlEngine>
#include <QString>

class QScreen;

class StatusPopupSurface : public PairedTransientSurfaceHost {
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
  [[nodiscard]] static PairedLayerSurfaceSpec surfaceSpec(QScreen* screen, const QString& popup_id, int anchor_x,
                                                          int anchor_width);

  Q_INVOKABLE void toggle(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width);
  Q_INVOKABLE void show(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width);
  Q_INVOKABLE void hide();

 Q_SIGNALS:
  void popupVisibleChanged();
  void activePopupChanged();
  void geometryChanged();

 protected:
  void onPairOpened() override;
  void onPairTerminated() override;

 private:
  [[nodiscard]] bool ensureSurface(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width);
  void destroySurface();
  void setPopupVisible(bool visible);
  void setActivePopupId(const QString& popup_id);
  void setPointerX(int value);

  QString active_popup_id_;
  int pointer_x_ = 0;
  bool popup_visible_ = false;
};

#pragma once

#include "PairedTransientSurfaceHost.h"
#include "StatusPopupGeometry.h"

#include <QQmlEngine>
#include <QString>

#include <optional>

class QScreen;

class StatusPopupSurface : public PairedTransientSurfaceHost {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool popupVisible READ isPopupVisible NOTIFY popupVisibleChanged)
  Q_PROPERTY(QString activePopupId READ activePopupId NOTIFY activePopupChanged)

 public:
  explicit StatusPopupSurface(QObject* parent = nullptr);
  ~StatusPopupSurface() override;
  StatusPopupSurface(const StatusPopupSurface&) = delete;
  StatusPopupSurface& operator=(const StatusPopupSurface&) = delete;
  StatusPopupSurface(StatusPopupSurface&&) = delete;
  StatusPopupSurface& operator=(StatusPopupSurface&&) = delete;

  [[nodiscard]] bool isPopupVisible() const { return popup_visible_; }
  [[nodiscard]] QString activePopupId() const { return active_popup_id_; }
  [[nodiscard]] static PairedLayerSurfaceSpec surfaceSpec(QScreen* screen, const QString& popup_id, int anchor_x,
                                                          int anchor_width);

  Q_INVOKABLE void toggle(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width);
  Q_INVOKABLE void show(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width);
  Q_INVOKABLE void hide();

 Q_SIGNALS:
  void popupVisibleChanged();
  void activePopupChanged();

 protected:
  void onPairOpened() override;
  void onPairConfigured() override;
  void onPairTerminated() override;

 private Q_SLOTS:
  void updateInputRegion();

 private:
  [[nodiscard]] bool ensureSurface(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width);
  void destroySurface();
  void updateSurfacePlacement();
  void setPopupVisible(bool visible);
  void setActivePopupId(const QString& popup_id);

  QPointer<QScreen> current_screen_;
  int anchor_local_x_ = 0;
  int anchor_width_ = 0;
  StatusPopupSurfacePlacement applied_placement_;
  std::optional<QRect> applied_input_rect_;
  QMetaObject::Connection screen_connection_;
  QMetaObject::Connection available_connection_;
  QString active_popup_id_;
  bool popup_visible_ = false;
};

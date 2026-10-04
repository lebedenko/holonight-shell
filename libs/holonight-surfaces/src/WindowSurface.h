#pragma once

#include "TransientSurfaceHost.h"

#include <QPointF>
#include <QRectF>
#include <QStringList>
#include <QtQml/qqml.h>

class WindowSurface final : public TransientSurfaceHost {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool visible READ visible NOTIFY changed)
  Q_PROPERTY(QString screenName READ screenName NOTIFY changed)
  Q_PROPERTY(QPointF menuPosition READ menuPosition NOTIFY changed)
  Q_PROPERTY(QRectF anchor READ anchor NOTIFY changed)
  Q_PROPERTY(bool besideAnchor READ besideAnchor NOTIFY changed)
  Q_PROPERTY(int mode READ mode NOTIFY changed)
  Q_PROPERTY(QString target READ target NOTIFY changed)
  Q_PROPERTY(QStringList choices READ choices NOTIFY changed)
 public:
  explicit WindowSurface(QObject* parent = nullptr) : TransientSurfaceHost("WindowSurface", parent) {}
  [[nodiscard]] bool visible() const { return hasSurface(); }
  [[nodiscard]] QString screenName() const { return screen_name_; }
  [[nodiscard]] QPointF menuPosition() const { return menu_position_; }
  [[nodiscard]] QRectF anchor() const { return anchor_; }
  [[nodiscard]] bool besideAnchor() const { return beside_anchor_; }
  [[nodiscard]] static QRectF localAnchor(const QRectF& global, const QRect& geometry);
  [[nodiscard]] int mode() const { return mode_; }
  [[nodiscard]] QString target() const { return target_; }
  [[nodiscard]] QStringList choices() const { return choices_; }
  Q_INVOKABLE void toggle(const QString& screen = {}, const QRectF& anchor = {});
  Q_INVOKABLE void menu(const QString& identifier, const QString& screen, const QRectF& anchor = {},
                        bool beside = false);
  Q_INVOKABLE void chooser(const QStringList& ids, const QString& screen, const QRectF& anchor = {});
  Q_INVOKABLE void desktopMenu(const QString& screen, qreal position_x = -1, qreal position_y = -1);
  Q_INVOKABLE void hide();
  [[nodiscard]] static Holonight::Wayland::LayerSurfaceSpec surfaceSpec(QScreen* screen, bool desktop_menu = false);
 Q_SIGNALS:
  void changed();
  void opened();
  void dismissed();

 private:
  void show(const QString& name, const QRectF& anchor = {});
  QRectF anchor_;
  bool beside_anchor_{false};
  void onSurfaceTerminated() override;
  QPointF menu_position_{-1, -1};
  int mode_{0};
  QString target_;
  QString screen_name_;
  QStringList choices_;
};

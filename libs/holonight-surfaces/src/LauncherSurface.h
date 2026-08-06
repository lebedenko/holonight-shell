#pragma once

#include "LauncherSurfaceLifecycle.h"
#include "LayerShell.h"
#include "LayerSurface.h"

#include <QObject>
#include <QQmlEngine>
#include <QQuickView>
#include <QString>

struct wl_surface;

class LauncherSurface : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool visible READ isVisible NOTIFY visibleChanged)

 public:
  explicit LauncherSurface(QObject* parent = nullptr);
  ~LauncherSurface() override;

  LauncherSurface(const LauncherSurface&) = delete;
  LauncherSurface& operator=(const LauncherSurface&) = delete;
  LauncherSurface(LauncherSurface&&) = delete;
  LauncherSurface& operator=(LauncherSurface&&) = delete;

  [[nodiscard]] bool isVisible() const { return visible_; }

  Q_INVOKABLE void toggle(const QString& screen_name = {});
  Q_INVOKABLE void show(const QString& screen_name = {});
  Q_INVOKABLE void hide();
  // Called by QML after the close animation completes.
  Q_INVOKABLE void notifyHideReady();

 Q_SIGNALS:
  void visibleChanged();

 private:
  bool ensureSurface(const QString& screen_name);
  void destroySurface();
  void executeCommand(const LauncherSurfaceCommand& command);
  void setVisible(bool visible);

  LayerShell shell_;
  QQuickView* view_{nullptr};
  LayerSurface* surface_{nullptr};
  wl_surface* wl_surface_{nullptr};
  LauncherSurfaceLifecycle lifecycle_;
  bool visible_{false};
};

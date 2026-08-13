#pragma once

#include "LauncherSurfaceLifecycle.h"
#include "TransientSurfaceHost.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>

class QScreen;

class LauncherSurface : public TransientSurfaceHost {
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

  [[nodiscard]] static Holonight::Wayland::LayerSurfaceSpec surfaceSpec(QScreen* screen);

 Q_SIGNALS:
  void visibleChanged();

 private:
  bool ensureSurface(const QString& screen_name);
  void destroySurface();
  void executeCommand(const LauncherSurfaceCommand& command);
  void setVisible(bool visible);

  void onSurfaceTerminated() override;

  LauncherSurfaceLifecycle lifecycle_;
  bool visible_{false};
};

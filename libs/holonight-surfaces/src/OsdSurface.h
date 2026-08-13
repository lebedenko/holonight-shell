#pragma once

#include "TransientSurfaceHost.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>

#include <holonight_shell_config/config_structs.h>

class QScreen;

// The OSD's layer-shell surface: one overlay-layer QQuickView following whichever monitor the
// caller names, hosting Osd/OsdView.qml. (The view is not named OsdSurface.qml: this class already
// exports that type name into the HolonightShell module, and a same-named QML file makes every
// import of the module ambiguous.)
//
// Deliberately a pure presentation shell (REQ-C-002/C-004): it knows nothing about OsdController,
// AudioService, or what a "channel" means. Content arrives through showLevel()/showSelection(),
// placement through setPosition()/ensureSurface(), and the decision of *which* monitor to use is
// made by ShellApplication (REQ-C-012), never here.
//
// Lifecycle mirrors NotificationToastSurface: create on demand, reuse while the monitor is
// unchanged, destroy and rebuild when it changes, and defer creation until the layer-shell global
// is bound. Unlike the toast stack the surface is sized entirely by its content (REQ-C-013) --
// there is no fixed width -- because the OSD's width varies with the keyboard layout name.
class OsdSurface : public TransientSurfaceHost {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

 public:
  explicit OsdSurface(QObject* parent = nullptr);
  ~OsdSurface() override;

  OsdSurface(const OsdSurface&) = delete;
  OsdSurface& operator=(const OsdSurface&) = delete;
  OsdSurface(OsdSurface&&) = delete;
  OsdSurface& operator=(OsdSurface&&) = delete;

  // Applied to a live surface immediately, so a config reload repositions the OSD without waiting
  // for the next event (REQ-C-005/C-006, REQ-C-011).
  void setPosition(HoloNight::ShellConfig::WidgetPosition position);

  // Creates (or keeps) the OSD surface on the given output. Deferred until the layer-shell global
  // is bound. Safe to call repeatedly for the same monitor (REQ-F-012).
  void ensureSurface(const QString& screen_name);

  void showLevel(const QString& channel, int value, bool muted);
  void showSelection(const QString& channel, const QString& short_label, const QString& full_label);

  // Starts the QML exit animation. The surface itself is torn down later, from
  // onHideAnimationFinished() -- not here -- so the fade-out actually gets to render.
  void hide();

  void destroySurface();

  [[nodiscard]] bool isActive() const { return hasSurface(); }
  [[nodiscard]] Holonight::Wayland::LayerSurfaceSpec surfaceSpec(QScreen* screen, const QString& screen_name) const;

  // Called by OsdView.qml once the exit animation completes, the same callback mechanism
  // SidebarManager uses for RightSidebar.qml's close animation. Queues the teardown rather than
  // performing it -- see destroyAfterHide().
  Q_INVOKABLE void onHideAnimationFinished();

 private Q_SLOTS:
  // The queued half of onHideAnimationFinished(): runs once the QML handler that requested the
  // teardown has returned, and only if no show has revived the OSD in the meantime.
  void destroyAfterHide();

  // REQ-C-013: resize the layer surface to whatever the QML root currently reports as its implicit
  // size, so the input-transparent region stays no larger than the visible OSD. Runs once per
  // rendered frame and no-ops unless the size moved.
  void updateSurfaceSize();

 private:
  bool createSurface(const QString& screen_name);
  void applyPlacement();
  void applyInputRegion();
  // Replays the last pushed content onto the QML root. Called after every content update and after
  // a rebuild, so migrating to another monitor never shows a frame of empty OSD.
  void pushPendingContent();

  void onSurfaceConfigured() override;
  void onSurfaceTerminated() override;

  QString current_screen_;
  // Last size handed to set_size, so the per-frame resample only issues a request when it changed.
  // -1 means "nothing applied yet", which no real size can match.
  int applied_width_ = -1;
  int applied_height_ = -1;
  bool pending_show_ = false;
  QString pending_screen_;
  HoloNight::ShellConfig::WidgetPosition position_{HoloNight::ShellConfig::WidgetPosition::CenterBottom};

  // Last content pushed, kept current whether or not a view exists at the time, so a rebuild has
  // something to replay immediately.
  bool pending_is_level_{true};
  QString pending_channel_;
  int pending_value_{0};
  bool pending_muted_{false};
  QString pending_short_label_;
  QString pending_full_label_;
};

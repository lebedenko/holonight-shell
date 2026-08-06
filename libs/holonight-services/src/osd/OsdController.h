#pragma once

#include "OsdChannelSource.h"
#include "OsdEvent.h"

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTimer>

#include <chrono>
#include <functional>
#include <vector>

// REQ-F-003/004/005/007/008. The single decision point between "a service's state changed" and
// "put something on screen".
//
// The shell does not own the media keys -- Hyprland does -- so every OSD trigger is an observation
// after the fact, not a keypress. That makes over-display, not under-display, the failure mode
// worth engineering against: the shell learns a value the moment it connects, on every reconnect,
// and on every unrelated re-broadcast. Three independent gates exist for that reason (prime, grace
// period, suppression), and they are all here rather than spread across the adapters so the whole
// policy is readable in one place -- see the ordered chain in onEventObserved().
//
// It knows nothing about AudioService, BrightnessService, or KeyboardLayoutService: sources arrive
// as OsdChannelSource* (REQ-F-002/025). It also never asks whether a surface is visible, because
// holonight_services may not depend on holonight_surfaces -- suppression is pushed in from
// ShellApplication through setSuppressed() (REQ-F-006).
class OsdController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

 public:
  using NowFn = std::function<std::chrono::steady_clock::time_point()>;

  // Sources are Qt-parented (normally to ShellApplication) and outlive the controller; raw
  // pointers per REQ-F-025 and this codebase's QObject conventions -- not shared_ptr. `now_fn`
  // exists so the grace period can be tested by advancing a value rather than sleeping; an empty
  // std::function is replaced with the real clock.
  explicit OsdController(
      std::vector<OsdChannelSource*> sources, NowFn now_fn = [] { return std::chrono::steady_clock::now(); },
      QObject* parent = nullptr);
  ~OsdController() override = default;

  OsdController(const OsdController&) = delete;
  OsdController& operator=(const OsdController&) = delete;
  OsdController(OsdController&&) = delete;
  OsdController& operator=(OsdController&&) = delete;

  Q_INVOKABLE void setSuppressed(const QString& channel, bool suppressed);   // REQ-F-005
  Q_INVOKABLE void setChannelEnabled(const QString& channel, bool enabled);  // REQ-F-007
  Q_INVOKABLE void setEnabled(bool enabled);                                 // REQ-C-010
  Q_INVOKABLE void setTimeoutMs(int timeout_ms);                             // REQ-C-011

#ifdef HOLONIGHT_TESTS
  [[nodiscard]] bool isHideTimerActiveForTest() const { return hide_timer_.isActive(); }
  [[nodiscard]] QString currentChannelForTest() const { return current_channel_; }
  [[nodiscard]] int timeoutMsForTest() const { return timeout_ms_; }
#endif

 Q_SIGNALS:
  void displayLevelEvent(const OsdLevelEvent& event);
  void displaySelectionEvent(const OsdSelectionEvent& event);
  void hideRequested();

 private Q_SLOTS:
  void onEventObserved(const OsdEvent& event);
  void onHideTimerTimeout();

 private:
  void onSourceAvailableChanged(const QString& channel, bool available);
  [[nodiscard]] bool inGracePeriod() const;
  [[nodiscard]] bool isChannelEnabled(const QString& channel) const;
  [[nodiscard]] bool isSuppressed(const QString& channel) const;
  void restartHideTimer();

  static constexpr auto kGracePeriod = std::chrono::milliseconds(2000);

  std::vector<OsdChannelSource*> sources_;  // non-owning; parented elsewhere
  NowFn now_fn_;
  std::chrono::steady_clock::time_point grace_start_;

  QHash<QString, OsdEvent> cache_;        // last observed value per channel -- always kept current
  QHash<QString, bool> suppressed_;       // REQ-F-005, written only by ShellApplication
  QHash<QString, bool> channel_enabled_;  // REQ-F-007, from config; absent means enabled
  bool master_enabled_{true};             // REQ-C-010, from config
  QString current_channel_;               // last channel actually displayed
  QTimer hide_timer_;                     // single-shot, restarted on every display (REQ-F-017/019)
  int timeout_ms_{1500};
};

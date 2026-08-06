# SDD Tasks — notification-daemon

## Dependency Arc Overview

Tasks are ordered to satisfy dependencies: foundation types → service extensions → core C++ model → D-Bus → CMake → QML components → surface orchestration → tests → verification.

---

## Task List

- [x] T-001: Create `NotificationTypes.h` with enums and data structures
  - REQs: REQ-F-009, REQ-F-012, REQ-F-013, REQ-C-048
  - Check: `src/services/notifications/NotificationTypes.h` compiles and defines `NotifUrgency`, `NotifCloseReason`, `NotifAccentKind`, `NotifLifecycle`, `NotifAction`, `NotificationData`; variable names all ≥3 chars

- [x] T-002: Implement free functions `accentForData()` and `effectiveTimeoutMs()`
  - REQs: REQ-F-035, REQ-F-036, REQ-F-037, REQ-F-014, REQ-F-015, REQ-F-016
  - Check: GTest case for `accentForData_critical` returns `NotifAccentKind::Critical`; GTest case for `effectiveTimeoutMs_negativeOneNormal` returns `default_timeout_ms`; GTest case for `effectiveTimeoutMs_negativeOneCritical` returns `-1`

- [x] T-003: Add `focusedMonitor` property and signal to `ActiveWindowService`
  - REQs: REQ-F-019
  - Check: `ActiveWindowService.h` declares `Q_PROPERTY(QString focusedMonitor READ focusedMonitor NOTIFY focusedMonitorChanged)` and `focusedMonitorChanged()` signal; property value matches `active_window_state_.focused_monitor_name` in both IPC event and `j/monitors` JSON parsing paths; signal emits when value changes

- [x] T-004: Add `[notifications]` config block parsing to `ConfigService`
  - REQs: REQ-F-039, REQ-F-040
  - Check: `ConfigService` has `NotificationsConfig` struct with `default_timeout_ms` (default 5000) and `max_visible` (default 3); `notificationsChanged()` signal emits when config file changes; missing keys use defaults; `task build` succeeds

- [x] T-005: Create `NotificationService` class with ID allocation and replace semantics
  - REQs: REQ-F-009, REQ-F-010, REQ-F-011, REQ-F-013
  - Check: GTest case `idAllocation_monotonic` verifies three successive `addOrReplace()` calls return increasing ids > 0; GTest case `replaceSemantics_existing` updates content in-place and returns same id; GTest case `replaceSemantics_unknown` allocates fresh id

- [x] T-006: Implement `NotificationService` per-monitor visible/queue model and place logic
  - REQs: REQ-F-030, REQ-F-031, REQ-F-032, REQ-F-034, REQ-F-020, REQ-F-021, REQ-F-022
  - Check: GTest case `queue_overflow_fifo` with `max_visible=2` and 5 notifications shows 2 visible; GTest case `critical_priority_jump` inserts critical into full visible set and bumps oldest non-critical to queue front; GTest case `perMonitorQueue_independent` shows separate queues for two monitors

- [x] T-007: Wire `NotificationService` timeout/timer logic and hover pause
  - REQs: REQ-F-014, REQ-F-015, REQ-F-016, REQ-F-017, REQ-F-018
  - Check: GTest case `timeout_explicit` sets timer to `expire_timeout` when > 0; GTest case `timeout_default` applies `default_timeout_ms` when expire_timeout=-1; GTest case `timeout_critical_never` does not set timer for critical urgency; mock timer test shows `hoverEntered()` pauses and `hoverLeft()` resumes

- [x] T-008: Implement `NotificationService` action and dismissal handlers
  - REQs: REQ-F-024, REQ-F-025, REQ-F-026, REQ-F-023
  - Check: `NotificationService` declares `invokeAction(id, action_key)` and `dismiss(id)` Q_INVOKABLEs; GTest case verifies `invokeAction` on resident notification does not close; GTest case verifies non-resident closes with reason 3; `hoverEntered(id)` and `hoverLeft(id)` Q_INVOKABLEs exist

- [x] T-009: Create `NotificationServer` D-Bus adapter with 4 methods and 2 signals
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-008, REQ-C-043
  - Check: `src/services/notifications/NotificationServer.h` declares `Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")`; implements `Notify`, `CloseNotification`, `GetCapabilities`, `GetServerInformation` Q_SLOTS; declares `NotificationClosed` and `ActionInvoked` Q_SIGNALS; `task build` succeeds

- [x] T-010: Wire `NotificationServer` to `NotificationService` and register D-Bus service
  - REQs: REQ-F-001
  - Check: `notificationd` binary registration test: `busctl introspect org.freedesktop.Notifications /org/freedesktop/Notifications` lists interface `org.freedesktop.Notifications` with 4 methods and 2 signals; service remains registered until daemon exits

- [x] T-011: Register `NotificationService` QML singleton and integrate into `ShellApplication`
  - REQs: REQ-C-044
  - Check: `ShellApplication` constructs `NotificationService`, `NotificationServer`, `NotificationManager`; `NotificationService` registered as QML singleton via `reg()` lambda; `task build` succeeds and no "unknown type NotificationService" QML errors at runtime

- [x] T-012: Add C++ sources to CMakeLists.txt (`holonight_services` and `holonight_surfaces`)
  - NOTE: services sources registered + verified now; `holonight_surfaces` entries (NotificationToastSurface, NotificationManager) added with their files in T-015/T-016 (cannot list non-existent sources).
  - REQs: REQ-C-044, REQ-C-047
  - Check: `CMakeLists.txt` lists `NotificationServer.h/.cpp`, `NotificationService.h/.cpp`, `NotificationTypes.h` in `holonight_services`; `NotificationToastSurface.h/.cpp`, `NotificationManager.h/.cpp` in `holonight_surfaces`; `task configure` and `task build` succeed

- [x] T-013: Create QML toast components: `ToastStack.qml`, `ToastItem.qml`, `ToastActionBar.qml`, `ToastActionButton.qml`
  - REQs: REQ-F-027, REQ-F-028, REQ-F-029, REQ-F-023, REQ-C-045, REQ-C-046
  - Check: All four files exist under `src/qml/Notifications/`; `ToastStack.qml` is added to `HOLONIGHT_QML_FILES`; all colors source from `HoloniightPalette` (no hex literals in grep); `MultiEffect` used for glow, not `Qt5Compat.GraphicalEffects.Glow`; `task qml-lint` reports no errors

- [x] T-014: Implement `ToastItem` body text markup stripping and elision
  - REQs: REQ-F-028, REQ-F-029
  - Check: `stripImgTags()` JS function in `ToastItem.qml` removes `<img ...>` tags; `body` Text element has `maximumLineCount: 3` and `elide: Text.ElideRight`; manual test with `<b>`, `<i>`, `<u>`, `<a>` renders styled text; manual test with `<img>` shows no pixmap

- [x] T-015: Create `NotificationToastSurface` layer-shell surface manager
  - REQs: REQ-F-020, REQ-F-021, REQ-F-022
  - Check: `src/surfaces/NotificationToastSurface.h/.cpp` exists; `ensureSurface(screen_name)` creates `QQuickView` with root `ToastStack.qml`; `isActive()` returns true while notifications exist; layer set to `layer_overlay`, anchored top-right, `exclusive_zone = 0`; `task build` succeeds

- [x] T-016: Create `NotificationManager` to orchestrate per-monitor surfaces
  - REQs: REQ-F-030, REQ-F-034
  - Check: `src/surfaces/NotificationManager.h/.cpp` exists; holds `QHash<QString, NotificationToastSurface*>` keyed by monitor; connects to `NotificationService::notificationAdded` → `ensureSurface()`; connects to `NotificationService::queueChanged` → destroys surface if both queue and visible empty; `task build` succeeds

- [x] T-017: Update CMakeLists.txt to register all QML files in `HOLONIGHT_QML_FILES`
  - REQs: REQ-C-044
  - Check: All `src/qml/Notifications/*.qml` files listed in `HOLONIGHT_QML_FILES`; `task configure` succeeds and reports no missing QML files; the discovery guard at line 369 does not fire

- [x] T-018: Write GTest unit tests for model logic: id allocation, replace, queue FIFO, critical priority
  - REQs: REQ-NF-041
  - Check: `tests/test_notification_service.cpp` contains at least 8 passing test cases: `idAllocation_monotonic`, `idAllocation_neverZero`, `replaceSemantics_existing`, `replaceSemantics_unknown`, `queueFifo_overflow`, `criticalPriorityJump`, `perMonitorQueue_independent`, `timeoutEffectiveMs_policies`; `task test` passes all notification tests

- [x] T-019: Write GTest unit tests for timeout-to-reason, category-to-accent, and hover pause
  - REQs: REQ-NF-041, REQ-F-035, REQ-F-036, REQ-F-037
  - Check: Test cases `accentForData_critical`, `accentForData_categoryIm`, `accentForData_categoryCall`, `accentForData_categoryPresence`, `accentForData_default`, `timeoutReason_expired`, `timeoutReason_dismissed`, `timeoutReason_closed`, mock timer pause/resume; `task test` passes all

- [x] T-020: Manual integration test: basic notification with `notify-send` and `dbus-monitor`
  - REQs: REQ-NF-042, REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005
  - Check: Run `notify-send "Title" "Body"` in live Wayland session; toast appears on-screen; `busctl introspect org.freedesktop.Notifications` shows correct interface; `dbus-monitor` shows `NotificationClosed(id, 2)` on dismiss or timeout

- [x] T-021: Manual test: notification actions, default action, and body click
  - REQs: REQ-NF-042, REQ-F-023, REQ-F-024, REQ-F-025
  - Check: `notify-send --action "reply:Reply" --action "ignore:Ignore"` shows two buttons; clicking button emits `ActionInvoked` via `dbus-monitor`; clicking body with `--action "default:Open"` invokes default; clicking body without default dismisses (reason 2)

- [x] T-022: Manual test: timeout behavior with explicit, default, and zero timeouts
  - REQs: REQ-NF-042, REQ-F-014, REQ-F-015, REQ-F-016, REQ-F-017
  - Check: `notify-send -t 2000` closes after ~2s; `notify-send -u normal` uses 5s default; `notify-send -t 0` never expires within 60s; `notify-send -u critical` never expires; hovering 5s toast at 2s mark prevents expiry for remaining 3s; all verified via manual observation and `dbus-monitor`

- [x] T-023: Manual test: multi-monitor placement and affinity
  - REQs: REQ-NF-042, REQ-F-020, REQ-F-021, REQ-F-022
  - Check: Dual-monitor setup; `notify-send` while focus on HDMI-1 places toast on HDMI-1; switch focus to DP-2, send new notification, appears on DP-2; old toast stays on HDMI-1 when focus changes; fallback to primary monitor if Hyprland IPC down

- [x] T-024: Manual test: queue overflow, FIFO promotion, and critical priority jump
  - REQs: REQ-NF-042, REQ-F-030, REQ-F-031, REQ-F-032, REQ-F-033
  - Check: Send 5 notifications with `max_visible=3` (from config); 3 toasts visible, 2 queued, no "+N more" indicator; close oldest toast, next queued appears; send critical while 3 normal visible, critical appears immediately, oldest normal goes to queue front (FIFO preserved)

- [x] T-025: Manual test: replace semantics and timer reset
  - REQs: REQ-NF-042, REQ-F-010, REQ-F-018
  - Check: `notify-send --replace-id=X ...` with unknown X creates new notification; with existing X updates content and resets timer (verify timer reset by observing toast at 4s mark of 5s timeout, replace it, observe new 5s expiry)

- [x] T-026: Manual test: markup rendering and accent color routing
  - REQs: REQ-NF-042, REQ-F-027, REQ-F-028, REQ-F-029, REQ-F-035, REQ-F-036, REQ-F-037
  - Check: `notify-send "Title" "Text with <b>bold</b> and <i>italic</i> and <u>underline</u>"` renders styled text; `notify-send -u critical` toast has red accent; `notify-send --hint category:im.received` has violet accent; `notify-send --hint category:mail.arrived` has cyan accent (default)

- [x] T-027: Manual test: config live-reload of timeouts and max_visible
  - REQs: REQ-NF-042, REQ-F-039, REQ-F-040
  - Check: Edit `$XDG_CONFIG_HOME/holonight/config.toml` `[notifications]` block to change `default_timeout_ms` to 3000 and `max_visible` to 5; save file; send new notification, verifies new timeout and max_visible apply; existing notifications unaffected

- [x] T-028: Run `task format-check`, `task tidy`, and `task qml-lint` on notification-daemon code
  - REQs: REQ-C-047, REQ-C-048
  - Check: `task format-check` reports no formatting issues in `src/services/notifications/`, `src/surfaces/notifications/`, `src/qml/Notifications/`; `task tidy` reports no clang-tidy violations (especially no `readability-identifier-length` or `readability-identifier-naming` for notification files); `task qml-lint` passes all QML files

- [x] T-029: Final build and integration verification
  - REQs: REQ-C-044, REQ-C-047
  - Check: `task build` succeeds without errors or warnings related to notifications; `task test` passes all notification tests; `task run` launches shell with notification daemon active; manual smoke test: `notify-send "Test" "Works"` displays and closes correctly

---

## Summary

**Total tasks:** 29

**REQ coverage (all 46 requirements):**
- REQ-F-001 through REQ-F-038: covered by T-001–T-027
- REQ-NF-041, REQ-NF-042: covered by T-018–T-027
- REQ-C-043 through REQ-C-048: covered by T-009, T-012, T-013, T-017, T-028, T-029

**Uncovered REQs:** None. All 46 requirements are satisfied by at least one task.

**Key dependencies satisfied:**
1. Foundation (T-001, T-002) → single-unit testable
2. Service extensions (T-003, T-004) → isolated small changes
3. Core model (T-005–T-008) → buildable, unit-testable
4. D-Bus layer (T-009, T-010) → wires to model
5. QML & CMake (T-011–T-017) → builds complete
6. Testing (T-018–T-027) → verify all paths
7. Quality (T-028, T-029) → lint, format, final check

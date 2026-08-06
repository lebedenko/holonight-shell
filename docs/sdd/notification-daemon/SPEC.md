# Notification Daemon (v1) — EARS Specification

## Overview

holonight-shell integrates a freedesktop-compliant notification daemon into the main binary. The daemon:
- Owns the `org.freedesktop.Notifications` D-Bus service (singleton at `/org/freedesktop/Notifications`)
- Models notifications as a state machine with lifecycle (active, expired, dismissed, closed)
- Renders toast popups in HoloNight visual style with interactive actions
- Queues overflowing notifications and prioritizes critical alerts

The implementation spans C++ service classes (`NotificationServer`, `NotificationService`) + QML toast surface, living in `src/services/notifications/`.

## Scope / Non-Goals

### v1 Scope
- D-Bus protocol (`Notify`, `CloseNotification`, `GetCapabilities`, `GetServerInformation`)
- In-memory notification model with id allocation, replace semantics, and lifecycle
- Toast popup surface with actions, timeout, hover-pause behavior
- Overflow queue with FIFO + critical prioritization
- Monitor-aware placement (born on focused monitor, stays there)
- Markup support for summary/body (`<b>`, `<i>`, `<u>`, `<a>`)
- Accent color routing (critical red, presence/call/IM violet, default cyan)
- Basic TOML config (`default_timeout_ms`, `max_visible`)
- GTest coverage of model logic; manual testing with `notify-send`

### Explicit Non-Goals (v2+)
- Notification center sidebar or persistent history
- Critical alert overlay (top-center/top-right corner)
- Top-bar indicator (badge, unread count)
- Do Not Disturb / quiet mode
- Notification grouping by app or category
- Persistence across shell restart
- Raw pixmap rendering (`image-data`/`image-path` hints)
- Audio playback for notifications
- Application-specific routing rules

---

## D-Bus Protocol

### REQ-F-001: D-Bus Service Registration
**Ubiquitous:** The system shall register a D-Bus service named `org.freedesktop.Notifications` at the object path `/org/freedesktop/Notifications`.

**Acceptance criterion:** `dbus-send` or `busctl` introspection lists `org.freedesktop.Notifications` with 4 methods and 2 signals; the service remains registered until the daemon exits.

---

### REQ-F-002: Notify Method Signature
**Ubiquitous:** The system shall implement the `Notify` method with signature `(sssuissh(ss)a{sv}i) → u` accepting: `app_name` (string), `replaces_id` (uint32), `app_icon` (string), `summary` (string), `body` (string), `actions` (array of string pairs), `hints` (dict string→variant), `expire_timeout` (int32); and returning a notification id (uint32).

**Acceptance criterion:** `dbus-send` invocation of `Notify` with valid arguments succeeds and returns a uint32 id > 0; malformed args produce a D-Bus error.

---

### REQ-F-003: CloseNotification Method
**Ubiquitous:** The system shall implement the `CloseNotification(uint32 id)` method that closes the notification with the given id.

**Acceptance criterion:** calling `CloseNotification` with an active notification id causes the notification to disappear from the UI and emits `NotificationClosed(id, 3)` signal; calling it with an unknown id produces no error and no signal.

---

### REQ-F-004: GetCapabilities Method
**Ubiquitous:** The system shall implement the `GetCapabilities()` method returning an array of strings: exactly `["body", "body-markup", "actions", "icon-static"]`.

**Acceptance criterion:** `dbus-call` to `GetCapabilities` returns the exact 4-element array; additional/missing strings cause test failure.

---

### REQ-F-005: GetServerInformation Method
**Ubiquitous:** The system shall implement the `GetServerInformation()` method returning a struct of (name: string, vendor: string, version: string, spec_version: string).

**Acceptance criterion:** `GetServerInformation` returns name "holonight-shell" (or matching binary name), a non-empty vendor string, a version string, and spec_version "1.2".

---

### REQ-F-006: D-Bus Interface Declaration
**Constraint:** The `NotificationServer` class implementing the D-Bus interface shall declare `Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")`.

**Acceptance criterion:** generated introspection XML lists the interface name as `org.freedesktop.Notifications` (not `local.NotificationServer`).

---

### REQ-F-007: NotificationClosed Signal
**Event-driven:** When a notification is closed (expired, dismissed, or explicitly closed), the system shall emit a `NotificationClosed(uint32 id, uint32 reason)` D-Bus signal.

**Acceptance criterion:** closing a notification via `CloseNotification`, UI dismissal, or timeout causes `dbus-monitor` to show `NotificationClosed` with the correct id and reason code (1 = expired, 2 = dismissed, 3 = closed by call).

---

### REQ-F-008: ActionInvoked Signal
**Event-driven:** When a toast action is invoked, the system shall emit an `ActionInvoked(uint32 id, string action_key)` D-Bus signal.

**Acceptance criterion:** clicking an action button in a toast causes `dbus-monitor` to show `ActionInvoked` with the notification id and the action key (e.g., "reply", "default").

---

## Notification Model & Lifecycle

### REQ-F-009: Monotonic ID Allocation
**Ubiquitous:** The system shall allocate notification ids as monotonically increasing uint32 values, never reusing ids and never assigning id 0.

**Acceptance criterion:** invoking `Notify` 3 times returns ids in strictly increasing order, all > 0, and differing by at least 1.

---

### REQ-F-010: Replace Semantics
**Event-driven:** When `Notify` is called with a `replaces_id` value > 0 that matches an active notification's id, the system shall update the existing notification's `summary`, `body`, `hints`, and `actions` in place; reset its expiry timer; and return the same id.

**Acceptance criterion:** calling `Notify` with `replaces_id=X` on an existing X updates its content (verified by querying model state); the returned id equals X; the toast's timeout counter restarts from `expire_timeout`.

---

### REQ-F-011: New Notification When replaces_id Unknown
**Event-driven:** When `Notify` is called with a `replaces_id` that does not match any active notification, the system shall treat it as a new notification request with the returned id being a freshly allocated one.

**Acceptance criterion:** calling `Notify` with `replaces_id=999` (nonexistent) on an empty notification list returns a new id ≠ 999; the notification is added to the active set.

---

### REQ-F-012: Notification Lifecycle States
**State-driven:** While a notification is active, it shall be in one of: `Visible` (rendered in toast), `Queued` (waiting to become visible), or `Expired` (timer reached zero). A notification shall transition to `Closed` upon: timeout expiry (reason 1), user dismissal (reason 2), `CloseNotification` call (reason 3), or action invocation without `resident` hint (reason 3).

**Acceptance criterion:** model state machine tracks each notification's state; GTest verifies all transitions; a `Closed` notification no longer appears in the visible/queued set.

---

### REQ-F-013: Notification Content Persistence
**Ubiquitous:** The system shall store and make available the `summary`, `body`, `hints` (as a variant map), and `actions` (as key-value pairs) for each active notification until it is closed.

**Acceptance criterion:** GTest queries a notification's `summary` after creation and `replace` operations; content matches the `Notify` arguments.

---

## Timeout Behavior

### REQ-F-014: Explicit Timeout (expire_timeout > 0)
**Event-driven:** When `Notify` is called with `expire_timeout` > 0, the system shall start a timer for that many milliseconds; when the timer expires, the notification shall close with reason code 1 (expired).

**Acceptance criterion:** `notify-send -t 2000` produces a toast that auto-closes after ~2s; `dbus-monitor` shows `NotificationClosed(..., 1)`.

---

### REQ-F-015: Default Timeout (expire_timeout = -1)
**Event-driven:** When `Notify` is called with `expire_timeout = -1`, the system shall apply the `default_timeout_ms` config value (default 5000) as the timeout for low/normal urgency notifications (urgency hint 0 or 1); critical notifications (urgency 2) shall never auto-expire.

**Acceptance criterion:** `notify-send -u normal` with no `-t` uses 5s default; `notify-send -u critical` with no `-t` stays visible until user dismissal or `CloseNotification`; config file override changes the default to a custom value and subsequent notifications obey it.

---

### REQ-F-016: Never-Expire Timeout (expire_timeout = 0)
**Event-driven:** When `Notify` is called with `expire_timeout = 0`, the system shall not auto-expire the notification; it closes only on user action or `CloseNotification`.

**Acceptance criterion:** `notify-send -t 0` produces a toast that remains visible after 60s; it closes only when the user dismisses it or `CloseNotification` is called.

---

### REQ-F-017: Hover Pauses Expiry
**Event-driven:** When the mouse pointer enters a visible toast, the system shall pause its expiry timer; when the pointer leaves, the timer shall resume from the remaining time.

**Acceptance criterion:** hovering a 5s toast at the 2s mark prevents expiry; leaving hover allows the remaining 3s to elapse and expire the notification. Manual verification with pointer movement and `dbus-monitor`.

---

### REQ-F-018: Replace Resets Timer
**Event-driven:** When `Notify` with `replaces_id` updates an existing notification, the system shall reset its expiry timer to the new `expire_timeout` value (or default if -1).

**Acceptance criterion:** an active 5s toast updated via replace at the 4s mark receives a fresh 5s timer; if it would have expired at 5s, it instead expires at 5s + 4s = 9s total.

---

## Monitor Placement

### REQ-F-019: Extend ActiveWindowService with focusedMonitor
**Ubiquitous:** The system shall extend the existing `ActiveWindowService` class to add a `focusedMonitor` property (string) and `focusedMonitorChanged(QString name)` signal, derived from Hyprland's `j/monitors` (focused flag) and `focusedmon` IPC events, reusing the existing IPC socket.

**Acceptance criterion:** `focusedMonitor` property reflects the current focused monitor name; signal emits when Hyprland's focused monitor changes; no additional socket is created (verified via `netstat` or strace).

---

### REQ-F-020: Toast Placement on Focused Monitor
**Event-driven:** When a new notification is created, the system shall determine the focused monitor via `ActiveWindowService::focusedMonitor` and render the toast on that monitor.

**Acceptance criterion:** multi-monitor setup: `notify-send` while focus is on HDMI-1 places the toast on HDMI-1; switching focus to DP-2 and sending a new notification places the new toast on DP-2. If IPC is down, use the primary monitor (fallback).

---

### REQ-F-021: Toast Monitor Affinity
**State-driven:** While a toast is visible, it shall remain on the monitor on which it was created, even if the focused monitor changes.

**Acceptance criterion:** a toast born on HDMI-1 stays on HDMI-1 when focus switches to DP-2; a new notification sent after the focus change appears on DP-2. Verified by visual inspection or monitor-aware geometry queries.

---

### REQ-F-022: Fallback Monitor for Unknown Focus
**Ubiquitous:** If the focused monitor cannot be determined (IPC down, startup race), the system shall place toasts on the primary monitor.

**Acceptance criterion:** stopping Hyprland IPC and sending a notification places it on the primary monitor; resuming IPC and sending a new notification places it on the currently focused monitor.

---

## Actions

### REQ-F-023: Action Button Rendering
**Ubiquitous:** The system shall render the `actions` array from `Notify` as a grid of outlined HUD-style buttons in the toast footer, one button per action key-value pair.

**Acceptance criterion:** `notify-send --action "reply:Reply" --action "ignore:Ignore"` produces a toast with two buttons labeled "Reply" and "Ignore"; buttons are styled with outline (not filled), matching HUD affordance.

---

### REQ-F-024: Default Action on Body Click
**Event-driven:** When the user clicks the toast body (summary or body text, not an action button), the system shall:
- If the notification declares a `"default"` action key, invoke that action and emit `ActionInvoked(id, "default")`.
- Otherwise, dismiss the toast (close with reason 2).

**Acceptance criterion:** a notification with `--action "default:Open"` closes with `ActionInvoked(id, "default")` on body click; a notification without a default action closes with reason 2 on body click. Verified by `dbus-monitor` and toast state.

---

### REQ-F-025: Action Button Invocation
**Event-driven:** When the user clicks an action button, the system shall emit `ActionInvoked(id, action_key)` for that action and close the notification with reason 3, unless the `resident` hint is true.

**Acceptance criterion:** clicking a button labeled "Reply" (action key "reply") emits `ActionInvoked(id, "reply")` and closes the toast; if the notification has `hints["resident"] = true`, the toast remains visible after the action.

---

### REQ-F-026: Resident Hint Behavior
**Conditional:** Where the `resident` hint (boolean) is set to true, the system shall keep the notification visible after an action is invoked, emitting `ActionInvoked` but not closing the toast.

**Acceptance criterion:** a notification with `--hint "resident:true"` and an action button remains visible after the button is clicked; a second click invokes another action without closing; closing the notification explicitly (via body dismiss or `CloseNotification`) closes it.

---

## Rendering & Markup

### REQ-F-027: Summary Rendering
**Ubiquitous:** The system shall render the `summary` field as a single elided line in the toast header, using the system theme font and HoloniightPalette accent color.

**Acceptance criterion:** a long summary (>80 chars) displays as a single line with "…" ellipsis at the end; shortening the window narrows the elision point; font and color match the HoloNight design system.

---

### REQ-F-028: Body Rendering with Markup
**Ubiquitous:** The system shall render the `body` field using Qt's `Text.StyledText` mode, supporting `<b>`, `<i>`, `<u>`, and `<a>` tags; `<img>` tags shall be stripped; hyperlinks shall be styled cyan but non-clickable.

**Acceptance criterion:** `notify-send "Title" "Text with <b>bold</b> and <i>italic</i>"` displays bold and italic text; `<a href="...">link</a>` displays cyan text but does not open the URL on click; `<img src="...">` is omitted (no pixmap rendered); body text elides to ~3 lines.

---

### REQ-F-029: Body Elision
**Ubiquitous:** The system shall elide the `body` field to approximately 3 lines; additional text shall be hidden with a "…" indicator.

**Acceptance criterion:** a 10-line body displays 3 lines with "…" at the end of the 3rd line; the toast height does not grow beyond that point.

---

## Overflow & Queue

### REQ-F-030: Maximum Visible Toasts
**Ubiquitous:** The system shall display at most `max_visible` toasts concurrently on each monitor (config value, default 3).

**Acceptance criterion:** sending 5 notifications rapidly when `max_visible=3` displays 3 toasts and queues 2; closing one toast pops the next queued notification from the queue.

---

### REQ-F-031: FIFO Queue for Overflow
**Event-driven:** When a new notification arrives and the visible set is full of normal/low-urgency notifications, the system shall append it to a FIFO queue.

**Acceptance criterion:** 3 visible toasts + 2 queued; closing the oldest visible toast removes it and promotes the 1st queued toast to visible (in time order, not priority order — unless it is critical).

---

### REQ-F-032: Critical Priority Jump
**Conditional:** Where a critical notification (urgency hint = 2) arrives and the visible set is full of non-critical notifications, the system shall move the oldest non-critical visible toast back to the queue and display the critical notification immediately.

**Acceptance criterion:** 3 visible normal toasts + 1 queued; `notify-send -u critical "Urgent"` bumps the oldest normal toast back to the queue and displays the critical toast immediately; the bumped toast re-enters the queue at its original priority position (FIFO, not rear).

---

### REQ-F-033: No Visual Overflow Indicator
**Ubiquitous:** The system shall not display a "+N more" chip, badge, or notification count when toasts are queued.

**Acceptance criterion:** 5 notifications with `max_visible=3` displays 3 toasts with no visible indicator of the 2 queued notifications.

---

### REQ-F-034: Per-Monitor Queue
**State-driven:** While notifications are managed, the system shall maintain a separate queue per monitor; toasts on HDMI-1 do not block toasts from appearing on DP-2, and vice versa.

**Acceptance criterion:** dual-monitor setup; 3 toasts on HDMI-1 (full) and 3 toasts on DP-2 (full) coexist; sending notifications to each monitor independently enqueues on their respective monitor queues.

---

## Accent Color Routing

### REQ-F-035: Critical Urgency Always Red
**Conditional:** Where the `urgency` hint is 2 (critical), the system shall render the toast accent color as red (from HoloniightPalette.critical or equivalent).

**Acceptance criterion:** `notify-send -u critical` produces a toast with red accent (glow, header bar, or action button highlight); `notify-send -u normal` or default produces a different color.

---

### REQ-F-036: Category-Based Accent Routing
**Event-driven:** When a notification's `category` hint begins with `im.`, `call.`, or `presence.` (e.g., `im.received`, `call.missed`), the system shall render the toast accent as violet (from HoloniightPalette.violet or category color).

**Acceptance criterion:** `notify-send --hint category:im.received` produces a violet accent; `notify-send --hint category:call.incoming` produces violet; `notify-send --hint category:mail.arrived` produces the default (cyan) because it does not match the prefixes.

---

### REQ-F-037: Default Accent Color
**Ubiquitous:** For notifications that are not critical and whose category does not match a routed prefix, the system shall use cyan (from HoloniightPalette.accent or default) as the toast accent.

**Acceptance criterion:** `notify-send "Title" "Body"` (no urgency, no category) produces a cyan-accent toast; this is the fallback for all unrouted cases.

---

### REQ-F-038: No App-Name Lookup Table
**Constraint:** The system shall not maintain or consult a mapping of application names to accent colors; all accent routing shall be based solely on urgency and category hints.

**Acceptance criterion:** adding a new app or changing app names does not require code changes to accent color logic; accent is determined only by urgency and category hint values.

---

## TOML Configuration

### REQ-F-039: Config Block Location and Keys
**Ubiquitous:** The system shall read notification settings from a `[notifications]` block in the existing TOML config file at `$XDG_CONFIG_HOME/holonight/config.toml` (injected via `ConfigService`), supporting keys: `default_timeout_ms` (integer, default 5000) and `max_visible` (integer, default 3).

**Acceptance criterion:** a config file with `[notifications]` section containing `default_timeout_ms = 3000` and `max_visible = 5` causes subsequent notifications to use 3s default timeout and display at most 5 toasts; missing keys use defaults.

---

### REQ-F-040: Config Live-Reload
**Event-driven:** When the config file is modified and `ConfigService` signals changes, the system shall re-read the `[notifications]` block and apply the new values to future notifications; existing visible/queued notifications retain their original timeout timers.

**Acceptance criterion:** editing the config to change `default_timeout_ms` to 10000, saving, and sending a new notification applies the new timeout to the new notification; an already-active notification's timer is unaffected.

---

## Testing & Acceptance

### REQ-NF-041: Model Logic Coverage
**Ubiquitous:** The system's notification model logic shall be covered by GTest unit tests exercising the core paths: id allocation, replace semantics, queue management (FIFO + per-monitor overflow), timeout-to-reason mapping, critical priority jump, and category-to-accent mapping.

**Acceptance criterion:** `task test` runs and all notification-related tests pass; the suite contains at least one test for each core path listed above (id allocation, replace, queue overflow, critical priority, timeout→reason, category→accent). No fixed line-coverage percentage is gated.

---

### REQ-NF-042: Manual Integration Testing
**Ubiquitous:** The system shall be manually tested in a live Wayland session using `notify-send` to verify: basic toast display, actions, timeouts, multi-monitor placement, queue overflow, critical priority, markup rendering, and `dbus-monitor` signal correctness.

**Acceptance criterion:** a written test plan (in comments or docs) documents at least 10 manual test cases (e.g., "send critical while queue full", "hover pause timer", "replace updates content"); each test is performed in a live session and documented as passed/failed.

---

## Constraints

### REQ-C-043: D-Bus Class Declaration
**Constraint:** The `NotificationServer` D-Bus service class shall be defined in a header file (`.h`), not a `.cpp` file, to allow proper moc processing. If defined in a `.cpp`, it shall include `#include "FileName.moc"` at the end of that file.

**Acceptance criterion:** compilation succeeds without AutoMoc link errors; `dbus-send` introspection shows the correct interface and methods.

---

### REQ-C-044: QML Module and QRC Paths
**Constraint:** Toast popup QML files shall be registered in the `HolonightShell` QML module (URI), added to `HOLONIGHT_QML_FILES` in `CMakeLists.txt`, and use QRC prefix `/HolonightShell/` (e.g., `qrc:/HolonightShell/Notifications/Toast.qml`).

**Acceptance criterion:** `task configure` succeeds; CMake does not report missing QML files; the toast surface loads without "unknown component" errors at runtime.

---

### REQ-C-045: Color Sourcing from Theme
**Constraint:** All colors in the toast QML (accent, text, background, glow) shall be sourced from `HoloniightPalette.<token>` (imported via `import Holonight`); no hardcoded hex color values shall appear in QML.

**Acceptance criterion:** running `grep -r "#[0-9a-fA-F]\\{6\\}" src/qml/Notifications/` returns no matches; all colors query `HoloniightPalette` properties.

---

### REQ-C-046: Glow via MultiEffect
**Constraint:** Toast glow effects shall use `QtQuick.Effects.MultiEffect` with `shadowEnabled: true`; the deprecated `Qt5Compat.GraphicalEffects.Glow` shall not be used.

**Acceptance criterion:** compiling the QML does not reference `Qt5Compat.GraphicalEffects.Glow`; the toast displays a glow halo matching the HoloNight design; `task qml-lint` reports no errors.

---

### REQ-C-047: Build via Task
**Constraint:** Building the notification daemon (and all holonight-shell code) shall use the `task` command (defined in Taskfile.yml), not CMake directly; supported tasks are `task configure`, `task build`, `task test`, `task format`, `task tidy`, `task qml-lint`.

**Acceptance criterion:** running `task build` succeeds; `task format-check`, `task tidy`, and `task qml-lint` report no notification-daemon-related failures (pre-existing failures in other files are not the daemon's responsibility).

---

### REQ-C-048: Variable Naming (clang-tidy)
**Constraint:** All C++ variable names in the notification daemon shall be ≥3 characters; abbreviations like `eq`, `fi`, `ok`, `id` (in loop counters) shall be avoided to comply with `readability-identifier-length` clang-tidy rule.

**Acceptance criterion:** `task tidy` reports no `readability-identifier-length` violations in files under `src/services/notifications/`; variable names use `notifId`, `idMap`, `msgId`, etc. instead of short forms.

---

## Deferred Decisions (to be Specified in v2+)

- **Notification Center / History Sidebar:** persistent list of recent notifications, searchable by app/category, with action replay.
- **Critical Alert Overlay:** full-screen or top-center "urgent" notification with auto-lock / interaction-required semantics.
- **Top-Bar Indicator:** notification badge on topbar showing unread count or icon.
- **Do Not Disturb / Quiet Mode:** system-wide or per-app silent mode; critical notifications still visible or priority-filtered.
- **Notification Grouping:** by application, category, or time window; "+N more" expandable UI.
- **Persistence & Archive:** notifications saved to disk; restored on restart; query API for history.
- **Image Data & Path Rendering:** support for `image-data` (pixmap binary) and `image-path` (file) hints; app icons and custom images in toast.
- **Sound & Vibration:** play audio/haptic feedback on notification arrival; per-urgency or per-category rules.

---

## Summary of Requirements by Category

| Category | Count |
|----------|-------|
| Functional (REQ-F-*) | 38 |
| Non-Functional (REQ-NF-*) | 2 |
| Constraint (REQ-C-*) | 6 |
| **Total** | **46** |

---

## References & Appendix

### D-Bus Freedesktop Notification Spec
- https://specifications.freedesktop.org/notification-spec/notification-spec-latest.html
- Standard close reason codes: 1 (expired), 2 (dismissed), 3 (closed), 4 (undefined)

### HoloNight Design System
- See `assets/dont-commit/` in the project for color palette, glow patterns, and visual examples.

### Hyprland IPC
- `j/monitors` and `focusedmon` events used by `ActiveWindowService` for monitor focus tracking.

### Qt6 & QML
- `QtQuick.Effects.MultiEffect` for glow (preferred over Qt5Compat).
- `Text.StyledText` mode for markup support.

### Config Integration
- `ConfigService` injects TOML values into dependent services; no manual file I/O in NotificationService.

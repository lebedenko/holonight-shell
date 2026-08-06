# Power Extensions Feature Specification

## Overview

The Power Extensions feature enhances HoloNight Shell's power management capabilities through four integrated components:

1. **Activity Gate**: Suspends internal service polling when the laptop lid closes, resuming on open
2. **Low-Battery Notifications**: Alerts the user when battery drops below configurable thresholds
3. **Suspend Inhibitor Diagnostics**: Displays active system suspend inhibitors in the sidebar
4. **Charge Limit Display**: Shows the current battery charge limit when available

These features build on existing `BatteryService` (UPower), `PowerProfilesService` (power-profiles-daemon), and sidebar integration patterns established in the codebase.

---

## Feature 1: Activity Gate (Lid-Close Power Saving)

When the laptop lid closes, HoloNight must pause all internal service timers and polling. On lid open, services resume. This is purely internal plumbing—no D-Bus/logind configuration.

### Functional Requirements

#### REQ-F-001 — ActivityGate Interface
**Ubiquitous**: The system shall define a formal `ActivityGate` interface that services implement to participate in the gate.

**Acceptance Criterion**: An abstract base class or Qt meta-interface named `ActivityGate` exists in the codebase with virtual methods `pauseActivity()` and `resumeActivity()`, and at least one service (e.g., `BatteryService` or a new `LidStateMonitor`) inherits and implements these methods successfully.

---

#### REQ-F-002 — Pause on Lid Close
**Event-driven**: When the laptop lid closes, the system shall pause all services implementing `ActivityGate` within 1 second.

**Acceptance Criterion**: A test or manual verification demonstrates that within 1 second of lid close (detected via UPower), all registered `ActivityGate` implementations have `pauseActivity()` called exactly once, and all timers/polling on those services are stopped (no D-Bus calls in the following 2 seconds).

---

#### REQ-F-003 — Resume on Lid Open
**Event-driven**: When the laptop lid opens, the system shall resume all paused services implementing `ActivityGate`.

**Acceptance Criterion**: A test or manual verification demonstrates that after lid open (detected via UPower), all previously paused `ActivityGate` implementations have `resumeActivity()` called exactly once, and polling/timers resume within 2 seconds.

---

#### REQ-F-004 — Lid State from UPower
**Ubiquitous**: The system shall read lid open/close state from the UPower D-Bus service.

**Acceptance Criterion**: A component (e.g., `LidStateMonitor`) subscribes to UPower lid device signals and emits local signals on lid state change; the subscription persists and updates are received within 500 ms of a physical lid state change on test hardware.

---

#### REQ-F-005 — Graceful Desktop Handling
**If** a laptop has no lid device (e.g., a desktop system), **then** the system shall degrade gracefully without errors or warnings in the log.

**Acceptance Criterion**: When the system starts on a desktop with no lid device, UPower queries return no lid object, the `LidStateMonitor` does not log errors (or logs a single info-level message "Lid device not available"), and the activity gate remains functional (services never pause).

---

### Non-Functional Requirements

#### REQ-NF-001 — Pause/Resume Latency
**Ubiquitous**: The system shall complete all `pauseActivity()` calls and stop polling within 1 second of lid close detection.

**Acceptance Criterion**: Measure wall-clock time from UPower `LidClosed` signal emission to the last D-Bus call from a paused service; this interval must be ≤ 1 second on a system under typical load.

---

### Constraints

#### REQ-C-001 — No Lid UI
**Ubiquitous**: The system shall not display lid state in any user-facing UI.

**Acceptance Criterion**: Code review confirms no QML component, sidebar row, or status indicator shows lid open/closed state; lid state is internal only.

---

## Feature 2: Low-Battery Notifications

HoloNight must fire one-time desktop notifications when battery percent drops below configurable thresholds while discharging. Thresholds reset when charging or plugged in.

### Functional Requirements

#### REQ-F-006 — Battery Percent Monitoring
**Ubiquitous**: The system shall read battery percent from `BatteryService`.

**Acceptance Criterion**: A component subscribes to `BatteryService::percentChanged()` and receives updates whenever UPower battery percent changes; the latest value is accessible for threshold comparison at any time.

---

#### REQ-F-007 — Warning Threshold Notification
**Event-driven**: When battery percent drops to or below the warning threshold while discharging, the system shall fire a desktop notification with appropriate warning styling.

**Acceptance Criterion**: With threshold set to 20%, when battery discharges from 21% to 20%, a desktop notification appears with a distinct warning appearance (icon/color); the notification does not re-fire on subsequent reads at ≤ 20% in the same discharge cycle.

---

#### REQ-F-008 — Critical Threshold Notification
**Event-driven**: When battery percent drops to or below the critical threshold while discharging, the system shall fire a desktop notification with critical styling.

**Acceptance Criterion**: With threshold set to 10%, when battery discharges from 11% to 10%, a desktop notification appears with distinct critical appearance (urgent icon/color); the notification does not re-fire on subsequent reads at ≤ 10% in the same discharge cycle.

---

#### REQ-F-009 — Notification Reset on Charge
**Event-driven**: When `BatteryService::charging()` or `fullyCharged()` becomes true, the system shall reset the notification state (allow new notifications on the next discharge below thresholds).

**Acceptance Criterion**: After warning notification fires at 20%, manually charge the battery to 21%, then discharge back to 20%; the warning notification fires again (second time in overall session).

---

#### REQ-F-010 — Notification Reset on Battery Insert
**Event-driven**: When `BatteryService::present()` changes (battery inserted/removed), the system shall reset notification state.

**Acceptance Criterion**: Remove and reinsert the battery (or simulate via test mock); notification state resets and future threshold crossings re-trigger notifications as if the session just started.

---

#### REQ-F-011 — No Automatic Actions
**Ubiquitous**: The system shall take no automatic power management actions (e.g., forced suspend, profile switch) in response to low-battery notifications.

**Acceptance Criterion**: Code review and test logs confirm no calls to `PowerProfilesService`, `logind.Manager.Suspend`, or similar when notifications fire; only desktop notification D-Bus calls are present.

---

### Non-Functional Requirements

#### REQ-NF-002 — Configurable Thresholds
**Ubiquitous**: The system shall allow users to configure warning and critical battery thresholds.

**Acceptance Criterion**: A settings storage mechanism (file or D-Bus properties) allows reading/writing `warningThresholdPercent` and `criticalThresholdPercent` values; the system loads and applies these on startup and reacts to changes at runtime.

---

### Constraints

#### REQ-C-002 — Default Warning Threshold
**Ubiquitous**: The system shall default to 20% for the warning threshold.

**Acceptance Criterion**: If no settings file exists or the setting is unset, `warningThresholdPercent` is 20.

---

#### REQ-C-003 — Default Critical Threshold
**Ubiquitous**: The system shall default to 10% for the critical threshold.

**Acceptance Criterion**: If no settings file exists or the setting is unset, `criticalThresholdPercent` is 10.

---

#### REQ-C-004 — logind Remains Authoritative
**Ubiquitous**: The system shall not invoke any suspend, shutdown, or power-policy actions; `logind` retains all power authority.

**Acceptance Criterion**: HoloNight code contains no calls to `org.freedesktop.login1.Manager.{Suspend,PowerOff,Halt,Reboot,ScheduleShutdown}` methods.

---

## Feature 3: Suspend Inhibitor Diagnostics

HoloNight reads the active suspend inhibitor list from `logind` and displays inhibitor details (process name + reason) in the sidebar battery section when any exist.

### Functional Requirements

#### REQ-F-012 — Query logind Inhibitors
**Ubiquitous**: The system shall query `org.freedesktop.login1.Manager.ListInhibitors()` and parse the active suspend inhibitor list.

**Acceptance Criterion**: A component (e.g., `SuspendInhibitorService`) makes a D-Bus call to `logind` on startup and periodically (e.g., 5-second interval), receives a list of active inhibitors (each with `(Who, Why, What, Mode)`), and stores the inhibitor entries.

---

#### REQ-F-013 — Display Inhibitors in Sidebar
**State-driven**: While any suspend inhibitor is active, the system shall display the inhibitor list in the sidebar battery section.

**Acceptance Criterion**: A new row/section appears in the battery panel showing active inhibitors; each inhibitor row displays process name (`Who` field) and reason (`Why` field); the section is visible and scrollable if multiple inhibitors exist.

---

#### REQ-F-014 — Hide When No Inhibitors
**State-driven**: While no suspend inhibitors are active, the system shall hide the inhibitor display section.

**Acceptance Criterion**: When `ListInhibitors()` returns an empty list, the inhibitor row/section is not rendered in the sidebar (or is set to `visible: false` if pre-allocated); no placeholder text appears.

---

#### REQ-F-015 — Inhibitor Details
**Ubiquitous**: The system shall display the inhibitor process name and reason for each active inhibitor.

**Acceptance Criterion**: A test or manual session with a known inhibitor (e.g., `systemd-logind` preventing suspend) shows both the process name and a human-readable reason string in the sidebar.

---

#### REQ-F-016 — No Inhibitor Take
**Ubiquitous**: The system shall NOT create, hold, or register its own suspend inhibitor with `logind`.

**Acceptance Criterion**: Code review confirms no calls to `org.freedesktop.login1.Manager.Inhibit()`; HoloNight appears in `logind`'s inhibitor list only if a user or another service explicitly adds it.

---

### Non-Functional Requirements

#### REQ-NF-003 — Inhibitor Query Frequency
**Ubiquitous**: The system shall poll the inhibitor list at intervals not exceeding 5 seconds.

**Acceptance Criterion**: The inhibitor service updates at a fixed or adaptive interval ≤ 5 seconds; manual verification confirms new inhibitors appear in the sidebar within 5 seconds of activation.

---

### Constraints

#### REQ-C-005 — Diagnostic Only
**Ubiquitous**: The sidebar inhibitor display is read-only and informational; the system shall not provide UI controls to add, remove, or modify inhibitors.

**Acceptance Criterion**: Code review confirms no buttons, toggles, or interactive elements in the inhibitor section; it is text/icon display only.

---

## Feature 4: Charge Limit Display

HoloNight attempts to read the battery charge limit from UPower or sysfs and displays it in the sidebar if available; if unavailable, the row is hidden entirely.

### Functional Requirements

#### REQ-F-017 — Query UPower Charge Limit
**Ubiquitous**: The system shall attempt to read the battery charge limit from the UPower service.

**Acceptance Criterion**: A component queries UPower's battery device object for a charge limit property (e.g., `org.freedesktop.UPower.Device.ChargeMaximum` or equivalent); if the property exists and is readable, its value is captured.

---

#### REQ-F-018 — Fallback to sysfs
**If** the UPower charge limit property is unavailable or returns an error, **then** the system shall attempt to read from sysfs.

**Acceptance Criterion**: When UPower does not expose charge limit, the system attempts to read from sysfs paths (e.g., `/sys/class/power_supply/BAT0/charge_max` or similar); if sysfs contains a valid value, it is used.

---

#### REQ-F-019 — Display Charge Limit
**Where** a charge limit is successfully retrieved, the system shall display it in the sidebar battery section.

**Acceptance Criterion**: A new row in the battery panel shows the charge limit (e.g., "Charge Limit: 80%"); the row is visible when limit data is available.

---

#### REQ-F-020 — Hide When Unavailable
**Where** a charge limit is not available from UPower or sysfs, the system shall not display the charge limit row.

**Acceptance Criterion**: On a system with no exposed charge limit, the charge limit row is not rendered (or is set to `visible: false`); no "N/A", "Not available", or placeholder text appears.

---

#### REQ-F-021 — No Charge Limit Write
**Ubiquitous**: The system shall not provide any interface to set or modify the battery charge limit.

**Acceptance Criterion**: Code review confirms no calls to UPower or sysfs write paths for charge limits; the display is read-only.

---

### Non-Functional Requirements

#### REQ-NF-004 — Charge Limit Persistence
**Ubiquitous**: The system shall query charge limit information once on startup and cache the result for the session; it shall refresh only if the underlying property signals a change.

**Acceptance Criterion**: A single UPower or sysfs read occurs at startup (or on BatteryService first availability); subsequent updates only if UPower emits a property-changed signal or sysfs file modification is detected via inotify.

---

### Constraints

#### REQ-C-006 — Read-Only
**Ubiquitous**: The charge limit display in the sidebar is read-only and informational.

**Acceptance Criterion**: Code review confirms no write paths, buttons, or input fields for charge limit modification; display only.

---

## Integration Notes

- **ActivityGate**: CalendarSyncManager's existing idle gate serves as a pattern reference; apply the same dual-callback (`pauseActivity()` / `resumeActivity()`) pattern to BatteryService, LidStateMonitor, SuspendInhibitorService, and other periodic services.
- **Notifications**: Use the existing notification system (D-Bus org.freedesktop.Notifications interface) already integrated with the sidebar notification history.
- **Sidebar Layout**: Low-battery, inhibitor, and charge-limit rows all live in the battery section of the right sidebar; coordinate with existing battery-info and brightness-slider rows to avoid layout conflicts.
- **Testing**: Unit tests should mock UPower/logind D-Bus responses; QML tests should verify sidebar visibility/hiding logic when inhibitor or charge-limit data is present/absent.

---

## Acceptance Criteria Summary

| Requirement | Verification Method |
|---|---|
| ActivityGate interface exists | Code review + inheritance check |
| Pause within 1 second | Manual test with lid sensor or D-Bus spy |
| Resume fires on lid open | Manual test + service polling verification |
| Battery notifications fire once | Manual discharge test with threshold crossing |
| Thresholds are configurable | Settings mutation + notification re-test |
| Inhibitor list displayed | Manual test with active inhibitor (e.g., Teams away) |
| No charge limit → hidden | Manual test on desktop or system without limit |
| Charge limit displayed when available | Manual test on supported hardware |


# Spec — notification-hardening

## Overview

The notification-hardening pipeline extends the existing `org.freedesktop.Notifications` D-Bus server in HoloNight shell with user control and criticality awareness. This feature adds Do Not Disturb (DND) mode, per-application notification rules, special handling for critical-urgency notifications, and detection when another D-Bus service already owns the notifications interface.

## Scope

### In scope
- Do Not Disturb (DND) mode: manual toggle, runtime-only, no persistence
- Per-application notification rules: per-app on/off toggle and urgency-level filtering
- Critical notification handling: persistent display, red/warning styling, DND bypass
- Daemon-already-running detection: diagnostic display in sidebar when another process owns `org.freedesktop.Notifications`
- UI controls and dialogs for managing notification settings
- History and persistence of per-app rules within the session

### Out of scope
- Scheduled or automatic DND (e.g., time-based, calendar-aware)
- DND persistence across shell restarts
- Notification blocking based on content/keywords (only app name and urgency)
- Custom sounds or vibration on critical notifications
- Integration with system-wide notification do-not-disturb services (e.g., OS-level DND)
- Rebuild or replacement of the existing notification server implementation

---

## Requirements

### DND Mode

**REQ-F-NH01** — Ubiquitous  
The shell shall provide a Do Not Disturb (DND) toggle in the sidebar notification settings.

**Acceptance Criterion:**  
A DND toggle control is visible in the sidebar notification settings section; toggling it switches DND on/off with immediate effect.

---

**REQ-F-NH02** — State-driven  
While DND is enabled, the shell shall suppress all incoming notifications that do not have critical urgency.

**Acceptance Criterion:**  
A notification with urgency < 2 is received when DND is on; the notification does not display and does not make sounds; urgency is verified via `org.freedesktop.Notifications.Notify` hints dictionary.

---

**REQ-F-NH03** — State-driven  
While DND is enabled, the shell shall deliver and display all incoming notifications with critical urgency (`urgency=2`).

**Acceptance Criterion:**  
A notification with urgency=2 is received when DND is on; the notification displays immediately and is not suppressed.

---

**REQ-F-NH04** — Ubiquitous  
The shell shall not persist DND state across restarts.

**Acceptance Criterion:**  
DND is toggled on; the shell is restarted; DND is off after restart.

---

**REQ-F-NH05** — Ubiquitous  
The shell shall provide visual indication of DND status in the notification settings UI.

**Acceptance Criterion:**  
The DND toggle state is visually distinguishable when on vs. off; the indicator updates immediately when toggled.

---

### Per-App Rules

**REQ-F-NH06** — Event-driven  
When a notification is received with an `app_name` hint, the shell shall add that app to the per-app rules list if not already present.

**Acceptance Criterion:**  
A notification arrives with `app_name="ExampleApp"` and no rule exists for it; after delivery, "ExampleApp" appears in the app list in notification settings.

---

**REQ-F-NH07** — Ubiquitous  
The shell shall allow toggling on/off notifications per application via a per-app notification enable/disable rule.

**Acceptance Criterion:**  
A rule for "ExampleApp" exists in the settings; toggling it off prevents notifications from "ExampleApp" from displaying; toggling it on re-enables them.

---

**REQ-F-NH08** — Ubiquitous  
The shell shall allow setting an urgency filter per application: suppress low (`urgency=0`), suppress normal (`urgency=1`), or suppress all (`urgency=0,1`).

**Acceptance Criterion:**  
A per-app rule has urgency filter set to "suppress normal"; a notification arrives with urgency=1 from that app; it does not display; a notification with urgency=2 from the same app displays normally.

---

**REQ-F-NH09** — Event-driven  
When a notification from an app is suppressed by a per-app rule, the shell shall not add the suppressed notification to the notification history.

**Acceptance Criterion:**  
A per-app rule suppresses all notifications from "ExampleApp"; a notification arrives from "ExampleApp"; it does not appear in the notification history.

---

**REQ-F-NH10** — Conditional  
Where a per-app rule is set to off (notifications disabled for that app), the shell shall suppress all notifications from that app regardless of urgency.

**Acceptance Criterion:**  
An app rule is toggled off; a notification with urgency=2 arrives from that app; it does not display.

---

**REQ-F-NH11** — Ubiquitous  
The shell shall persist per-app rules within the session.

**Acceptance Criterion:**  
A per-app rule is configured; the sidebar is closed and reopened; the rule persists with the same settings.

---

**REQ-F-NH12** — Ubiquitous  
The shell shall not persist per-app rules across shell restarts.

**Acceptance Criterion:**  
A per-app rule is configured; the shell is restarted; the rule list is empty or reset to defaults.

---

**REQ-F-NH13** — Ubiquitous  
The shell shall populate the app list with app names observed in the current session only; no pre-configured app list shall be shipped.

**Acceptance Criterion:**  
The shell starts; the per-app rules list is empty; a notification arrives with `app_name="NewApp"`; "NewApp" appears in the list; the shell restarts; the list is empty again.

---

### Critical Notification Handling

**REQ-F-NH14** — Event-driven  
When a notification with critical urgency (`urgency=2`) is received, the shell shall apply red or warning-colored accent styling to the notification display.

**Acceptance Criterion:**  
A notification with urgency=2 is received; the notification's visual container uses the red/warning accent color from the HoloNight palette; this is visually distinct from normal-urgency notifications.

---

**REQ-F-NH15** — Event-driven  
When a notification with critical urgency is received, the shell shall not apply an auto-dismiss timeout.

**Acceptance Criterion:**  
A notification with urgency=2 is received; 10 seconds elapse; the notification remains displayed; it dismisses only when the user explicitly closes it.

---

**REQ-F-NH16** — Conditional  
Where a notification has critical urgency, the shell shall deliver and display it even if the application has a per-app rule disabling notifications from that app.

**Acceptance Criterion:**  
A per-app rule for "CriticalApp" is set to off; a notification with urgency=2 from "CriticalApp" is received; it displays immediately.

---

**REQ-F-NH17** — Ubiquitous  
The shell shall display a persistent dismiss button or close affordance on critical notifications.

**Acceptance Criterion:**  
A notification with urgency=2 is displayed; a close/dismiss button is visible and clickable; clicking it removes the notification.

---

### Daemon Detection

**REQ-F-NH18** — Event-driven  
When the shell starts, if another process already owns `org.freedesktop.Notifications` on the session D-Bus, the shell shall display a diagnostic message in the sidebar diagnostics area.

**Acceptance Criterion:**  
Another notification daemon is running and owns `org.freedesktop.Notifications`; the shell is launched; a diagnostic message appears in the sidebar indicating that another notification service is already active; the shell continues to run.

---

**REQ-F-NH19** — Ubiquitous  
When a daemon-already-running diagnostic is displayed, the shell shall continue to operate and shall not exit.

**Acceptance Criterion:**  
The daemon detection diagnostic is triggered; the shell remains running; all other features function normally.

---

**REQ-F-NH20** — Ubiquitous  
The diagnostic message for daemon-already-running shall be clear and actionable (e.g., indicating which service owns the interface and suggesting to stop it).

**Acceptance Criterion:**  
The diagnostic message is displayed; it names the D-Bus service owning `org.freedesktop.Notifications` (or displays "unknown" if not retrievable); it suggests stopping the conflicting service.

---

### Notification Delivery & Interaction

**REQ-F-NH21** — Event-driven  
When a notification passes all DND and per-app rules and is not suppressed, the shell shall display it to the user.

**Acceptance Criterion:**  
A notification is received with urgency=1 from "App1"; no DND is active; App1 has no suppression rule; the notification displays immediately.

---

**REQ-F-NH22** — Event-driven  
When a user explicitly dismisses a notification, the shell shall remove it from the display.

**Acceptance Criterion:**  
A notification is displayed; the user clicks the dismiss button; the notification is removed from the screen and from the notification history.

---

**REQ-F-NH23** — Ubiquitous  
The shell shall apply DND and per-app rules to incoming notifications only; existing notifications in the history shall not be retroactively hidden or removed.

**Acceptance Criterion:**  
A notification is displayed and added to history; DND is then toggled on, or a per-app rule is configured to suppress that app; the historical notification remains visible in the history.

---

### Non-Functional & Constraints

**REQ-NF-NH01** — Non-Functional  
DND and per-app rules shall take effect immediately upon configuration without requiring a shell restart.

**Acceptance Criterion:**  
DND is toggled on; a new notification is received within 1 second; it is suppressed without delay.

---

**REQ-NF-NH02** — Non-Functional  
Per-app rule changes shall not cause perceptible lag or stuttering in the shell UI.

**Acceptance Criterion:**  
A per-app rule is toggled on/off repeatedly; the sidebar remains responsive and no frame drops is observed.

---

**REQ-C-NH01** — Constraint  
The app list shall be populated from the `app_name` hint in the `org.freedesktop.Notifications.Notify` D-Bus call; if the hint is absent or empty, the notification shall be treated as from an unlabeled app.

**Acceptance Criterion:**  
A notification is sent without an `app_name` hint; it does not create an entry in the per-app rules list; or it is grouped under a special "Unlabeled" or "Unknown" category.

---

**REQ-C-NH02** — Constraint  
Critical urgency shall be defined as notifications with the urgency hint value of 2 per the freedesktop.org notification specification.

**Acceptance Criterion:**  
The code uses `hints["urgency"].toUInt() == 2` (or equivalent) to identify critical notifications.

---

**REQ-C-NH03** — Constraint  
The shell shall use HoloNight theme colors for critical notification styling; no hardcoded hex colors shall appear in QML.

**Acceptance Criterion:**  
The critical notification visual styling imports `Holonight` and accesses colors via `HoloniightPalette.<token>` (e.g., a red/warning token).

---

**REQ-C-NH04** — Constraint  
Per-app rules shall be stored in memory only during the session; there shall be no persistent file or database storage of rules.

**Acceptance Criterion:**  
The shell's process terminates; no rule configuration file is written; rules are gone after restart.

---

**REQ-C-NH05** — Constraint  
The daemon detection check shall occur at shell startup, before any D-Bus method advertisements are made.

**Acceptance Criterion:**  
The shell starts; it checks D-Bus for existing `org.freedesktop.Notifications` ownership before registering its own service.

---

---

## Known Limitations

### App Name String Matching

The per-app rules system keys on the free-form `app_name` D-Bus hint provided by the client. This string is not standardized and applications may change their reported name across versions, locales, or platform rebuilds.

**Impact:**  
A user configures a rule for an app reporting `app_name="ExampleApp"`. The app is updated and begins reporting `app_name="Example App v2.0"`. The existing rule will no longer apply, and the user will need to reconfigure it or create a new rule for the new app name.

**Mitigation (out of scope for this spec):**  
Future enhancements could match on binary name, window class, or Desktop Entry ID instead of or in addition to `app_name`, but this is not in scope for notification-hardening.

---

## Acceptance Test Matrix

| Feature | Test Case | Pass/Fail |
|---------|-----------|-----------|
| DND Mode | Toggle DND on; suppress non-critical notification | |
| DND Mode | DND on; critical (urgency=2) notification displays | |
| DND Mode | DND setting not preserved after restart | |
| Per-App Rules | New app auto-added to rules list on first notification | |
| Per-App Rules | Disable app in rules; notification suppressed | |
| Per-App Rules | Set urgency filter to "suppress normal"; normal notifications blocked, critical allowed | |
| Per-App Rules | Suppressed notification does not appear in history | |
| Per-App Rules | Per-app rule disabled; critical notification still displays | |
| Per-App Rules | Rules persist within session but not across restarts | |
| Critical Notifications | Critical notification styled with warning color | |
| Critical Notifications | Critical notification has no auto-dismiss timeout | |
| Critical Notifications | Critical notification dismissible only by user action | |
| Daemon Detection | Another daemon running; diagnostic displayed in sidebar | |
| Daemon Detection | Shell continues to run despite daemon conflict | |
| Daemon Detection | Diagnostic message is clear and actionable | |
| Delivery & Rules | Non-critical notification passes all checks; displays | |
| Delivery & Rules | DND and rules apply to new notifications only; history unchanged | |


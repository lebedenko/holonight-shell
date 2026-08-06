# SPEC — idle-management

## Overview

The `idle-management` feature tracks user inactivity and provides idle inhibition capabilities for the holonight-shell. The shell claims the well-known D-Bus name `org.freedesktop.ScreenSaver` to implement the freedesktop idle protocol, allowing third-party applications (e.g., Teams, Zoom, Slack) to query the session idle time and request idle inhibition. Internally, the shell uses the `ext-idle-notify-v1` Wayland protocol to track the last user activity timestamp, exposes a configurable idle threshold (default 5 minutes) that controls when services like WeatherService and CalendarService pause polling, and provides a sidebar quick-action toggle ("Keep Awake") to acquire system inhibitors via logind.

At startup, the shell detects whether `hypridle` or `swayidle` is running; if neither is found, a single notification alerts the user that automatic screen lock and dimming won't work.

---

## Requirements

### Functional

#### REQ-F-001: IdleBackend shall be an abstract C++ interface
**Requirement:** The system shall define an abstract `IdleBackend` class with a pure virtual method `getSessionIdleTimeSeconds() → uint` and a signal `idleThresholdExceeded(bool idle)`. Concrete implementations shall inherit and override these members.

**Acceptance Criterion:**
- A header file declares `class IdleBackend : public QObject` with a pure virtual method
- All concrete backends (ExtIdleNotifyBackend, NullIdleBackend) derive from IdleBackend
- The interface compiles without instantiation errors

---

#### REQ-F-002: ExtIdleNotifyBackend shall subscribe to ext-idle-notify-v1 Wayland protocol
**Requirement:** The system shall create a concrete `ExtIdleNotifyBackend` that subscribes to the `ext-idle-notify-v1` Wayland protocol with a 1-second idle threshold, and record the timestamp of the last user activity.

**Acceptance Criterion:**
- The `ExtIdleNotifyBackend` constructor creates a `zwp_ext_idle_notify_v1` listener
- The listener fires at the 1-second idle mark and on user input resume
- `getSessionIdleTimeSeconds()` returns `now − last_activity_timestamp`, rounded down to the nearest integer
- The timestamp updates within 1 second of keyboard or pointer input

---

#### REQ-F-003: NullIdleBackend shall return zero idle time
**Requirement:** If the Wayland compositor does not support `ext-idle-notify-v1`, the system shall instantiate a `NullIdleBackend` that always returns 0 from `getSessionIdleTimeSeconds()`.

**Acceptance Criterion:**
- On a compositor without `ext-idle-notify-v1` support, NullIdleBackend is instantiated without errors
- `getSessionIdleTimeSeconds()` always returns 0
- No Wayland protocol errors or connection failures are logged

---

#### REQ-F-004: IdleService shall track idle state with a configurable threshold
**Requirement:** The `IdleService` C++ singleton shall expose a `Q_PROPERTY int idleThresholdMs` (readable, writable, notified) with default value 300000 (5 minutes), and emit a `idleChanged(bool idle)` signal when the seat idle time exceeds this threshold.

**Acceptance Criterion:**
- `idleThresholdMs` defaults to 300000 (5 minutes)
- `idleChanged(false)` fires within 1 second of user input when previously idle
- `idleChanged(true)` fires within 1 second after `idleThresholdMs` has elapsed with no activity
- The threshold is adjustable at runtime without restarting the shell

---

#### REQ-F-005: IdleService shall expose idle state to QML
**Requirement:** The `IdleService` shall expose a read-only Q_PROPERTY `bool isIdle` that reflects the current idle state, updated by the `idleChanged` signal.

**Acceptance Criterion:**
- QML bindings to `IdleService.isIdle` reflect the true idle state within 1 second of state change
- Initial value is false at startup

---

#### REQ-F-006: IdleService shall expose idle backend availability
**Requirement:** The `IdleService` shall expose a read-only Q_PROPERTY `bool idleBackendAvailable` that is true when using `ExtIdleNotifyBackend` and false when using `NullIdleBackend`.

**Acceptance Criterion:**
- On a compositor with `ext-idle-notify-v1`, `idleBackendAvailable` is true
- On a compositor without it, `idleBackendAvailable` is false
- The value is set during IdleService initialization and does not change at runtime

---

#### REQ-F-007: IdleService shall expose idle daemon detection status
**Requirement:** The `IdleService` shall expose a read-only Q_PROPERTY `bool idleDaemonDetected` that reflects whether `hypridle` or `swayidle` is running at startup.

**Acceptance Criterion:**
- If at least one of `hypridle` or `swayidle` is running, `idleDaemonDetected` is true
- If neither is running, `idleDaemonDetected` is false
- The property is set during IdleService initialization (at shell startup)

---

#### REQ-F-008: ScreenSaver D-Bus service shall implement GetSessionIdleTime
**Requirement:** The system shall claim the D-Bus well-known name `org.freedesktop.ScreenSaver` and expose an `org.freedesktop.ScreenSaver` interface at object path `/org/freedesktop/ScreenSaver` with a synchronous method `GetSessionIdleTime() → uint` that returns the session idle time in seconds.

**Acceptance Criterion:**
- `qdbus org.freedesktop.ScreenSaver /org/freedesktop/ScreenSaver GetSessionIdleTime` returns a uint32 value
- The return value increases by approximately 1 each second while idle
- The return value resets to near-zero within 1 second of keyboard or pointer activity

---

#### REQ-F-009: ScreenSaver D-Bus service shall emit ActiveChanged signal
**Requirement:** The D-Bus interface shall emit a signal `ActiveChanged(bool active)` when idle state changes at the `idleThresholdMs` threshold. The signal shall fire once per state transition (not continuously).

**Acceptance Criterion:**
- `ActiveChanged(true)` fires exactly once after `idleThresholdMs` elapses with no user input
- `ActiveChanged(false)` fires exactly once within 1 second of user input when the seat was idle
- A D-Bus client subscribing to the signal receives both transitions during a test session

---

#### REQ-F-010: ScreenSaver D-Bus service shall implement Inhibit method
**Requirement:** The D-Bus interface shall expose a method `Inhibit(app_name: string, reason: string) → cookie: uint` that apps call to inhibit session idle. The shell shall hold a logind inhibitor on the caller's behalf while the cookie is outstanding, and return a unique uint32 cookie to the caller.

**Acceptance Criterion:**
- An external app calls `Inhibit("MyApp", "Playing video")` and receives a uint32 cookie
- While the cookie is outstanding, the shell holds a logind inhibitor
- Multiple concurrent calls from different apps each return unique cookies
- Each cookie is a valid uint32 (non-zero)

---

#### REQ-F-011: ScreenSaver D-Bus service shall implement UnInhibit method
**Requirement:** The D-Bus interface shall expose a method `UnInhibit(cookie: uint)` that apps call to release a previously-issued inhibit cookie. When the last outstanding cookie is released, the shell shall release the logind inhibitor.

**Acceptance Criterion:**
- An app calls `Inhibit()`, receives cookie N, then calls `UnInhibit(N)` without error
- The logind inhibitor is released within 1 second of the last `UnInhibit()` call
- Calling `UnInhibit()` with an invalid cookie logs a warning without crashing
- If multiple apps hold cookies, the inhibitor is released only after all cookies are released

---

#### REQ-F-012: ScreenSaver D-Bus service shall handle name conflict gracefully
**Requirement:** If another D-Bus service already owns the `org.freedesktop.ScreenSaver` name at startup, the shell shall log a warning and proceed without registering; the shell shall not crash, block, or retry indefinitely.

**Acceptance Criterion:**
- If another process holds the name, the shell logs exactly one warning message
- The shell continues to initialize other services and display the UI
- The shell does not attempt to re-claim the name during runtime

---

#### REQ-F-013: Logind inhibitor shall be held via file descriptor
**Requirement:** The system shall use `org.freedesktop.login1.Manager.Inhibit(what="idle:sleep", who="HoloNight Shell", why=<reason>, mode="block")` to acquire an inhibitor and hold its returned file descriptor. Closing the fd shall release the inhibit.

**Acceptance Criterion:**
- A call to logind's Inhibit method returns a valid file descriptor (integer > 0)
- Holding the fd prevents the seat from entering idle at the logind level
- Closing the fd releases the inhibit within 1 second
- Multiple concurrent inhibitors are tracked independently

---

#### REQ-F-014: Sidebar shall display Keep Awake quick-action toggle
**Requirement:** The sidebar shall expose a quick-action toggle labeled "Keep Awake" that acquires/releases a logind inhibitor when toggled on/off.

**Acceptance Criterion:**
- A toggle button or switch labeled "Keep Awake" is visible in the sidebar
- Toggling on acquires a logind inhibitor with reason "User-requested" via HoloNight Shell's D-Bus method
- Toggling off closes the inhibitor file descriptor
- The toggle state persists until explicitly changed by the user
- The toggle is visually distinct when active (on)

---

#### REQ-F-015: WeatherService shall pause polling while idle
**Requirement:** `WeatherService` shall connect to `IdleService::idleChanged` and pause periodic polling when idle=true. When idle=false, it shall immediately request a refresh and resume periodic polling.

**Acceptance Criterion:**
- While `IdleService.isIdle` is true, WeatherService does not send HTTP requests to the weather API
- Transitioning to idle cancels any pending network request gracefully
- Within 1 second of transitioning to idle=false, WeatherService fetches fresh weather data
- Periodic polling resumes at the configured interval (e.g., 10 minutes) after the refresh

---

#### REQ-F-016: CalendarService shall pause polling while idle
**Requirement:** `CalendarService` shall connect to `IdleService::idleChanged` and pause periodic polling when idle=true. When idle=false, it shall immediately request a sync and resume periodic polling.

**Acceptance Criterion:**
- While `IdleService.isIdle` is true, CalendarService does not send HTTP/DAV requests
- Transitioning to idle cancels any pending sync gracefully
- Within 1 second of transitioning to idle=false, CalendarService syncs all calendar providers
- Periodic polling resumes at the configured interval after the sync

---

#### REQ-F-017: Shell shall detect hypridle or swayidle at startup
**Requirement:** At startup, the system shall scan running processes to detect whether `hypridle` or `swayidle` is running. This scan shall use a pattern reused from the session-lock-backend's `Locker` class (e.g., `CommandRunner` / `ProcessEnvironment` seams).

**Acceptance Criterion:**
- A process scan executes synchronously or via a background thread before other service initialization
- If `hypridle` is found in the process list, `idleDaemonDetected` is set to true
- If `swayidle` is found, `idleDaemonDetected` is set to true
- If neither is found, `idleDaemonDetected` is set to false
- The scan handles edge cases gracefully (missing /proc, permission denied, etc.)

---

#### REQ-F-018: Shell shall emit notification if idle daemon missing
**Requirement:** If `idleDaemonDetected` is false (neither `hypridle` nor `swayidle` running), the system shall emit exactly one `NotificationService` notification at startup with summary "No idle daemon detected" and body "Automatic screen lock and screen dimming won't work. Install and start hypridle or swayidle."

**Acceptance Criterion:**
- If neither daemon is running, a notification appears once during shell startup
- The notification has the exact summary and body as specified
- No additional notifications are fired on subsequent idle events
- If a daemon is detected, no notification is fired

---

#### REQ-F-019: IdleService shall track session lock state
**Requirement:** The `IdleService` shall subscribe to the D-Bus property `org.freedesktop.login1.Session.LockedHint` on the user's session object and expose a read-only Q_PROPERTY `bool sessionLocked` that reflects this hint.

**Acceptance Criterion:**
- `sessionLocked` is true when logind's LockedHint property is true
- `sessionLocked` is false when logind's LockedHint property is false
- The property updates within 1 second of a logind change
- The initial value is correct at startup (before and after lock)

---

#### REQ-F-020: BatteryService shall not connect to idle signal
**Requirement:** `BatteryService` shall not listen to idle state changes and shall continue updating battery status at its normal rate regardless of idle state.

**Acceptance Criterion:**
- BatteryService polls UPower or responds to D-Bus property changes independent of idle state
- Battery percent, charging state, and presence are always current
- No idle-related code appears in BatteryService implementation

---

#### REQ-F-021: NetworkService shall not connect to idle signal
**Requirement:** `NetworkService` shall not listen to idle state changes and shall continue updating network status at its normal rate regardless of idle state.

**Acceptance Criterion:**
- NetworkService queries NetworkManager independent of idle state
- WiFi SSID, signal strength, and connection state are always current
- No idle-related code appears in NetworkService implementation

---

#### REQ-F-022: NotificationService shall not connect to idle signal
**Requirement:** `NotificationService` shall not listen to idle state changes and shall continue to deliver and dismiss notifications regardless of idle state.

**Acceptance Criterion:**
- Notifications are delivered while idle and while active
- Notification dismissal timers run independently of idle state
- No idle-related code appears in NotificationService implementation

---

### Non-Functional

#### REQ-NF-001: IdleBackend shall track idle time without polling
**Requirement:** The `IdleBackend` implementation shall subscribe to Wayland `ext-idle-notify-v1` events rather than polling the system clock, to minimize CPU usage while idle.

**Acceptance Criterion:**
- ExtIdleNotifyBackend registers a Wayland event listener on initialization
- No `QTimer` or polling loop exists in the idle tracking code
- CPU usage remains constant while idle (no active polling)

---

#### REQ-NF-002: D-Bus name claim shall not block startup
**Requirement:** Claiming the `org.freedesktop.ScreenSaver` D-Bus name shall not block the main Qt event loop or delay shell initialization.

**Acceptance Criterion:**
- The name is claimed asynchronously or in a background task
- The shell displays the top bar and sidebar within 500 ms even if D-Bus is slow
- Name-claim failures do not prevent shell startup

---

#### REQ-NF-003: Logind inhibitor operations shall handle errors gracefully
**Requirement:** Calls to logind's `Inhibit()` and `UnInhibit()` methods shall handle D-Bus errors (e.g., connection loss, method not found) without crashing or blocking the UI.

**Acceptance Criterion:**
- If logind is unavailable, a warning is logged and the operation is skipped
- The shell continues to run and other services remain functional
- Attempting to toggle "Keep Awake" when logind is unavailable shows a user-facing error message or is disabled

---

#### REQ-NF-004: Wayland protocol absence shall not crash
**Requirement:** If the Wayland compositor does not advertise `ext-idle-notify-v1`, the system shall fall back to `NullIdleBackend` without crashing, logging errors, or stalling.

**Acceptance Criterion:**
- On a Wayland server without the protocol, NullIdleBackend is instantiated
- No protocol errors or missing-interface warnings are logged
- The shell continues to run with `idleBackendAvailable = false`

---

#### REQ-NF-005: Code shall pass all linting and formatting checks
**Requirement:** All C++ and QML code for the idle-management feature shall pass `task format-check`, `task qml-lint` (for any QML components), and `task tidy` without errors or warnings.

**Acceptance Criterion:**
- `task build` completes without compiler warnings
- `task format-check` reports the code is correctly formatted
- `task tidy` reports zero new warnings in idle-management source files
- Any existing warnings in unmodified files are not attributed to this feature

---

#### REQ-NF-006: Unit tests shall not require a live Wayland session
**Requirement:** GTest unit tests for `IdleBackend`, `IdleService`, and D-Bus methods shall run offline (in a test environment without a live Wayland compositor).

**Acceptance Criterion:**
- Tests mock the Wayland protocol via a test backend or fixture
- Tests mock D-Bus services via `QDBusTestUtil` or similar testing utilities
- All unit tests pass in a headless CI environment
- Coverage includes happy paths, edge cases, and error conditions

---

### Constraints

#### REQ-C-001: IdleBackend shall use ext-idle-notify-v1 protocol
**Requirement:** The concrete idle-tracking implementation shall subscribe to the `ext-idle-notify-v1` Wayland protocol exclusively; no alternative idle-tracking APIs (e.g., `/sys/class/power_supply`, logind IdleHint, X11 XScreenSaver) shall be used.

**Acceptance Criterion:**
- The only Wayland protocol instantiated for idle tracking is `ext-idle-notify-v1`
- No system files or alternative D-Bus properties are queried for idle time
- The implementation compiles with `wlroots` compositors (Hyprland, Sway, Niri)

---

#### REQ-C-002: ScreenSaver interface shall conform to freedesktop specification
**Requirement:** The D-Bus interface `org.freedesktop.ScreenSaver` and its methods shall conform to the freedesktop.org ScreenSaver D-Bus specification.

**Acceptance Criterion:**
- Method signatures match the specification (e.g., `GetSessionIdleTime() → uint32`)
- Signal names and argument types match the specification (e.g., `ActiveChanged(bool)`)
- Third-party apps (Teams, Zoom, Slack) recognize and use the interface without modification

---

#### REQ-C-003: Inhibit/UnInhibit cookies shall be uint32
**Requirement:** Inhibit cookies shall be unique uint32 values. Repeated calls to `Inhibit()` from the same app or different apps shall return different cookies.

**Acceptance Criterion:**
- Each `Inhibit()` call returns a unique uint32 value
- Cookies never repeat during a shell session (or repeat only after restart)
- `UnInhibit(cookie)` expects a uint32 argument

---

#### REQ-C-004: Logind inhibitor shall use hardcoded what="idle:sleep"
**Requirement:** All calls to `org.freedesktop.login1.Manager.Inhibit()` shall use `what="idle:sleep"` to inhibit both screen blank and system sleep.

**Acceptance Criterion:**
- The logind Inhibit call always passes `what="idle:sleep"` (no variation)
- Both idle timeout and sleep are prevented while the inhibitor is held

---

#### REQ-C-005: IdleService shall be a QML singleton
**Requirement:** The `IdleService` shall be exposed to QML via `qmlRegisterSingletonInstance<IdleService>()` in the C++ main function, making it globally accessible as `IdleService.<property>`.

**Acceptance Criterion:**
- QML components can access idle state via `import HolonightShell` and `IdleService.isIdle`
- Only one instance of IdleService exists throughout the application lifetime
- Properties are writable and emit notifications for bindings

---

#### REQ-C-006: Idle threshold shall be configurable
**Requirement:** The idle threshold (default 300000 ms = 5 minutes) shall be a writable Q_PROPERTY on `IdleService`, allowing adjustment at runtime.

**Acceptance Criterion:**
- A settings module can modify `IdleService.idleThresholdMs` to change the threshold
- The change takes effect immediately (next idle detection uses the new value)
- Default value is 300000 ms if not overridden

---

#### REQ-C-007: Session lock detection shall use logind LockedHint
**Requirement:** The `sessionLocked` property shall reflect `org.freedesktop.login1.Session.LockedHint` via D-Bus subscription, not via polling or alternative lock-detection mechanisms.

**Acceptance Criterion:**
- IdleService subscribes to logind Session properties
- Changes to LockedHint are received via D-Bus PropertyChanged signals
- No polling loop or alternative mechanism is used

---

#### REQ-C-008: Process detection shall reuse session-lock-backend pattern
**Requirement:** Process detection for `hypridle` / `swayidle` shall reuse the `CommandRunner` and `ProcessEnvironment` seams from the existing session-lock-backend to enable test injection.

**Acceptance Criterion:**
- Idle-daemon detection uses the same `CommandRunner` interface as session-lock-backend
- Unit tests can inject a mock CommandRunner to simulate daemon presence/absence
- No direct `/proc` parsing or shell commands are hardcoded in production code

---

#### REQ-C-009: ext-idle-notify-v1.xml shall be added to protocols directory
**Requirement:** The `ext-idle-notify-v1.xml` protocol definition shall be added to the `protocols/` directory and compiled into client headers via `wayland-scanner` in CMakeLists.txt.

**Acceptance Criterion:**
- File `protocols/ext-idle-notify-v1.xml` exists and is well-formed XML
- CMakeLists.txt invokes `wayland-scanner client-header` and `client-code` on this file
- Generated headers `ext-idle-notify-v1-client.h` and `ext-idle-notify-v1-client.c` are produced
- The shell builds without missing headers or linker errors

---

#### REQ-C-010: No generation or modification of hypridle/swayidle configs
**Requirement:** The idle-management feature shall not generate, modify, or validate `hypridle.conf` or `swayidle.conf` files.

**Acceptance Criterion:**
- No file write operations target config directories or home directories
- Config file paths are not hardcoded or scanned for validation
- The notification is purely informational; the user must install and configure these daemons manually

---

#### REQ-C-011: No lock screen or screen dimmer implementation
**Requirement:** The idle-management feature shall not implement a lock screen, screen dimmer, or any visual state change tied to idle time (beyond the `Keep Awake` toggle visibility).

**Acceptance Criterion:**
- No new window or overlay is created in response to idle state
- The shell does not dim, blank, or modify the display
- Lock screen requests are delegated to external daemons (hypridle → hyprlock)

---

#### REQ-C-012: No SimulateUserActivity or Lock stubs
**Requirement:** The D-Bus interface shall not implement `SimulateUserActivity()` or `Lock()` methods; these methods are optional in the freedesktop spec and not required for Teams/Zoom compatibility.

**Acceptance Criterion:**
- The D-Bus introspection XML lists only `GetSessionIdleTime()` and `Inhibit()`/`UnInhibit()`
- Apps calling `SimulateUserActivity()` or `Lock()` receive a "method not found" D-Bus error
- This does not affect Teams/Zoom/Slack idle behavior

---

#### REQ-C-013: No Sway/Niri/generic compositor-specific backends
**Requirement:** The idle tracking implementation shall use `ext-idle-notify-v1` exclusively and require no compositor-specific branches or fallback implementations (beyond NullIdleBackend for missing protocol support).

**Acceptance Criterion:**
- The code contains no Sway-specific, Niri-specific, or compositor-detection logic
- ExtIdleNotifyBackend works on all wlroots compositors (Hyprland, Sway, Niri)
- NullIdleBackend is the only fallback for protocol absence

---

#### REQ-C-014: SessionLocked property is read-only
**Requirement:** The `sessionLocked` Q_PROPERTY shall be read-only (no setter); it is updated exclusively via D-Bus subscription to logind.

**Acceptance Criterion:**
- No `setSessionLocked()` method exists
- QML and C++ cannot modify the property directly
- Changes propagate from logind LockedHint via D-Bus PropertyChanged signals

---

## Non-Goals

The following features are explicitly out of scope for this pipeline:

1. **Lock screen or screen dimmer**: Automatic screen blank or dimming is delegated to `hypridle` → `hyprlock`. This feature only detects idle; it does not perform visual actions.

2. **Listing inhibiting apps**: The D-Bus interface does not expose which external apps are currently inhibiting idle. Only the aggregate inhibitor state matters (inhibit active or inactive).

3. **Settings UI for idle threshold**: The `idleThresholdMs` property is writable and can be set from QML, but no dedicated settings page is included in this pipeline. Settings integration is deferred to a future UI feature.

4. **Lock screen protocol implementation**: The shell does not implement `org.freedesktop.portal.Lockscreen` or provide its own lock-screen surface. This responsibility belongs to `hyprlock` or other external lockers.

5. **Alternative idle-tracking mechanisms**: No X11 XScreenSaver, `/sys/class/power_supply` scanning, or logind IdleHint polling is used. Only `ext-idle-notify-v1` is implemented.

6. **Daemon management**: The shell does not start, stop, or restart `hypridle` or `swayidle`. Detection is informational only.

7. **Activity simulation**: The `SimulateUserActivity()` D-Bus method (if present) is not implemented; third-party activity injection is not supported.

---

## Testing Strategy

### Unit Testing

**IdleBackend abstraction:**
- Mock `IdleBackend` implementation for testing `IdleService` without a live Wayland session
- Test `getSessionIdleTimeSeconds()` return values and threshold crossing logic
- Verify signal emission at idle transitions

**D-Bus ScreenSaver interface:**
- Mock D-Bus environment (QDBusTestUtil or similar)
- Test `GetSessionIdleTime()` method and return value correctness
- Test `ActiveChanged` signal firing at the threshold
- Test `Inhibit()` / `UnInhibit()` cookie generation and release
- Test D-Bus name conflict handling (graceful fallback, no crash)

**Process detection:**
- Mock `CommandRunner` to simulate presence/absence of `hypridle` and `swayidle`
- Verify correct notification firing based on daemon detection result
- Verify `idleDaemonDetected` property is set correctly

**Service pause/resume:**
- Mock `IdleService` idle signal
- Test WeatherService pauses polling on `idle=true` and resumes on `idle=false`
- Test CalendarService pauses polling on `idle=true` and resumes on `idle=false`
- Verify immediate refresh requests upon resuming

**Logind inhibitor:**
- Mock logind D-Bus service
- Test file descriptor acquisition and release via `Inhibit()` / `UnInhibit()`
- Test multiple concurrent inhibitors
- Verify inhibitor release only occurs when last cookie is released

### Integration Testing

**Idle threshold crossing:**
- On a live Hyprland session with `ext-idle-notify-v1` support, verify idle state transitions at the configured threshold
- Confirm `ActiveChanged` signal fires at transitions
- Confirm `GetSessionIdleTime()` returns correct seconds

**Third-party app compatibility:**
- Run Teams, Zoom, or Slack on a live Hyprland session
- Verify these apps call `GetSessionIdleTime()` without errors
- Confirm apps transition to "Away" after their configured idle timeout
- Verify app auto-away is reset when `GetSessionIdleTime()` returns near-zero

**Sidebar Keep Awake toggle:**
- Toggle "Keep Awake" on and verify logind inhibitor is held (system stays awake)
- Toggle "Keep Awake" off and verify inhibitor is released
- Confirm idle state transitions are blocked while toggle is on

**Notification on startup:**
- Stop `hypridle` and `swayidle`, then restart the shell
- Verify notification "No idle daemon detected" appears once
- Restart the shell with at least one daemon running and verify no notification fires

**Service pause/resume on live session:**
- Idle for > 5 minutes and verify WeatherService and CalendarService stop polling
- Move mouse or type a key and verify both services immediately sync and resume polling
- Verify BatteryService, NetworkService, and NotificationService are unaffected

### Acceptance Criteria Checklist

- [ ] `qdbus org.freedesktop.ScreenSaver /org/freedesktop/ScreenSaver GetSessionIdleTime` returns increasing seconds while idle
- [ ] Teams/Zoom/Slack transitions to "Away" after idle threshold on live Hyprland
- [ ] Sidebar "Keep Awake" toggle holds logind inhibitor while on
- [ ] WeatherService and CalendarService stop polling after 5 min idle, immediately re-sync on resume
- [ ] Notification fires once at startup if `hypridle` and `swayidle` are both absent
- [ ] `task build` completes without warnings
- [ ] `task tidy` reports zero new warnings in idle-management code
- [ ] `task format-check` passes all idle-management files
- [ ] All unit tests pass in headless CI (no live Wayland required)

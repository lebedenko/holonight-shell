# Idle Management — Architecture Design

## 1. Overview

The idle-management feature adds two capabilities to holonight-shell: (1) a Wayland-native idle
tracker that records when the user's seat has been inactive, and (2) a freedesktop ScreenSaver
D-Bus service that lets third-party applications (Teams, Zoom, Slack) query that idle time and
request idle inhibition. Internally, `IdleService` is a QML singleton that exposes `isIdle`,
`idleThresholdMs`, `sessionLocked`, and related properties; `WeatherService` and `CalendarService`
connect to its `idleChanged` signal to pause network polling while the seat is idle. Idle
inhibition by external apps is handled by `ScreenSaverAdaptor`, which owns a cookie table and
delegates to `IdleInhibitor` for logind fd management. A sidebar "Keep Awake" quick-action toggle
lets the user acquire a logind inhibitor directly. At startup, `IdleService` scans for `hypridle`
or `swayidle` using the same `ProcessEnvironment` seam already present in the session-lock-backend;
if neither is found, it posts a single `NotificationService` notification. The concrete
`ExtIdleNotifyBackend` subscribes to the `ext-idle-notify-v1` Wayland protocol at a 1-second
threshold to track the last activity timestamp; on compositors without the protocol, `NullIdleBackend`
substitutes transparently.

---

## 2. Component Map

### C++ classes

| Class | File | Responsibility |
|---|---|---|
| `IdleBackend` | `src/services/idle/IdleBackend.h` | Abstract base. Declares `getSessionIdleTimeSeconds()` and the `idleThresholdExceeded(bool)` signal. All concrete backends derive from it. |
| `ExtIdleNotifyBackend` | `src/services/idle/ExtIdleNotifyBackend.h/.cpp` | Subscribes to `ext-idle-notify-v1` at a 1-second threshold. Captures `last_activity_` on `resumed` events; emits `idleThresholdExceeded` when the threshold crosses. Implements `getSessionIdleTimeSeconds()` as `now − last_activity_`. |
| `NullIdleBackend` | `src/services/idle/NullIdleBackend.h/.cpp` | Used when the compositor does not advertise `ext-idle-notify-v1`. `getSessionIdleTimeSeconds()` always returns 0. Never emits `idleThresholdExceeded`. |
| `IdleService` | `src/services/idle/IdleService.h/.cpp` | QML singleton. Owns the backend. Exposes `isIdle`, `idleThresholdMs`, `idleBackendAvailable`, `idleDaemonDetected`, and `sessionLocked` as `Q_PROPERTY`. Emits `idleChanged(bool)`. Subscribes to logind `LockedHint` for `sessionLocked`. Scans for idle daemons at construction via `ProcessEnvironment`. Posts the "No idle daemon" notification if neither is found. |
| `ScreenSaverAdaptor` | `src/services/idle/ScreenSaverAdaptor.h/.cpp` | Claims `org.freedesktop.ScreenSaver` on the session bus. Implements `GetSessionIdleTime()`, `Inhibit()`, `UnInhibit()`, and emits `ActiveChanged(bool)`. Maintains the cookie table; delegates inhibitor fd lifecycle to `IdleInhibitor`. Forwards idle-time queries to `IdleService`. |
| `IdleInhibitor` | `src/services/idle/IdleInhibitor.h/.cpp` | Manages a single logind inhibitor fd. Calls `org.freedesktop.login1.Manager.Inhibit(what="idle:sleep", ...)` to acquire the fd; closes it to release. Ref-counted against the cookie set owned by `ScreenSaverAdaptor` — acquired on the first active cookie, released when the last cookie is removed. |

### Wayland protocol

| File | Responsibility |
|---|---|
| `protocols/ext-idle-notify-v1.xml` | Protocol definition for `ext-idle-notify-v1`. Added to `protocols/` and compiled by `qt6_generate_wayland_protocol_client_sources` into `qwayland-ext-idle-notify-v1.h/.cpp` and `wayland-ext-idle-notify-v1-client-protocol.h/.c`. |

### QML component

| File | Responsibility |
|---|---|
| `src/qml/RightSidebar/KeepAwakeAction.qml` | Sidebar quick-action toggle. Reads `IdleService.idleInhibited` (a writable `Q_PROPERTY` on `IdleService` that drives the user-facing logind inhibitor) and toggles it. Visually distinct when active. |

---

## 3. Data Flow

### 3.1 Idle detection flow

```
Compositor
    │  ext-idle-notify-v1: idle event (1s threshold elapsed)
    ▼
ExtIdleNotifyBackend
    │  records last_activity_ = now − 1s (implied by idle event)
    │  emits idleThresholdExceeded(true)  ← only when threshold_ms has passed
    ▼
IdleService::onThresholdExceeded(bool idle)
    │  sets is_idle_ = idle
    │  emits idleChanged(idle)
    ├──▶ WeatherService::onIdleChanged(true)   → stops refresh_timer_
    └──▶ CalendarService::onIdleChanged(true)  → stops sync timer

Compositor
    │  ext-idle-notify-v1: resumed event (input received)
    ▼
ExtIdleNotifyBackend
    │  records last_activity_ = now
    │  emits idleThresholdExceeded(false)
    ▼
IdleService::onThresholdExceeded(false)
    │  sets is_idle_ = false
    │  emits idleChanged(false)
    ├──▶ WeatherService::onIdleChanged(false)  → immediate fetch(); restarts refresh_timer_
    └──▶ CalendarService::onIdleChanged(false) → immediate sync(); restarts sync timer
```

`ExtIdleNotifyBackend` registers the `ext_idle_notification_v1` at the 1-second mark. The
`idleThresholdExceeded` signal is only emitted when `getSessionIdleTimeSeconds() * 1000 >=
idleThresholdMs`, preventing the 1-second protocol threshold from propagating to consumers as
rapid-fire state flips. `IdleService` performs this check in `onThresholdExceeded` and guards
against redundant transitions (emits only on actual state change).

### 3.2 ScreenSaver query flow

```
External app (e.g. Teams)
    │  D-Bus call: GetSessionIdleTime()
    ▼
ScreenSaverAdaptor::GetSessionIdleTime()
    │  delegates to IdleService::getIdleTimeSeconds()
    ▼
IdleService::getIdleTimeSeconds()
    │  delegates to backend_->getSessionIdleTimeSeconds()
    ▼
ExtIdleNotifyBackend::getSessionIdleTimeSeconds()
    │  returns static_cast<uint>(now − last_activity_)  // in seconds
    ◀─────────────────────────────────────────────────────
Return value flows back up to the external app as uint32.
```

When `idleChanged(true)` fires (threshold crossed), `ScreenSaverAdaptor` emits `ActiveChanged(true)`
on the D-Bus interface. On `idleChanged(false)`, it emits `ActiveChanged(false)`. This pair of
signals is what Teams/Zoom observe to transition to and from "Away" state — they do not poll
`GetSessionIdleTime()` continuously; they watch the signal.

### 3.3 Inhibit flow

```
External app
    │  D-Bus call: Inhibit("MyApp", "Playing video")
    ▼
ScreenSaverAdaptor::Inhibit(app_name, reason)
    │  generates cookie = ++next_cookie_  (uint32, monotone)
    │  stores cookie in inhibit_cookies_ map
    │  if inhibit_cookies_.size() == 1: IdleInhibitor::acquire(reason)
    │      └─▶ logind Manager.Inhibit(what="idle:sleep", who="HoloNight Shell",
    │                                   why=reason, mode="block") → fd
    │          stores inhibit_fd_ (QDBusPendingCallWatcher pattern)
    ◀─ returns cookie to caller
    │
    │  D-Bus call: UnInhibit(cookie)
    ▼
ScreenSaverAdaptor::UnInhibit(cookie)
    │  removes cookie from inhibit_cookies_
    │  if inhibit_cookies_.isEmpty(): IdleInhibitor::release()
    │      └─▶ closes inhibit_fd_  → logind auto-releases on fd close
```

The user-facing "Keep Awake" toggle in `KeepAwakeAction.qml` writes `IdleService.idleInhibited`.
`IdleService` holds its own cookie slot in `ScreenSaverAdaptor` (calling `Inhibit`/`UnInhibit` on
itself via a direct C++ call, not via D-Bus) or, simpler: it holds a dedicated `IdleInhibitor`
instance that it acquires/releases directly, separate from the external-app inhibitor managed by
`ScreenSaverAdaptor`. This keeps the two inhibitor paths independent and avoids self-D-Bus-call
complexity.

---

## 4. Interfaces & APIs

### 4.1 `IdleBackend` — abstract base

```cpp
// src/services/idle/IdleBackend.h
class IdleBackend : public QObject {
  Q_OBJECT
 public:
  virtual ~IdleBackend() = default;

  IdleBackend(const IdleBackend&) = delete;
  IdleBackend& operator=(const IdleBackend&) = delete;
  IdleBackend(IdleBackend&&) = delete;
  IdleBackend& operator=(IdleBackend&&) = delete;

  // Returns elapsed idle time in whole seconds: now − last_activity.
  // NullIdleBackend always returns 0.
  [[nodiscard]] virtual uint getSessionIdleTimeSeconds() const = 0;

 Q_SIGNALS:
  // Emitted once when the compositor reports the seat has been idle for the configured
  // threshold (idle=true) and once when the user resumes activity (idle=false).
  void idleThresholdExceeded(bool idle);

 protected:
  explicit IdleBackend(QObject* parent = nullptr);
};
```

### 4.2 `IdleService` — QML singleton

```cpp
// src/services/idle/IdleService.h
class IdleService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(bool isIdle READ isIdle NOTIFY idleChanged FINAL)
  Q_PROPERTY(int idleThresholdMs
             READ idleThresholdMs WRITE setIdleThresholdMs NOTIFY idleThresholdMsChanged FINAL)
  Q_PROPERTY(bool idleBackendAvailable READ idleBackendAvailable CONSTANT FINAL)
  Q_PROPERTY(bool idleDaemonDetected   READ idleDaemonDetected   CONSTANT FINAL)
  Q_PROPERTY(bool sessionLocked        READ sessionLocked         NOTIFY sessionLockedChanged FINAL)
  Q_PROPERTY(bool idleInhibited
             READ idleInhibited WRITE setIdleInhibited NOTIFY idleInhibitedChanged FINAL)

 public:
  explicit IdleService(const ProcessEnvironment* env,
                       NotificationService* notif,
                       QObject* parent = nullptr);
  // Test seam: inject a pre-built backend.
  explicit IdleService(std::unique_ptr<IdleBackend> backend,
                       bool daemon_detected,
                       QObject* parent = nullptr);
  ~IdleService() override;

  [[nodiscard]] bool isIdle() const { return is_idle_; }
  [[nodiscard]] int  idleThresholdMs() const { return idle_threshold_ms_; }
  [[nodiscard]] bool idleBackendAvailable() const;
  [[nodiscard]] bool idleDaemonDetected() const { return daemon_detected_; }
  [[nodiscard]] bool sessionLocked() const { return session_locked_; }
  [[nodiscard]] bool idleInhibited() const { return user_inhibited_; }
  void setIdleThresholdMs(int threshold_ms);
  void setIdleInhibited(bool inhibited);

  // Called by ScreenSaverAdaptor to answer GetSessionIdleTime().
  [[nodiscard]] uint getIdleTimeSeconds() const;

 Q_SIGNALS:
  void idleChanged(bool idle);
  void idleThresholdMsChanged();
  void sessionLockedChanged();
  void idleInhibitedChanged();

 private Q_SLOTS:
  void onThresholdExceeded(bool idle);
  void onLogindLockedHintChanged(bool locked);

 private:
  void connectBackend();
  void detectDaemon(const ProcessEnvironment* env);
  void subscribeLockedHint();
  void postDaemonNotification(NotificationService* notif) const;

  std::unique_ptr<IdleBackend> backend_;
  std::unique_ptr<IdleInhibitor> user_inhibitor_;  // held while idleInhibited_ == true
  bool is_idle_{false};
  int  idle_threshold_ms_{300'000};  // default: 5 minutes
  bool daemon_detected_{false};
  bool session_locked_{false};
  bool user_inhibited_{false};
};
```

`idleInhibited` is the writable property the "Keep Awake" QML toggle binds to. Setting it `true`
calls `user_inhibitor_->acquire()`; setting it `false` calls `user_inhibitor_->release()`. This
path does not go through `ScreenSaverAdaptor` and does not issue D-Bus calls to itself.

### 4.3 `ScreenSaverAdaptor` — D-Bus adaptor

```cpp
// src/services/idle/ScreenSaverAdaptor.h
// Q_CLASSINFO("D-Bus Interface", "org.freedesktop.ScreenSaver") is required so Qt
// exports methods under the correct interface name in introspection XML.
class ScreenSaverAdaptor : public QObject, protected QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.ScreenSaver")

 public:
  explicit ScreenSaverAdaptor(IdleService* idle_service, QObject* parent = nullptr);

  // Claims "org.freedesktop.ScreenSaver" on the session bus.
  // Returns false and logs a warning if the name is already owned (REQ-F-012).
  bool registerService();

 public Q_SLOTS:  // D-Bus method implementations
  uint GetSessionIdleTime();
  uint Inhibit(const QString& app_name, const QString& reason);
  void UnInhibit(uint cookie);

 Q_SIGNALS:
  void ActiveChanged(bool active);  // forwarded from IdleService::idleChanged

 private Q_SLOTS:
  void onIdleChanged(bool idle);

 private:
  IdleService* idle_service_;
  std::unique_ptr<IdleInhibitor> inhibitor_;
  QHash<uint, QString> inhibit_cookies_;  // cookie → app_name
  uint next_cookie_{0};
};
```

`QDBusContext` is inherited so the adaptor can identify the calling peer's bus name inside `Inhibit`
and `UnInhibit` for logging, following the pattern documented in CLAUDE.md. The `Q_CLASSINFO`
annotation is mandatory — without it Qt introspects slots under `local.ScreenSaverAdaptor` instead
of `org.freedesktop.ScreenSaver`.

### 4.4 `IdleInhibitor` — logind fd manager

```cpp
// src/services/idle/IdleInhibitor.h
class IdleInhibitor : public QObject {
  Q_OBJECT
 public:
  explicit IdleInhibitor(QObject* parent = nullptr);
  ~IdleInhibitor() override;

  IdleInhibitor(const IdleInhibitor&) = delete;
  IdleInhibitor& operator=(const IdleInhibitor&) = delete;
  IdleInhibitor(IdleInhibitor&&) = delete;
  IdleInhibitor& operator=(IdleInhibitor&&) = delete;

  // Calls logind Inhibit(what="idle:sleep", who="HoloNight Shell", why=reason, mode="block").
  // Stores the returned fd. No-op if already holding an fd.
  void acquire(const QString& reason);

  // Closes the fd, releasing the logind inhibitor. No-op if not holding.
  void release();

  [[nodiscard]] bool isHeld() const { return inhibit_fd_ >= 0; }

 private:
  int inhibit_fd_{-1};
};
```

`IdleInhibitor::acquire` issues a synchronous `QDBusInterface` call to
`org.freedesktop.login1 /org/freedesktop/login1 org.freedesktop.login1.Manager.Inhibit`. The reply
contains a Unix fd; Qt's D-Bus layer delivers it as `QDBusUnixFileDescriptor`. The raw fd is
extracted and stored as `inhibit_fd_`. On `release()`, `::close(inhibit_fd_)` is called and
`inhibit_fd_` is reset to -1. The destructor calls `release()` so the fd is never leaked even
on abnormal paths.

### 4.5 D-Bus interface summary

Object path: `/org/freedesktop/ScreenSaver`
Bus name: `org.freedesktop.ScreenSaver`

| Member | Kind | Signature | Notes |
|---|---|---|---|
| `GetSessionIdleTime` | method | `() → u` | Returns seconds since last user input. |
| `Inhibit` | method | `(ss) → u` | `(app_name, reason) → cookie`. Holds logind fd while cookie outstanding. |
| `UnInhibit` | method | `(u)` | `(cookie)`. Releases logind fd when last cookie removed. |
| `ActiveChanged` | signal | `(b)` | Emitted at `idleThresholdMs` threshold crossings only. |

`SimulateUserActivity` and `Lock` are not implemented (REQ-C-012). D-Bus introspection will list
only the four members above.

---

## 5. File Layout

```
protocols/
  ext-idle-notify-v1.xml               # new — Wayland protocol definition

src/services/idle/
  IdleBackend.h                        # abstract base class (QObject, pure virtual)
  IdleBackend.cpp                      # (may be header-only; .cpp if logging category needed)
  ExtIdleNotifyBackend.h
  ExtIdleNotifyBackend.cpp             # subscribes to ext-idle-notify-v1; timestamp tracking
  NullIdleBackend.h
  NullIdleBackend.cpp                  # always returns 0
  IdleService.h
  IdleService.cpp                      # QML singleton; backend factory; logind LockedHint sub
  ScreenSaverAdaptor.h
  ScreenSaverAdaptor.cpp               # D-Bus adaptor; cookie table; ActiveChanged relay
  IdleInhibitor.h
  IdleInhibitor.cpp                    # logind fd acquire/release

src/qml/RightSidebar/
  KeepAwakeAction.qml                  # new QML quick-action toggle (sidebar)
```

`ProcessEnvironment` and `CommandRunner` are not duplicated — `IdleService` receives a
`const ProcessEnvironment*` injected from `ShellApplication`, reusing the same instance already
owned by `SessionService`. No new seam classes are introduced.

---

## 6. CMake Integration

### 6.1 Wayland protocol scanner

`ext-idle-notify-v1.xml` is added to the existing `qt6_generate_wayland_protocol_client_sources`
call on `holonight_platform`. The protocol is a pure client-side tracker with no role assignment,
so it belongs alongside the other platform-layer Wayland protocols rather than inside
`holonight_services`.

```cmake
qt6_generate_wayland_protocol_client_sources(holonight_platform
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/protocols/ext-workspace-v1.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/protocols/ext-idle-notify-v1.xml   # new
        ${XDG_SHELL_PROTOCOL}
)
```

The generated files (`qwayland-ext-idle-notify-v1.h/.cpp` and
`wayland-ext-idle-notify-v1-client-protocol.h/.c`) land in `${CMAKE_CURRENT_BINARY_DIR}`, which
`holonight_platform` already adds to its private include path via `target_include_directories`.
`holonight_services` links `holonight_platform PUBLIC` and therefore sees the generated headers
transitively.

### 6.2 New source files in holonight_services

Add the following pairs to the `add_library(holonight_services STATIC ...)` block:

```cmake
    src/services/idle/IdleBackend.h
    src/services/idle/IdleBackend.cpp
    src/services/idle/ExtIdleNotifyBackend.h
    src/services/idle/ExtIdleNotifyBackend.cpp
    src/services/idle/NullIdleBackend.h
    src/services/idle/NullIdleBackend.cpp
    src/services/idle/IdleService.h
    src/services/idle/IdleService.cpp
    src/services/idle/ScreenSaverAdaptor.h
    src/services/idle/ScreenSaverAdaptor.cpp
    src/services/idle/IdleInhibitor.h
    src/services/idle/IdleInhibitor.cpp
```

Add the include directory:

```cmake
target_include_directories(holonight_services PUBLIC
    ...existing entries...
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/idle
)
```

### 6.3 Qt modules

`Qt6::DBus` is already linked by `holonight_services`. No new Qt modules are required.
`Qt6::WaylandClient` (via `holonight_qt_wayland_client`) is already linked by `holonight_platform`.

### 6.4 QML file registration

Add `src/qml/RightSidebar/KeepAwakeAction.qml` to `HOLONIGHT_QML_FILES` in `CMakeLists.txt`. The
list must remain alphabetically sorted (as enforced by the `list(SORT ...)` + equality check).
`KeepAwakeAction` sorts between `KeyboardLayoutWidget` and `LauncherActionRow`.

### 6.5 clang-tidy generated file exclusions

The two new generated Wayland headers must be added to `TIDY_GENERATED_FILES` in `CMakeLists.txt`
so clang-tidy does not attempt to lint them:

```cmake
set(TIDY_GENERATED_FILES
    ...existing entries...
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-ext-idle-notify-v1.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-ext-idle-notify-v1-client-protocol.h
)
```

---

## 7. Key Decisions with Rationale

**1-second `ext-idle-notify-v1` threshold for accurate `GetSessionIdleTime()`.**
Teams, Zoom, and Slack poll `GetSessionIdleTime()` or watch `ActiveChanged` to determine when to
mark users as "Away". If the Wayland notification threshold were set to 5 minutes (the service
pause threshold), `getSessionIdleTimeSeconds()` would only update in large jumps: it would either
report 0 (resumed) or a stale timestamp from 5 minutes ago. External apps computing elapsed time
relative to the returned value would be inaccurate. A 1-second threshold means the backend fires
its `resumed` event within 1 second of any keyboard or pointer input, so the returned value tracks
real elapsed time continuously. The `idleChanged` signal is still emitted only at `idleThresholdMs`
(default 5 minutes) — the 1s protocol threshold is invisible to consumers except via
`getSessionIdleTimeSeconds()`.

**`ScreenSaverAdaptor` as a separate class, not methods on `IdleService`.**
`IdleService` is a QML singleton that exposes idle state to the QML layer. Merging D-Bus adaptor
logic into it would make the class responsible for both QML property notification and D-Bus method
dispatch, violating single responsibility. More concretely: `QDBusContext` must be inherited
alongside `QObject` for sender-identity access; adding `protected QDBusContext` to a
`QML_SINGLETON` class introduces unnecessary coupling and may confuse the QML type registrar. As a
separate class, `ScreenSaverAdaptor` can be instantiated, tested, and replaced independently.
Unit tests for the cookie/inhibitor logic do not need a QML engine.

**`NullIdleBackend` instead of making `IdleService` optional.**
If `IdleService` were conditionally created only when `ext-idle-notify-v1` is available, every
QML binding to `IdleService.isIdle` would require a null-guard. More importantly,
`ScreenSaverAdaptor::GetSessionIdleTime()` must always return a valid uint — even on compositors
without the protocol. `NullIdleBackend` keeps the service unconditionally present, returns 0 from
`getSessionIdleTimeSeconds()`, and sets `idleBackendAvailable` to `false` so QML can display a
degraded-mode indicator if desired. The design is identical to `NullIdleBackend` in the session
backend: graceful degradation with a capability flag, not conditional instantiation.

**logind inhibit fd over `idle-inhibit-unstable-v1` Wayland protocol.**
`idle-inhibit-unstable-v1` binds the inhibitor to a `wl_surface`: the surface must remain mapped
and committed for the inhibit to stay active, and it is automatically released if the surface is
destroyed. This is the correct mechanism for a media player that wants "no screensaver while this
window is visible". For the shell's "Keep Awake" toggle and for inhibitors held on behalf of
headless apps (e.g., a background video call), there is no suitable surface. logind's `Inhibit`
fd is process-scoped, survives surface lifecycle, and is released cleanly on fd close (including
on process death). It is also what all major compositor idle daemons (hypridle, swayidle) respect.

**WeatherService and CalendarService self-register to `idleChanged` (no central coordinator).**
A central `IdleCoordinator` that iterates over a list of registered services would require each
service to implement a common interface and would make `IdleService` aware of every service it
manages. This creates a web of dependencies: adding a new pauseable service would require modifying
`IdleCoordinator`. The self-registration pattern (`connect(IdleService, &IdleService::idleChanged,
this, &WeatherService::onIdleChanged)` in `WeatherService`'s constructor or `start()`) is
consistent with how other cross-service wiring works in `ShellApplication` (e.g., calendar-service
battery gate, config-change propagation). Each service is responsible for its own pause/resume
logic, which is simpler to test in isolation.

---

## 8. Alternatives Considered

**`ext-idle-notify-v1` at a 5-minute threshold (matching `idleThresholdMs`).**
This would mean the `resumed` event fires up to 5 minutes after real activity, making
`GetSessionIdleTime()` return stale values. A caller that polls every 30 seconds would see the
same non-zero value for up to 5 minutes after the user returned. Teams/Zoom/Slack's "Away" state
would not reset promptly. Rejected in favor of the 1-second protocol threshold with filtering at
the `idleChanged` emission level.

**Implementing `org.freedesktop.ScreenSaver` inside `IdleService` directly.**
Merging the adaptor into the singleton avoids one class but violates single responsibility: idle
tracking, D-Bus dispatch, and cookie management would coexist. The `QDBusContext` mixin conflicts
with `QML_SINGLETON` registration patterns. Testing cookie generation and inhibitor release becomes
dependent on a running QML engine. Rejected in favor of `ScreenSaverAdaptor` as a dedicated class.

**Using `idle-inhibit-unstable-v1` Wayland protocol for inhibition.**
Surface-scoped and compositor-managed. Cannot serve headless callers (background apps, shell's own
"Keep Awake" toggle). Does not prevent logind-level sleep. Rejected; logind fd approach is
process-scoped and universally recognized.

**Central `IdleCoordinator` pausing services on idle.**
Creates coupling: `IdleCoordinator` must import service headers and know about each pauseable
service. Adding or removing a pauseable service modifies the coordinator. The self-registration
pattern requires no central knowledge and matches the project's existing style. Rejected.

**Using `org.freedesktop.login1.Session.IdleHint` for idle detection instead of Wayland protocol.**
logind's `IdleHint` is set by idle daemons (hypridle, swayidle), not by the compositor directly.
It therefore depends on an idle daemon being installed and configured — exactly the scenario the
spec warns against (REQ-F-017, REQ-F-018). Without an idle daemon, `IdleHint` is never set and
`GetSessionIdleTime()` would always return 0 even when the seat is idle. `ext-idle-notify-v1` is
compositor-native and does not require external daemons. Rejected per REQ-C-001.

---

## 9. Known Risks

**Frequent Wayland events at 1-second threshold during active use.**
`ext-idle-notify-v1` at a 1-second threshold means the compositor sends `idle` and `resumed` events
every second of continuous mouse movement. `ExtIdleNotifyBackend` only updates a timestamp and sets
a flag — no signal is emitted on the raw backend events. The threshold guard in `IdleService`
ensures `idleChanged` fires only on genuine state transitions (active → idle, idle → active), so
`WeatherService` and `CalendarService` are not hammered. However, the Wayland round-trip itself
occurs at 1-second intervals. Validation: verify on a live Hyprland session that CPU usage during
active input remains flat (no timer-driven busyloop; the events come from the compositor's dispatch
thread via Qt's Wayland integration). If event volume is measurably problematic at 1s, the protocol
threshold can be raised (e.g., to 5s) while accepting ~5s worst-case lag in `GetSessionIdleTime()`
accuracy — this trade-off must be re-evaluated with real app testing (REQ-NF-001).

**`org.freedesktop.ScreenSaver` name conflict at startup.**
Another process (KDE's `kscreenlocker`, GNOME's `gnome-screensaver`, a user's custom script) may
already own the name. `QDBusConnection::registerService()` returns `false` in this case.
`ScreenSaverAdaptor::registerService()` must detect this, log exactly one `qCWarning`, and return
`false`. `IdleService` continues to function — it still tracks idle state and emits `idleChanged`
for `WeatherService`/`CalendarService` — but `GetSessionIdleTime()` and `ActiveChanged` will not
be served to external apps. The shell does not retry (REQ-F-012, REQ-NF-002).

**logind inhibitor fd leak on shell crash.**
If `holonight-shell` is killed with `SIGKILL` while `IdleInhibitor` holds a fd, the fd is closed
by the kernel as part of process cleanup. logind monitors fds and automatically releases the
inhibitor when the last fd referring to it is closed. This is by design in the logind inhibitor
spec (`mode="block"`). No leak risk exists; this is documented as a non-issue.

**`sessionLocked` initial value timing.**
`IdleService` subscribes to logind's `PropertiesChanged` signal to track `LockedHint`. At
construction, it performs a synchronous `org.freedesktop.DBus.Properties.Get` call to initialize
`session_locked_`. If logind is slow or unavailable at shell startup, this call may block briefly.
Mitigation: use an async `QDBusPendingCallWatcher` for the initial read, defaulting
`session_locked_` to `false` until the reply arrives. This matches the pattern used by
`PowerProfilesService` for its initial property read.

**`QDBusArgument` read-mode trap in logind `Inhibit` fd reply.**
logind's `Inhibit` method returns a Unix fd via D-Bus. Qt's D-Bus layer wraps it as
`QDBusUnixFileDescriptor`. When extracting the fd from the reply, use
`reply.arguments().at(0).value<QDBusUnixFileDescriptor>()` and call `.takeFileDescriptor()` (which
transfers ownership and prevents the Qt wrapper from closing the fd on destruction). Do not use
`QVariant::canConvert` probes on the returned variant — this is the `QDBusArgument` read-mode trap
documented in CLAUDE.md that can log spurious `QDBusArgument: write from a read-only object` errors.

**`ext-idle-notify-v1` not available on older compositors.**
Hyprland has supported this protocol since early 2024; Sway since mid-2024; Niri since 0.1.x.
Users on older compositor versions will get `NullIdleBackend`. The `idleBackendAvailable: false`
property allows QML to display a degraded-mode indicator if desired. The shell does not crash or
log errors in this path (REQ-NF-004).

---

## 10. Modifications to Existing Services

### 10.1 `WeatherService`

In `WeatherService.cpp`, in the constructor (or in `start()`), add:

```cpp
if (auto* idle = qobject_cast<IdleService*>(
        qmlEngine(this)->singletonInstance<IdleService*>("HolonightShell", "IdleService"))) {
    connect(idle, &IdleService::idleChanged, this, &WeatherService::onIdleChanged);
}
```

Or, since `IdleService` is constructed before `WeatherService` in `ShellApplication`, pass the
pointer directly:

```cpp
// ShellApplication constructor
idle_service_(new IdleService(env_.get(), notification_service_, this)),
weather_(new WeatherService(config_service_, idle_service_, this)),
```

And in `WeatherService`:

```cpp
void WeatherService::onIdleChanged(bool idle) {
    if (idle) {
        refresh_timer_.stop();
        // Cancel any in-flight fetch if the provider supports it.
    } else {
        fetch();          // immediate refresh on resume
        resetBackoff();
        startRefreshTimer();
    }
}
```

`WeatherService.h` gains a private slot `void onIdleChanged(bool idle)`. No new Q_PROPERTY is
added — idle state is consumed silently.

### 10.2 `CalendarService`

Same pattern. `CalendarSyncManager` already exposes a `notifySidebarOpened()` that triggers an
immediate sync; a parallel `resumeFromIdle()` (or reuse of `notifySidebarOpened()`) triggers the
immediate sync on idle=false. The periodic sync timer is stopped on idle=true.

### 10.3 `ShellApplication`

**New member:**

```cpp
IdleService* idle_service_ = nullptr;
```

**Construction order** (in the member initializer list, before `weather_` and
`notification_service_`):

```cpp
notification_service_(new NotificationService(config_service_, aws_, this)),
idle_service_(new IdleService(env_.get(), notification_service_, this)),
```

`IdleService` needs `NotificationService` to post the "no daemon" notification, so it must be
constructed after `NotificationService`. `env_` is a `std::unique_ptr<ProcessEnvironment>`
currently owned by `SessionService`. To allow sharing, `env_` should be promoted to a
`ShellApplication`-owned member (created once, pointer passed to both `SessionService` and
`IdleService`). This avoids duplicating the `/proc` scan. The `CommandRunner` instance similarly
remains owned by `ShellApplication`.

**QML registration** (in `registerQmlTypes()`):

```cpp
reg(idle_service_, "IdleService");
```

**ScreenSaverAdaptor initialization** (in `startServices()`):

```cpp
auto* screensaver_adaptor = new ScreenSaverAdaptor(idle_service_, this);
screensaver_adaptor->registerService();  // logs warning on name conflict; never aborts
connect(idle_service_, &IdleService::idleChanged,
        screensaver_adaptor, &ScreenSaverAdaptor::onIdleChanged);
```

`ScreenSaverAdaptor` does not need to be a QML-registered type. It is created in `startServices()`
and parented to `ShellApplication` for lifetime management.

**Include additions** in `ShellApplication.cpp`:

```cpp
#include "IdleService.h"
#include "ScreenSaverAdaptor.h"
```

### 10.4 `CMakeLists.txt`

- Add `protocols/ext-idle-notify-v1.xml` to the `qt6_generate_wayland_protocol_client_sources` call on `holonight_platform`.
- Add all six `src/services/idle/` source pairs to `holonight_services`.
- Add `${CMAKE_CURRENT_SOURCE_DIR}/src/services/idle` to `target_include_directories(holonight_services PUBLIC ...)`.
- Add `src/qml/RightSidebar/KeepAwakeAction.qml` to `HOLONIGHT_QML_FILES` in sorted position.
- Add the two generated Wayland headers to `TIDY_GENERATED_FILES`.

### 10.5 `tests/CMakeLists.txt`

Add a new test file `tests/test_idle_service.cpp` to the existing `test_holonight_services`
executable. Run `task configure-tests` before `task test` after adding the file (stale-dep silent
skip documented in CLAUDE.md). Test coverage targets:

- `IdleService` with `FakeIdleBackend` (controls `idleThresholdExceeded` signal and `getSessionIdleTimeSeconds()` return value)
- `IdleService` with `FakeProcessEnvironment` for daemon detection (verifying `idleDaemonDetected` and notification posting)
- `ScreenSaverAdaptor` cookie generation, multi-cookie inhibitor lifecycle, and graceful `UnInhibit(invalid-cookie)` handling
- `IdleInhibitor` acquire/release round-trip against a mock logind D-Bus service

All tests run offline (no live Wayland session, no live logind) per REQ-NF-006.

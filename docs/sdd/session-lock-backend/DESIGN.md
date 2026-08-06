# Session Lock Backend — Architecture Design

## 1. Overview & Goals

`SessionService` currently hardcodes Hyprland and systemd APIs directly in its method bodies.
`lockScreen()` is a no-op. `logout()` always runs `hyprctl dispatch exit` regardless of compositor.

This refactor introduces a `SessionBackend` abstraction that:

- auto-detects the compositor at construction and selects the matching backend (REQ-F-001, REQ-C-001)
- confines compositor-specific code to the appropriate backend class (REQ-C-004)
- implements a shared, layered lock strategy as a collaborator owned by the base backend (REQ-F-006..009)
- exposes diagnostic properties for QML consumers (REQ-F-013..015, REQ-F-005)
- provides injectable seams so all lock branches are exercisable under GTest without a live session (REQ-NF-002)
- preserves the five existing `Q_INVOKABLE` signatures unchanged so no QML caller changes (REQ-NF-003)

---

## 2. Component Breakdown

### 2.1 `SessionBackend` — abstract base with common implementation

REQ-F-002 specifies all five methods as pure virtual. However, REQ-F-010, REQ-F-011, and REQ-F-012
mandate that `sleep()`, `reboot()`, and `shutdown()` execute identical `systemctl` commands on every
compositor. Making all five pure-virtual forces both concrete backends to duplicate three method
bodies that are semantically identical and must not diverge.

**Design refinement (for orchestrator approval):** `sleep()`, `reboot()`, and `shutdown()` are
implemented concretely in `SessionBackend` and are non-virtual. Only `logout()` and `lockScreen()`
are pure virtual. The three systemctl methods call `CommandRunner::run()` through the injected seam
so they remain testable. This satisfies REQ-F-010..012 with a single implementation and eliminates
a class of copy-paste drift bugs. The deviation from the spec's literal wording is intentional: the
spec lists all five as "overridable" for extensibility, but no production use case for overriding the
power actions exists; future compositor backends that genuinely need different power semantics can
use virtual dispatch if required at that point.

**Responsibilities:**
- Holds injected `ProcessEnvironment` (process probe seam) and `CommandRunner` (launch seam)
- Owns a `Locker` helper and delegates `lockScreen()` to it in the default implementation (concrete
  backends call `SessionBackend::lockScreen()` unless they need to override)
- Provides `logoutSupported()`, `backendName()`, `lockerAvailable()`, `lockerName()` as pure-virtual
  accessors — each concrete backend returns its own values
- Implements `sleep()`, `reboot()`, `shutdown()` as non-virtual final wrappers over `CommandRunner`

### 2.2 `HyprlandSessionBackend`

- `logout()`: calls `CommandRunner::run("hyprctl", {"dispatch", "exit"})`
- `lockScreen()`: delegates to the inherited `Locker`
- `logoutSupported()` → `true`
- `backendName()` → `"hyprland"`

All Hyprland-specific strings live here; SessionService and LogindSessionBackend contain no
reference to `hyprctl` (REQ-C-004).

### 2.3 `LogindSessionBackend`

- `logout()`: silent no-op (REQ-F-004)
- `lockScreen()`: delegates to the inherited `Locker`
- `logoutSupported()` → `false` (REQ-F-005)
- `backendName()` → `"logind"`

### 2.4 `Locker` — shared lock strategy collaborator

The lock strategy (idle-daemon detection → `loginctl lock-session`; PATH search → locker spawn;
idempotency guard) is identical for both compositors. Duplicating it inside each backend would
violate DRY and double the test surface. `Locker` is a plain C++ class (not a QObject) owned by
`SessionBackend`. It is constructed once, probes the environment via the two injected seams, caches
`lockerAvailable_` and `lockerName_` at construction, then executes the four-branch strategy on each
`lock()` call.

**Lock strategy (REQ-F-006..009):**

1. If any of `{hypridle, swayidle, xss-lock}` is running (via `ProcessEnvironment::isRunning`):
   invoke `loginctl lock-session` through `CommandRunner` and return.
2. Else: search PATH for `{hyprlock, swaylock, gtklock, waylock}` in that order via
   `ProcessEnvironment::findExecutable`. If found:
   a. If `ProcessEnvironment::isRunning(resolvedName)` — locker already up, skip spawn (REQ-F-008).
   b. Otherwise spawn the resolved binary with no arguments via `CommandRunner`.
3. If nothing is found: return without error or log-noise (REQ-F-009).

**Cached properties** (computed once at `Locker` construction, not on each `lock()` call):
- `lockerAvailable_` — true if any idle daemon is running OR any locker binary is on PATH
- `lockerName_` — idle daemon name if one is running; else first PATH-found locker name; else `""`

Property values are determined by the state at `SessionService` construction (REQ-C-001). They are
not live-updated. This is a deliberate trade-off documented in §10 (Known Risks).

### 2.5 `SessionService` — thin QML facade

Responsibilities:
- Runs the backend factory in its constructor (one-time, result stored as `std::unique_ptr<SessionBackend> backend_`)
- Exposes four `Q_PROPERTY` values by forwarding to `backend_->`
- Delegates all five `Q_INVOKABLE` methods to `backend_->` with no conditional logic at call sites
  (REQ-F-002 acceptance criterion)
- Carries `QML_ELEMENT` / `QML_SINGLETON` — no other class in this pipeline does

---

## 3. Key Seams for Testability (REQ-NF-002)

Three injectable interfaces eliminate all live-session dependencies from unit tests.

### 3.1 `ProcessEnvironment` — probe seam

```cpp
class ProcessEnvironment {
 public:
  virtual ~ProcessEnvironment() = default;
  // Returns true if a process with the given name is currently running.
  [[nodiscard]] virtual bool isRunning(const QString& processName) const = 0;
  // Searches PATH for the executable; returns full path or empty string.
  [[nodiscard]] virtual QString findExecutable(const QString& name) const = 0;

 protected:
  ProcessEnvironment() = default;
};

class SystemProcessEnvironment final : public ProcessEnvironment {
 public:
  [[nodiscard]] bool isRunning(const QString& processName) const override;  // reads /proc via QDir
  [[nodiscard]] QString findExecutable(const QString& name) const override; // QStandardPaths::findExecutable
};
```

The default `SystemProcessEnvironment::isRunning` scans `/proc` with `QDir` for a matching
`/proc/*/comm` entry. This avoids shelling out to `pidof` (which adds another process launch to the
critical path and is harder to intercept in tests). `findExecutable` delegates to
`QStandardPaths::findExecutable` so no hardcoded paths appear (REQ-C-003).

### 3.2 `CommandRunner` — launch seam

```cpp
class CommandRunner {
 public:
  virtual ~CommandRunner() = default;
  // Launches program with args, detached. Returns false if start fails.
  virtual bool run(const QString& program, const QStringList& args) = 0;

 protected:
  CommandRunner() = default;
};

class DetachedCommandRunner final : public CommandRunner {
 public:
  bool run(const QString& program, const QStringList& args) override;
  // Implementation: QProcess::startDetached(program, args).
  // startDetached avoids zombie processes by design — the child's parent
  // becomes init(1) immediately. On failure (program not found), it returns
  // false; log via qCWarning but do not throw (REQ-NF-001).
};
```

### 3.3 GTest coverage of all four lock branches

Using `FakeProcessEnvironment` (controls `isRunning` / `findExecutable` return values) and
`SpyCommandRunner` (records calls without launching anything):

```cpp
// Branch 1: idle daemon running → loginctl
FakeProcessEnvironment env;
env.setRunning("hypridle", true);
SpyCommandRunner runner;
Locker locker(&env, &runner);
locker.lock();
EXPECT_EQ(runner.lastProgram(), "loginctl");
EXPECT_THAT(runner.lastArgs(), ElementsAre("lock-session"));
EXPECT_EQ(runner.callCount(), 1);

// Branch 2: no daemon, locker on PATH, not running → spawn
env.setRunning("hypridle", false);
env.setFindExecutable("hyprlock", "/usr/bin/hyprlock");
locker.lock();
EXPECT_EQ(runner.lastProgram(), "/usr/bin/hyprlock");
EXPECT_EQ(runner.callCount(), 1);

// Branch 3: locker already running → no spawn (idempotency, REQ-F-008)
env.setRunning("hyprlock", true);
SpyCommandRunner runner3;
Locker locker3(&env, &runner3);
locker3.lock();
EXPECT_EQ(runner3.callCount(), 0);

// Branch 4: nothing available → graceful no-op (REQ-F-009)
FakeProcessEnvironment env4;   // all isRunning=false, findExecutable=""
SpyCommandRunner runner4;
Locker locker4(&env4, &runner4);
locker4.lock();                // must not throw or crash
EXPECT_EQ(runner4.callCount(), 0);
```

The `SessionService` itself is tested by injecting a `FakeSessionBackend` (implements the abstract
interface) and verifying that each of the five `Q_INVOKABLE` methods calls exactly the corresponding
backend method (REQ-F-002 acceptance).

---

## 4. Detection & Construction Flow

Backend detection happens once in the `SessionService` constructor via a factory function:

```
SessionService::SessionService(parent)
  │
  ├─ BackendFactory::create(env, runner)
  │    ├─ qgetenv("HYPRLAND_INSTANCE_SIGNATURE") non-empty?
  │    │    yes → new HyprlandSessionBackend(env, runner)
  │    │    no  → new LogindSessionBackend(env, runner)
  │
  └─ Each backend ctor:
       └─ Locker locker_(env, runner)   ← probes daemons + PATH here, once
            ├─ isRunning({hypridle, swayidle, xss-lock})
            ├─ findExecutable({hyprlock, swaylock, gtklock, waylock})
            ├─ sets lockerAvailable_, lockerName_
```

The `backend_` member is a `const std::unique_ptr<SessionBackend>` (const pointer, preventing
reassignment after construction — REQ-C-001). Properties are populated from the backend during
construction and do not change for the process lifetime.

`BackendFactory::create` accepts `ProcessEnvironment*` and `CommandRunner*` parameters with
production defaults (`SystemProcessEnvironment`, `DetachedCommandRunner`). In tests the caller
passes fakes directly to the backend constructor, bypassing the factory.

---

## 5. Public Interfaces / API

### 5.1 Seam interfaces

```cpp
// src/services/session/ProcessEnvironment.h
class ProcessEnvironment {
 public:
  virtual ~ProcessEnvironment() = default;
  [[nodiscard]] virtual bool isRunning(const QString& processName) const = 0;
  [[nodiscard]] virtual QString findExecutable(const QString& name) const = 0;
 protected:
  ProcessEnvironment() = default;
};

// src/services/session/CommandRunner.h
class CommandRunner {
 public:
  virtual ~CommandRunner() = default;
  virtual bool run(const QString& program, const QStringList& args) = 0;
 protected:
  CommandRunner() = default;
};
```

### 5.2 `SessionBackend` — abstract base

```cpp
// src/services/session/SessionBackend.h
#pragma once
#include <QString>
#include <memory>
class ProcessEnvironment;
class CommandRunner;
class Locker;

class SessionBackend {
 public:
  virtual ~SessionBackend() = default;

  SessionBackend(const SessionBackend&) = delete;
  SessionBackend& operator=(const SessionBackend&) = delete;
  SessionBackend(SessionBackend&&) = delete;
  SessionBackend& operator=(SessionBackend&&) = delete;

  // Compositor-specific — pure virtual.
  virtual void logout() = 0;
  virtual void lockScreen() = 0;

  // Common systemctl actions — non-virtual, implemented in base.
  void sleep();
  void reboot();
  void shutdown();

  // Capability accessors — pure virtual.
  [[nodiscard]] virtual bool logoutSupported() const = 0;
  [[nodiscard]] virtual QString backendName() const = 0;
  [[nodiscard]] virtual bool lockerAvailable() const = 0;
  [[nodiscard]] virtual QString lockerName() const = 0;

 protected:
  explicit SessionBackend(ProcessEnvironment* env, CommandRunner* runner);

  // Subclasses call this to delegate lockScreen() to the shared Locker.
  void runLocker();

 private:
  CommandRunner* runner_;
  std::unique_ptr<Locker> locker_;
};
```

### 5.3 Concrete backends

```cpp
// src/services/session/HyprlandSessionBackend.h
class HyprlandSessionBackend final : public SessionBackend {
 public:
  explicit HyprlandSessionBackend(ProcessEnvironment* env, CommandRunner* runner);

  void logout() override;    // hyprctl dispatch exit
  void lockScreen() override { runLocker(); }

  [[nodiscard]] bool logoutSupported() const override { return true; }
  [[nodiscard]] QString backendName() const override { return QStringLiteral("hyprland"); }
  [[nodiscard]] bool lockerAvailable() const override;
  [[nodiscard]] QString lockerName() const override;
};

// src/services/session/LogindSessionBackend.h
class LogindSessionBackend final : public SessionBackend {
 public:
  explicit LogindSessionBackend(ProcessEnvironment* env, CommandRunner* runner);

  void logout() override {}  // silent no-op
  void lockScreen() override { runLocker(); }

  [[nodiscard]] bool logoutSupported() const override { return false; }
  [[nodiscard]] QString backendName() const override { return QStringLiteral("logind"); }
  [[nodiscard]] bool lockerAvailable() const override;
  [[nodiscard]] QString lockerName() const override;
};
```

### 5.4 `Locker`

```cpp
// src/services/session/Locker.h
class Locker {
 public:
  explicit Locker(ProcessEnvironment* env, CommandRunner* runner);

  void lock();

  [[nodiscard]] bool lockerAvailable() const { return locker_available_; }
  [[nodiscard]] QString lockerName() const { return locker_name_; }

 private:
  ProcessEnvironment* env_;
  CommandRunner* runner_;
  bool locker_available_{false};
  QString locker_name_;
  QString resolved_path_;      // full path from findExecutable, empty if daemon path
  bool via_daemon_{false};     // true when locker_name_ refers to a running daemon
};
```

### 5.5 `SessionService` — updated header

```cpp
// src/services/SessionService.h  (in-place replacement)
#pragma once
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <memory>
class SessionBackend;

class SessionService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(QString backendName   READ backendName   CONSTANT)
  Q_PROPERTY(bool    logoutSupported READ logoutSupported CONSTANT)
  Q_PROPERTY(bool    lockerAvailable READ lockerAvailable CONSTANT)
  Q_PROPERTY(QString lockerName    READ lockerName    CONSTANT)

 public:
  explicit SessionService(QObject* parent = nullptr);
  ~SessionService() override;   // defined in .cpp so unique_ptr<SessionBackend> destructor resolves

  SessionService(const SessionService&) = delete;
  SessionService& operator=(const SessionService&) = delete;
  SessionService(SessionService&&) = delete;
  SessionService& operator=(SessionService&&) = delete;

  // Test injection point — passes ownership of a pre-built backend.
  explicit SessionService(std::unique_ptr<SessionBackend> backend, QObject* parent = nullptr);

  [[nodiscard]] QString backendName()    const;
  [[nodiscard]] bool    logoutSupported() const;
  [[nodiscard]] bool    lockerAvailable() const;
  [[nodiscard]] QString lockerName()     const;

  Q_INVOKABLE void lockScreen();
  Q_INVOKABLE void logout();
  Q_INVOKABLE void sleep();
  Q_INVOKABLE void reboot();
  Q_INVOKABLE void shutdown();

 private:
  const std::unique_ptr<SessionBackend> backend_;
};
```

All five `Q_INVOKABLE` signatures are unchanged (REQ-NF-003). All four `Q_PROPERTY` values are
`CONSTANT` — they are computed once at construction and never change (REQ-C-001).

---

## 6. File Layout & Build Wiring

### New files under `src/services/session/`

```
src/services/session/
  ProcessEnvironment.h          # seam interface + SystemProcessEnvironment declaration
  ProcessEnvironment.cpp        # SystemProcessEnvironment::isRunning (/proc scan), findExecutable
  CommandRunner.h               # seam interface + DetachedCommandRunner declaration
  CommandRunner.cpp             # DetachedCommandRunner::run (QProcess::startDetached)
  SessionBackend.h              # abstract base + protected runLocker()
  SessionBackend.cpp            # sleep/reboot/shutdown implementations; runLocker body
  HyprlandSessionBackend.h
  HyprlandSessionBackend.cpp    # logout() → hyprctl; lockerAvailable/Name forwarded from Locker
  LogindSessionBackend.h
  LogindSessionBackend.cpp      # logout() no-op; lockerAvailable/Name forwarded from Locker
  Locker.h
  Locker.cpp                    # four-branch lock strategy; cached lockerAvailable_/lockerName_
```

`SessionService.h` and `SessionService.cpp` remain at `src/services/` (no move needed).

### `CMakeLists.txt` additions — `holonight_services` target

Add the following source pairs to the existing `add_library(holonight_services STATIC ...)` block:

```cmake
    src/services/session/ProcessEnvironment.h
    src/services/session/ProcessEnvironment.cpp
    src/services/session/CommandRunner.h
    src/services/session/CommandRunner.cpp
    src/services/session/SessionBackend.h
    src/services/session/SessionBackend.cpp
    src/services/session/HyprlandSessionBackend.h
    src/services/session/HyprlandSessionBackend.cpp
    src/services/session/LogindSessionBackend.h
    src/services/session/LogindSessionBackend.cpp
    src/services/session/Locker.h
    src/services/session/Locker.cpp
```

Add `${CMAKE_CURRENT_SOURCE_DIR}/src/services/session` to `target_include_directories(holonight_services PUBLIC ...)`.

### Test file

New file: `tests/test_session_service.cpp`

Added to the existing `test_holonight_services` executable in `tests/CMakeLists.txt`:

```cmake
holonight_add_test_exe(test_holonight_services
  ...existing files...
  test_session_service.cpp
)
```

Run `task configure-tests` after adding the file before running `task test` to avoid the stale-dep
silent-skip trap documented in CLAUDE.md.

---

## 7. Data Flow Diagram

```
QML caller
    │
    │  SessionService.logout() / lockScreen() / sleep() / reboot() / shutdown()
    ▼
SessionService   (QML_SINGLETON, owns backend_)
    │
    │  backend_->logout()            backend_->lockScreen()
    │  backend_->sleep()             backend_->reboot()
    │  backend_->shutdown()
    ▼
SessionBackend (abstract)
    ├── HyprlandSessionBackend          LogindSessionBackend
    │       logout()                         logout() → no-op
    │       lockScreen() → runLocker()       lockScreen() → runLocker()
    │       sleep/reboot/shutdown            sleep/reboot/shutdown
    │       (inherited, non-virtual)         (inherited, non-virtual)
    │              │                                │
    │              └──────────┬─────────────────────┘
    │                         │
    │                       Locker
    │                         │
    │          ┌──────────────┼────────────────┐
    │          ▼              ▼                ▼
    │   ProcessEnvironment  CommandRunner   (cached props)
    │   (isRunning,         (run program    lockerAvailable_
    │    findExecutable)     + args)        lockerName_
    │          │              │
    │          ▼              ▼
    │   /proc scan        QProcess::startDetached
    │   QStandardPaths    loginctl / locker binary
    │
    └── sleep/reboot/shutdown (non-virtual)
              │
              ▼
         CommandRunner::run
              │
              ▼
         systemctl suspend / reboot / poweroff
```

---

## 8. Key Decisions & Rationale

**Base-with-common-impl vs pure interface.**
The three systemctl commands are byte-for-byte identical across backends. Pure-virtual forces
duplication or a non-obvious mixin. A shared non-virtual base implementation is simpler, eliminates
drift, and is still testable via the `CommandRunner` seam.

**`Locker` as a separate collaborator, not duplicated per backend.**
Both backends would have identical lock logic. Extracting it into `Locker` gives a single
well-tested implementation, a clean constructor seam for test injection, and a single place to
update when new lock candidates are added.

**Seam interfaces over subclassing for process I/O.**
`ProcessEnvironment` and `CommandRunner` are small, stable interfaces. Injecting them via
constructor parameters (not a global/static) keeps `SessionBackend` and `Locker` pure C++ objects
— no `QObject` overhead, no event-loop dependency, no signal-slot complexity. Tests construct
fake instances inline without registering metatypes or spinning an event loop.

**`QProcess::startDetached` over managed `QProcess`.**
`startDetached` makes the child process a direct child of `init(1)`, avoiding zombie accumulation
(REQ-NF-001). A managed `QProcess` would require connecting `finished` signals, storing the object,
and handling cleanup — significant complexity for fire-and-forget actions like locking and power
control. Failure is reported via the `bool` return value and logged with `qCWarning`; no exception
is thrown (REQ-NF-001 acceptance).

**Properties are `CONSTANT` / computed-once.**
Backend selection and lock-strategy probing happen at construction. Re-probing on each `Q_PROPERTY`
read would make QML bindings non-deterministic and violate REQ-C-001. Staleness (a locker installed
after start is not reflected) is documented in §10.

**Constructor injection for `SessionService` test seam.**
The two-constructor pattern (production ctor calls factory; test ctor accepts `unique_ptr<SessionBackend>`)
mirrors `PowerProfilesService`'s `SkipInitTag` pattern. It avoids any global/singleton backdoor and
keeps the test surface minimal.

---

## 9. Alternatives Considered

**Keep all logic in `SessionService` with `if`-branches.**
Rejected. No seam exists for `lockScreen()` since the method bodies cannot be mocked. Every new
compositor would add more conditionals. REQ-NF-002 cannot be met.

**Put lock logic in each backend.**
Rejected. Identical code in two places — duplicated tests, duplicated bug risk. Any change to the
locker priority list must be applied twice. `Locker` as a collaborator avoids this entirely.

**D-Bus logind `org.freedesktop.login1.Manager.LockSession` vs `loginctl lock-session` CLI.**
D-Bus would avoid spawning a child process and would surface errors as D-Bus reply codes. However:
(a) it requires adding `QDBusInterface` / `QDBusConnection` to this path, increasing coupling;
(b) `loginctl` is what the spec requires and matches the pre-existing project style for CLI
invocations (all current `sleep`/`reboot`/`shutdown` use `QProcess`); (c) D-Bus would be useful
if live error feedback or a reply signal were needed — neither is required here. The CLI path is
retained. A future iteration could switch without changing the external interface.

**Separate `SleepBackend` / `PowerBackend` for hypothetical non-systemd targets.**
Premature. The spec (REQ-F-010..012) explicitly states systemctl on "any backend". The
non-virtual base handles this cleanly and can be revisited if a non-systemd target ever surfaces.

**Using `QStandardPaths::findExecutable` to check if a process is running (not just on PATH).**
`findExecutable` checks the filesystem, not running processes. A process can be running without its
binary being on the current PATH (e.g. installed to `/usr/local/bin` after PATH was captured, or
launched with a full path). `/proc` scanning is the correct mechanism for `isRunning`. Both seams
are needed and are separate.

---

## 10. Known Risks

**Daemon-detection false negatives.**
`/proc` scanning matches on process name (`comm`), which is truncated to 15 characters on Linux.
Names like `xss-lock` (8 chars) and `swayidle` (8 chars) are safe. If a daemon renames its process
or runs under a wrapper, detection fails silently and the lock falls through to PATH search.
Mitigation: log via `qCInfo` when a daemon is detected to aid debugging.

**`startDetached` failure invisibility.**
If the locker binary path is stale (uninstalled between shell start and lock invocation),
`startDetached` returns `false` and the session is not locked. The `CommandRunner` logs a warning
via `qCWarning`, but the QML caller receives no error signal (the method returns `void`). This is
consistent with the project's fire-and-forget pattern for `sleep`/`reboot`/`shutdown`. A future
`lockFailed` signal could address this if user-visible feedback is required.

**polkit denials for systemctl.**
On some configurations, `systemctl reboot` and `systemctl poweroff` require an active logind
session with appropriate polkit rules. `startDetached` returns `true` (launch succeeded) even when
systemctl is denied by polkit. The user sees no feedback. This matches pre-refactor behavior — no
regression introduced.

**Property staleness — locker installed after shell start.**
`lockerAvailable` and `lockerName` are computed once at construction. If the user installs `hyprlock`
after `holonight-shell` starts, the property shows `false` until the shell restarts. Since these
are `CONSTANT` Q_PROPERTYs, QML cannot bind to changes. Acceptable given REQ-C-001's lifetime
constraint; documented as a limitation. A `rescanLocker()` slot could be added in a later iteration
if runtime refresh becomes necessary.

**`HYPRLAND_INSTANCE_SIGNATURE` spoofing.**
Any process can set this env var. Detection is a heuristic, not a security check. HoloNight is not
a security boundary (as stated in the spec's overview), so this is acceptable.

# POC Remediation Phase 1 — Design

Stage 2 design for `docs/sdd/poc-remediation-phase1/SPEC.md`. Resolves the three items the spec
explicitly deferred to this stage:

1. Item A — `ExtWorkspaceManager` constructor statement ordering around the null-config guard.
2. Item B — session-command failure signal name/signature and its full propagation path.
3. Item C — concrete file paths for the two extracted helper functions.

All three items are independent; none of the file changes in one item touch the files of another.

---

## 1. Components affected

### Item A

| File | Change |
|---|---|
| `apps/shell/app/ShellApplication.cpp` (`startShell()`, ~line 247) | Replace `Q_ASSERT(registered_ && services_started_);` with an `if` guard + `qCritical()` + early `return;`. |
| `libs/holonight-core/src/ExtWorkspaceManager.cpp` (constructor, ~line 100) | Replace `Q_ASSERT(config != nullptr);` with an `if (config == nullptr)` guard + `qCritical()` + early `return;` from the constructor body. |
| `tests/test_shell_application.cpp` (**new**) | New GTest file exercising the `startShell()` guard. |
| `tests/test_ext_workspace_manager.cpp` (**new**) | New GTest file exercising the null-config guard. |
| `tests/CMakeLists.txt` | Add both new files to their respective test executables (`test_holonight_app`, `test_holonight_core`). |

### Item B

| File | Change |
|---|---|
| `libs/holonight-services/src/session/SessionCommandResult.h` (**new**) | Tiny value type `{bool ok; QString reason;}` shared by `Locker` and `SessionBackend`. |
| `libs/holonight-services/src/session/Locker.h` / `.cpp` | `void lock()` → `[[nodiscard]] SessionCommandResult lock()`. |
| `libs/holonight-services/src/session/SessionBackend.h` / `.cpp` | `run()`, `runLocker()`, `sleep()`, `reboot()`, `shutdown()` return `SessionCommandResult` instead of `void`. `logout()`/`lockScreen()` pure-virtuals now return `SessionCommandResult`. |
| `libs/holonight-services/src/session/HyprlandSessionBackend.h` / `.cpp` | `logout()`/`lockScreen()` overrides updated to return `SessionCommandResult`. |
| `libs/holonight-services/src/session/LogindSessionBackend.h` | `logout()`/`lockScreen()` overrides updated to return `SessionCommandResult` (`logout()` returns `SessionCommandResult::success()` — unsupported is a documented no-op, not a failure). |
| `libs/holonight-services/src/SessionService.h` / `.cpp` | Add `Q_SIGNAL void commandFailed(const QString& action, const QString& reason);`; each of the five `Q_INVOKABLE` methods inspects the backend's `SessionCommandResult` and emits on failure. Public signatures of the five invokables are unchanged (still `void`), so QML callers are unaffected. |
| `apps/shell/app/ShellApplication.cpp` (`startServices()`, near line 209–213) | New `connect(session_, &SessionService::commandFailed, this, [this](...) { notification_server_->Notify(...); });` — the only new code in the composition root. |
| `tests/test_session_service.cpp` | Extend `SpyCommandRunner` with a `setShouldFail(bool)` toggle; add one failure test per command (5) + one non-regression "success path stays silent" test. |

No new files needed in `apps/shell/app/` beyond the one `connect()` line — `ShellApplication.h` already declares `session_` and `notification_server_` as members, and already includes both `SessionService.h` and `NotificationServer.h`.

### Item C

| File | Change |
|---|---|
| `libs/holonight-services/src/LogindSessionResolver.h` (**new**) | Declares `QString resolveActiveLogindSessionPath();`. |
| `libs/holonight-services/src/LogindSessionResolver.cpp` (**new**) | Implements the GetSessionByPID + loginctl fallback logic (moved verbatim from the two duplicate sites). |
| `libs/holonight-services/src/ThemeConfigPath.h` (**new**) | Declares `struct ThemeConfigPaths { QString dir_path; QString file_path; };` and `ThemeConfigPaths resolveHolonightThemeConfigPaths();`. |
| `libs/holonight-services/src/ThemeConfigPath.cpp` (**new**) | Implements the XDG_CONFIG_HOME + `/holonight/theme.conf` resolution (moved verbatim). |
| `libs/holonight-services/src/idle/IdleService.cpp` (`subscribeLockedHint()`, ~line 172) | Replaces its inline GetSessionByPID/loginctl block with a call to `resolveActiveLogindSessionPath()`. |
| `libs/holonight-services/src/brightness/SysfsBackend.cpp` (`resolveSessionPath()`, ~line 82) | Same replacement. |
| `libs/holonight-services/src/ThemeService.cpp` (`resolveThemeConfigPath()`, ~line 17) | Replaces its inline XDG logic with a call to `resolveHolonightThemeConfigPaths()`. |
| `libs/holonight-services/src/portal/SettingsPortalBackend.cpp` (anonymous-namespace `themeConfigPath()`, ~line 18) | Function deleted; its one call site (line 155, `QSettings settings{themeConfigPath(), ...}`) now calls `resolveHolonightThemeConfigPaths().file_path`. |
| `tests/test_theme_config_path.cpp` (**new**) | Deterministic unit tests for `resolveHolonightThemeConfigPaths()` (XDG_CONFIG_HOME set / unset). |
| `tests/test_logind_session_resolver.cpp` (**new**) | Smoke test for `resolveActiveLogindSessionPath()` (see Test Plan — this one is inherently non-deterministic w.r.t. a live logind session). |
| `tests/CMakeLists.txt` | Add both new test files to `test_holonight_services`. |

`libs/holonight-services/src/mime/MimeService.cpp` and its `xdgConfigHome()` helper are **not touched** (REQ-C-C.1).

---

## 2. Item A — design decision: constructor statement ordering

### Current constructor body (unchanged statements, for reference)

```cpp
ExtWorkspaceManager::ExtWorkspaceManager(WorkspaceModel* model, ConfigService* config, QObject* parent)
    : QWaylandClientExtensionTemplate(1), model_(model) {
  Q_ASSERT(config != nullptr);
  model_->setDisplayCount(config->barWorkspaces().count);              // (1) dereferences config
  connect(config, &ConfigService::barWorkspacesChanged, this,
          [this, config]() { model_->setDisplayCount(config->barWorkspaces().count); });  // (2) dereferences config
  connect(model_, &WorkspaceModel::activateWorkspaceRequested, this,
          &ExtWorkspaceManager::activateWorkspace);                     // (3) model_-only
  connect(model_, &WorkspaceModel::activateSpecialWorkspaceRequested, this,
          &ExtWorkspaceManager::activateSpecialWorkspace);               // (4) model_-only
  connect(this, &QWaylandClientExtension::activeChanged, this, [this]() { ... });  // (5) model_/config-independent
}
```

Statements (1) and (2) dereference `config`. Statements (3), (4), (5) touch only `model_` and `this`
and would be safe to run even with `config == nullptr`.

### Decision: full early return, object left fully inert

```cpp
ExtWorkspaceManager::ExtWorkspaceManager(WorkspaceModel* model, ConfigService* config, QObject* parent)
    : QWaylandClientExtensionTemplate(1), model_(model) {
  if (config == nullptr) {
    qCritical("ExtWorkspaceManager: constructed with null ConfigService; workspace manager will remain inert");
    return;
  }
  model_->setDisplayCount(config->barWorkspaces().count);
  connect(config, &ConfigService::barWorkspacesChanged, this,
          [this, config]() { model_->setDisplayCount(config->barWorkspaces().count); });
  connect(model_, &WorkspaceModel::activateWorkspaceRequested, this, &ExtWorkspaceManager::activateWorkspace);
  connect(model_, &WorkspaceModel::activateSpecialWorkspaceRequested, this,
          &ExtWorkspaceManager::activateSpecialWorkspace);
  connect(this, &QWaylandClientExtension::activeChanged, this, [this]() {
    if (!isActive()) {
      qWarning("ExtWorkspaceManager: ext-workspace-v1 not supported by compositor");
    }
  });
}
```

The guard sits **before all five statements**, including (3)/(4)/(5), which do not dereference
`config`. `qCritical` matches the existing plain-call style already used in this file (`qWarning(...)`
at the bottom of the constructor) and in `ShellApplication.cpp` (`qCritical("ShellApplication: wlr-layer-shell protocol not available")`) — no new `Q_LOGGING_CATEGORY` needed.

**Rationale**: `config == nullptr` is a genuine construction-contract violation, not a recoverable
runtime condition — it is why this was a `Q_ASSERT` in the first place. In production the only call
site (`apps/shell/app/ShellApplication.cpp`, `new ExtWorkspaceManager(model_, config_service_, this)`)
always passes a valid `config_service_` constructed earlier in the same initializer list, so this
branch is unreachable in practice; it exists purely as a release-mode safety net where a crashing
assert previously existed. Given that, a fully inert object (no Wayland activate-forwarding wired
either) is easier to reason about than a "partially wired" object that reacts to some things (compositor
workspace activation requests) but silently ignores others (config-driven display-count changes) —
that half-alive state is the kind of thing that produces a much harder-to-diagnose bug report than a
visibly-inert one paired with a `qCritical` log line. The object remains safely destructible either way
(`~ExtWorkspaceManager()` only touches `groups_`/`handle_map_`, both empty when the guard fires).

**Alternative considered and rejected**: wire statements (3)/(4)/(5) (the `model_`-only connects)
before the guard, and only skip (1)/(2). Rejected because it adds a "degraded but partially
functional" object with unclear value — the workspace bar would show live activation forwarding
but never reflect config's bar-workspace count, which is a confusing half-state to debug and adds
code-review burden for a defensive branch that should never fire in production.

---

## 3. Item B — design decision: failure signal + data flow

### Failure-result plumbing (non-QObject layer)

New file `libs/holonight-services/src/session/SessionCommandResult.h`:

```cpp
#pragma once

#include <QString>

// Outcome of a session-command dispatch (lock/logout/sleep/reboot/shutdown). `reason` is only
// meaningful when ok == false. CommandRunner::run() only reports launch success/failure (bool) —
// launches are detached/fire-and-forget (QProcess::startDetached), so there is no exit code to
// report — so `reason` is a human-readable description built from the program+args that failed
// to launch. Graceful no-ops (e.g. "no locker installed", "already locked") are ok == true:
// REQ-F-B.* requires signaling command DISPATCH failures, not the absence of work.
struct SessionCommandResult {
  bool ok{true};
  QString reason;

  [[nodiscard]] static SessionCommandResult success() { return {}; }
  [[nodiscard]] static SessionCommandResult failure(QString why) {
    return {.ok = false, .reason = std::move(why)};
  }
};
```

`Locker::lock()` changes from `void` to `[[nodiscard]] SessionCommandResult lock();`. Its three
branches:
- `Mode::Daemon`: `runner_->run("loginctl", {"lock-session"})` — return `failure("failed to launch loginctl lock-session")` if it returns `false`, else `success()`.
- `Mode::Locker`, already running: unchanged no-op, `success()`.
- `Mode::Locker`, spawning: `runner_->run(res.path, {})` — `failure(QStringLiteral("failed to launch %1").arg(res.name))` if `false`, else `success()`.
- `Mode::None`: unchanged graceful no-op, `success()`.

`SessionBackend::run()` (the protected helper both `sleep()/reboot()/shutdown()` and subclass
`logout()` overrides call) changes from `void` to `SessionCommandResult`, forwarding
`runner_->run()`'s bool into `success()`/`failure("failed to launch <program> <args>")`.
`SessionBackend::runLocker()` returns `locker_->lock()` directly. `sleep()/reboot()/shutdown()`
each return what `run()` returns. `HyprlandSessionBackend::logout()` returns what `run("hyprctl", …)`
returns; `LogindSessionBackend::logout()` (unsupported, currently a no-op) returns
`SessionCommandResult::success()` — it is a documented capability limitation already surfaced via
`logoutSupported()`, not a dispatch failure.

### Signal: chosen signature

```cpp
// SessionService.h, in the Q_SIGNALS: section
void commandFailed(const QString& action, const QString& reason);
```

`action` is one of the five lowercase command names used throughout the spec: `"lock"`, `"logout"`,
`"sleep"`, `"reboot"`, `"shutdown"`. `reason` is the `SessionCommandResult::reason` string.

Each `Q_INVOKABLE` method in `SessionService.cpp` becomes, e.g.:

```cpp
void SessionService::lockScreen() {
  const SessionCommandResult result = backend_->lockScreen();
  if (!result.ok) {
    emit commandFailed(QStringLiteral("lock"), result.reason);
  }
}
```

(and identically for `logout`/`sleep`/`reboot`/`shutdown`, with the matching action string). Public
method signatures on `SessionService` are unchanged (`void lockScreen()`, etc.) — QML/`.qml` call
sites need no changes.

**Why a signal `(QString action, QString reason)` over a `lastError`/`lastErrorChanged()` property**:
- A property models *state* ("the current error"), but a session-command failure is an *event* —
  there is no meaningful "current" failure to query later (nothing reads it after the fact), and a
  property would need extra plumbing to reset/clear (when? after N seconds? on next successful
  command?) purely to avoid it going stale. A signal has no such lifecycle question.
- Multiple rapid failures (e.g. user mashes the power-menu shutdown button twice while offline) are
  naturally each a separate signal emission; a property would either silently overwrite the first
  failure or need a queue, both add complexity a plain signal avoids.
- The existing codebase already uses this "fire happened, here is context" pattern for symmetrical
  cases — e.g. `NotificationServer`'s `NotificationClosed(uint notif_id, uint reason)` — so a signal
  is the more idiomatic and consistent choice.
- REQ-C-B.2 ("payload SHALL include both command name and failure reason") maps directly onto two
  signal parameters; a property would need to bundle them into a `QVariantMap` or a second
  `lastErrorAction` property, which is more surface area for the same information.

**Alternative considered and rejected**: `Q_PROPERTY(QString lastError READ lastError NOTIFY
lastErrorChanged)` plus a separate `lastErrorAction` property. Rejected per the bullets above —
event semantics fit a signal better, and two coupled properties (action + reason) that must always
update together is exactly what a two-argument signal expresses in one step.

### Data flow (step by step)

1. User (via QML `SessionService.lockScreen()` or the power-menu equivalents) invokes one of the
   five `Q_INVOKABLE` methods on `SessionService`.
2. `SessionService::lockScreen()` (etc.) calls `backend_->lockScreen()` (etc.), a virtual call into
   the active `SessionBackend` subclass (`HyprlandSessionBackend` or `LogindSessionBackend`).
3. `SessionBackend::lockScreen()` calls the protected `runLocker()`, which calls
   `locker_->lock()` — `Locker::lock()` runs its layered resolution (daemon → spawn locker →
   no-op) and calls `runner_->run(program, args)` for the daemon/spawn branches.
4. `CommandRunner::run()` (production: `DetachedCommandRunner::run()`, wrapping
   `QProcess::startDetached`) returns `false` if the process could not be started at all (e.g. the
   binary vanished from PATH between resolution and dispatch, or a `fork()`/`exec()` failure).
5. `Locker::lock()` turns that `false` into `SessionCommandResult::failure("failed to launch …")`
   and returns it up through `SessionBackend::runLocker()` → `SessionBackend::lockScreen()` (or the
   analogous `run()` path for `sleep()/reboot()/shutdown()`/`logout()`).
6. `SessionService::lockScreen()` receives the `SessionCommandResult`, sees `ok == false`, and emits
   `commandFailed(QStringLiteral("lock"), result.reason)`.
7. **Composition root** (`apps/shell/app/ShellApplication.cpp`, inside `startServices()`, placed
   next to the existing `notification_server_->start(); if (notification_server_->conflictDetected()) { … }`
   block around line 209–213) has, since both `session_` and `notification_server_` are constructed
   members of `ShellApplication` and both headers are already `#include`d:

   ```cpp
   connect(session_, &SessionService::commandFailed, this,
           [this](const QString& action, const QString& reason) {
     notification_server_->Notify(QStringLiteral("HoloNight Shell"), 0, QString(),
                                   QStringLiteral("Session command failed: %1").arg(action),
                                   reason, {}, {}, -1);
   });
   ```

8. `NotificationServer::Notify(...)` — already confirmed callable in-process without
   `calledFromDBus()` gating anything inside it — builds a `NotificationData` via
   `buildNotificationData(...)` and calls `service_->addOrReplace(std::move(data))` on the same
   `notification_service_` instance already wired into the rest of the shell. `addOrReplace()` runs
   the existing DND/per-app rule filter pipeline (`evaluateFilter()` in
   `NotificationService::addOrReplace`, `libs/holonight-services/src/notifications/NotificationService.cpp:201`)
   exactly as any other notification would, and the user sees a toast through the existing
   `ToastStack.qml` pipeline — no new QML surface needed.

**Why route through `NotificationServer::Notify()` rather than building a `NotificationData` and
calling `NotificationService::addOrReplace()` directly** (the pattern `IdleService.cpp` already uses
for its "no idle daemon detected" notification, ~line 160–167): the spec explicitly calls out
`NotificationServer::Notify()` as the confirmed-safe in-process entry point, and using it keeps
`ShellApplication`'s routing lambda decoupled from `NotificationData`'s internal field layout — it
only needs to speak the stable, public 8-argument protocol surface that any other notification
producer (including real D-Bus clients) uses, rather than reaching into
`libs/holonight-services/src/notifications/NotificationTypes.h` internals. Functionally the two paths
are equivalent (`Notify()` is a thin wrapper that itself calls `addOrReplace()`), so this is a
call-site ergonomics choice, not a behavioral one.

### Compile-time dependency check (REQ-C-B.1)

No file under `libs/holonight-services/src/session/` and neither `SessionService.h` nor
`SessionService.cpp` gains an `#include` of `NotificationServer.h`/`NotificationService.h`. The only
new `#include`-free coupling is the `connect()` call in `ShellApplication.cpp`, which already
included both headers before this change. Verified by:

```bash
grep -rn "Notification" libs/holonight-services/src/session/ libs/holonight-services/src/SessionService.h libs/holonight-services/src/SessionService.cpp
```
expected to return nothing.

---

## 4. Item C — design decision: helper file locations

### Why `libs/holonight-services/src/` (root), not a new subdirectory

`libs/holonight-services/CMakeLists.txt` globs sources with `file(GLOB_RECURSE ... CONFIGURE_DEPENDS
src/*.h src/*.cpp)`, so **any** new file anywhere under `src/` is picked up automatically (after a
`task configure` / `task configure-tests` re-run — no `CMakeLists.txt` source-list edit needed
either way). The difference that *does* matter is `target_include_directories`, which lists the root
`src/` plus a fixed set of subdirectories (`audio`, `calendar`, `brightness`, `idle`, `portal`, `mime`,
`kde-compat`, `session-integration`, `launcher`, `network`, `session`, `weather`, `weather-icon`,
`notifications`) so that unqualified `#include "Foo.h"` resolves regardless of which of those
directories `Foo.h`/the includer live in. Placing the two new helpers **directly in the root `src/`**
(already on that include path) means:
- Zero `CMakeLists.txt` changes of any kind (no new subdirectory to register).
- Both consumers — `idle/IdleService.cpp` + `brightness/SysfsBackend.cpp` for the logind helper,
  `ThemeService.cpp` (root) + `portal/SettingsPortalBackend.cpp` for the theme-path helper — can
  `#include` them unqualified, exactly like `idle/IdleService.cpp` already does today for the
  root-level `NotificationService.h`.

Concrete signatures:

```cpp
// libs/holonight-services/src/LogindSessionResolver.h
#pragma once
#include <QString>

[[nodiscard]] QString resolveActiveLogindSessionPath();
```

```cpp
// libs/holonight-services/src/ThemeConfigPath.h
#pragma once
#include <QString>

struct ThemeConfigPaths {
  QString dir_path;   // e.g. "<xdg-config-home>/holonight"
  QString file_path;  // e.g. "<xdg-config-home>/holonight/theme.conf"
};

[[nodiscard]] ThemeConfigPaths resolveHolonightThemeConfigPaths();
```

`resolveHolonightThemeConfigPaths()` returns **both** the directory and file path as a small
value type (rather than deriving `dir_path` from `file_path` at each call site via, say,
`QFileInfo::absolutePath()`) specifically to avoid any path-normalization edge case (e.g. a stray
trailing slash in `$XDG_CONFIG_HOME`) changing the exact string `ThemeService` previously computed
and fed to `QFileSystemWatcher::addPath()`. This is a literal, byte-for-byte port of the existing
two-line construction (`xdg + "/holonight"`, then `+ "/theme.conf"`) into one place — REQ-NF-C.1
("identical output ... same fallback logic") is satisfied by construction, not by re-derivation.

Call-site updates:

```cpp
// ThemeService.cpp
void ThemeService::resolveThemeConfigPath() {
  const ThemeConfigPaths paths = resolveHolonightThemeConfigPaths();
  theme_config_dir_path_ = paths.dir_path;
  theme_config_path_ = paths.file_path;
}
```

```cpp
// SettingsPortalBackend.cpp — the anonymous-namespace themeConfigPath() function is deleted;
// its one call site becomes:
QSettings settings{resolveHolonightThemeConfigPaths().file_path, QSettings::IniFormat};
```

```cpp
// IdleService.cpp — subscribeLockedHint() keeps its QDBusInterface/connect/readInitialLockedHint
// logic but replaces the inline GetSessionByPID+loginctl block with:
session_path_ = resolveActiveLogindSessionPath();
if (session_path_.isEmpty()) {
  qCInfo(lcIdleService) << "Could not determine logind session path; LockedHint unavailable";
  return;
}
```

```cpp
// SysfsBackend.cpp — resolveSessionPath() becomes:
void SysfsBackend::resolveSessionPath() {
  session_path_ = resolveActiveLogindSessionPath();
  if (session_path_.isEmpty()) {
    qCWarning(lcBrightness) << "Could not resolve logind session path; brightness writes disabled";
  }
}
```

`resolveActiveLogindSessionPath()`'s internal implementation is the GetSessionByPID call followed
by the loginctl-fallback block, moved verbatim from either duplicate (they were byte-for-byte
identical except for the surrounding class/logging context). The one intentional, called-out
deviation: the `qCInfo(lcIdleService) << "GetSessionByPID unavailable (user service scope); resolved
session via loginctl: " << session_id` success-path log line (previously slightly differently
worded between the two sites) moves *into* the shared helper under a new logging category
(`holonight.dbus.session`) and is emitted once, identically, regardless of caller. This changes log
*text* only — not the returned path, not the control flow, not the two callers' own
empty-path failure logging (which stays local to each, at whatever severity fits: `qCInfo` in
`IdleService`, `qCWarning` in `SysfsBackend`) — so it does not violate REQ-NF-C.1's "same paths, same
fallback logic, same error handling" scope, but implementers should be aware of it.

**Alternative considered for file location**: a new subdirectory, e.g. `libs/holonight-services/src/dbus/`
housing both helpers (mirroring how `session/`, `portal/`, etc. are organized) — considered because it
groups "shared D-Bus/OS-probe helpers" under one conceptual roof rather than mixing them in with the
`Service`-suffixed QObject files at `src/` root. Rejected for Phase 1 because it requires adding
`${CMAKE_CURRENT_SOURCE_DIR}/src/dbus` to `target_include_directories` in
`libs/holonight-services/CMakeLists.txt` for no functional benefit — the root `src/` include path
already covers every current and (barring a much larger reorg) future consumer, and Phase 1's scope
is explicitly a minimal, behavior-preserving de-duplication, not a directory reorganization. If a
third duplicate of either concern shows up later, promoting these two files into a `dbus/` or `xdg/`
subdirectory is a cheap follow-up.

**Alternative considered for API shape (logind helper)**: accepting an injected `QDBusConnection` or
a `ProcessEnvironment`-style seam so the function becomes unit-testable with a fake bus/process
runner. Rejected for Phase 1 — REQ-C-C.1/REQ-NF-C.1 scope this as a pure extraction (no behavior
change), and neither existing call site had such a seam before; introducing one is a larger,
separately-reviewable change than "move this code, don't change it."

---

## 5. Key decisions summary

| # | Question | Decision |
|---|---|---|
| 1 | `ExtWorkspaceManager` guard ordering | Guard first, full early `return;` — all five constructor statements (including the two `model_`-only connects) run only when `config != nullptr`. Object is fully inert (not partially wired) when the guard fires; always safely destructible. |
| 2 | Item B signal shape | `void commandFailed(const QString& action, const QString& reason)` on `SessionService`, fed by a new `SessionCommandResult{bool ok; QString reason;}` value type threaded through `Locker::lock()`/`SessionBackend::run()`/subclass overrides. Routed at the composition root via `NotificationServer::Notify()`. |
| 3 | Item C file locations | `libs/holonight-services/src/LogindSessionResolver.{h,cpp}` and `libs/holonight-services/src/ThemeConfigPath.{h,cpp}` — root of `src/`, no new subdirectory, no `CMakeLists.txt` include-path edits needed. |

---

## 6. Known risks

- **CMake `CONFIGURE_DEPENDS` staleness**: all four new `holonight-services` source files (Items B
  needs none; Item C needs the two `.h`/`.cpp` pairs) rely on `file(GLOB_RECURSE CONFIGURE_DEPENDS)`
  to be picked up. Per the project's known Ninja/ CMake gotcha, a bare `task build` after adding files
  can silently miss them if the configure step doesn't re-run first — always `task configure-tests`
  (or `task configure`) after adding the new files, before building.
- **Architecture-boundary script**: `scripts/check-architecture-boundaries.sh` only greps
  `#include "..."` lines inside `libs/holonight-surfaces/src` against header basenames that exist
  under `libs/holonight-services/src`. None of Items A/B/C touch `libs/holonight-surfaces/`, and none
  of the new/changed headers are included from there — the script's behavior is unaffected and no
  entry needs to be added to `allowed_surface_service_includes`.
- **moc / Q_OBJECT**: none of the six new files (Item B's `SessionCommandResult.h`; Item C's
  `LogindSessionResolver.{h,cpp}` and `ThemeConfigPath.{h,cpp}`) declare a `QObject`-derived class or
  use `Q_OBJECT` — all are plain structs/free functions per the spec's framing, so no
  `#include "*.moc"` requirement applies and `AUTOMOC` has nothing to do for them.
- **`SessionCommandResult` return-type churn**: changing `Locker::lock()`, `SessionBackend::run()`/
  `runLocker()`/`sleep()`/`reboot()`/`shutdown()`, and both backend subclasses' `logout()`/
  `lockScreen()` overrides from `void` to `SessionCommandResult` touches six files in one item; get
  the header (`SessionCommandResult.h`) landed and compiling standalone first, then update the
  chain bottom-up (`Locker` → `SessionBackend` → subclasses → `SessionService`) to keep each
  intermediate compile step small.
- **Theme-path log-line unification** (noted in §4): the one `qCInfo` success-path log message
  inside the new `resolveActiveLogindSessionPath()` will read identically from both call sites,
  whereas today `IdleService` and `SysfsBackend` word it slightly differently. This is a text-only
  change, flagged here so it isn't mistaken for a missed behavior-preservation requirement.
- **`ExtWorkspaceManager` test cost**: there is no existing test target exercising
  `ExtWorkspaceManager` or `ShellApplication` at all today (`grep` of `tests/CMakeLists.txt` finds
  neither). Item A's two new test files are the first coverage for either class — see Test Plan for
  how their scope is kept minimal (smoke-style, not full behavioral coverage of either class).

---

## 7. Test plan alignment

| Item | Test file | New/existing | What it covers |
|---|---|---|---|
| A (`ShellApplication`) | `tests/test_shell_application.cpp` | **New** — add to `test_holonight_app` in `tests/CMakeLists.txt` | Constructs a real `ShellApplication`, calls `startShell()` *before* `registerQmlTypes()`/`startServices()` and asserts the process does not abort (regression check vs. the old `Q_ASSERT`, which aborts even in default GTest Debug builds); then calls `registerQmlTypes(); startServices(); startShell();` in the correct order and asserts that succeeds too (proves the guarded early return left no latched bad state — `shell_started_` was not set to `true` on the guarded path). |
| A (`ExtWorkspaceManager`) | `tests/test_ext_workspace_manager.cpp` | **New** — add to `test_holonight_core` in `tests/CMakeLists.txt` | Constructs `ExtWorkspaceManager(model, /*config=*/nullptr, parent)` under the offscreen QPA test harness and asserts no crash/abort, and that the object is destructible without incident. Does not assert on `qCritical` log text (no existing precedent in this test suite for message-content capture; the functional contract — "does not dereference null, stays destructible" — is what REQ-F-A.2 actually requires). |
| B | `tests/test_session_service.cpp` | **Existing file, extended** | Extend the file-local `SpyCommandRunner` with `setShouldFail(bool)`. Add five new `TEST`s (`LockFailureEmitsCommandFailed`, `LogoutFailureEmitsCommandFailed`, `SleepFailureEmitsCommandFailed`, `RebootFailureEmitsCommandFailed`, `ShutdownFailureEmitsCommandFailed`), each constructing a `SessionService` over a failing `SpyCommandRunner`, using `QSignalSpy` on `SessionService::commandFailed`, and asserting exactly one emission with the right action string and a non-empty reason. Add one regression test (`SuccessfulCommandsDoNotEmitCommandFailed`) confirming the existing passing-path tests in this file continue to see zero emissions. No new test infrastructure file — satisfies REQ-NF-B.1. |
| C (logind resolver) | `tests/test_logind_session_resolver.cpp` | **New** — add to `test_holonight_services` in `tests/CMakeLists.txt` | Smoke test only: calls `resolveActiveLogindSessionPath()` twice in immediate succession and asserts the two results are equal (idempotent re-probe), since no live logind session is guaranteed in the CI/offscreen environment and the function's real strategy involves live D-Bus + a `loginctl` subprocess that cannot be mocked without introducing a new seam (explicitly out of scope per §4's rejected alternative). Regression coverage for "same output as before" instead comes from `tests/test_idle_service.cpp` and `tests/test_brightness_service.cpp` continuing to pass unchanged (neither currently exercises `subscribeLockedHint()`/`resolveSessionPath()` directly — confirmed via `grep` — so the refactor risk to those suites is limited to compilation, not behavior assertions). |
| C (theme path) | `tests/test_theme_config_path.cpp` | **New** — add to `test_holonight_services` in `tests/CMakeLists.txt` | Fully deterministic: `qputenv("XDG_CONFIG_HOME", ...)` then assert `resolveHolonightThemeConfigPaths()` returns the expected `dir_path`/`file_path`; `qunsetenv("XDG_CONFIG_HOME")` then assert the `QDir::homePath() + "/.config/holonight[...]"` fallback. This directly satisfies REQ-F-C.2's "new test verifies same output." |

Grep-based verification for both Item C requirements (no remaining duplicate logic):

```bash
# Logind session resolution — should show exactly one implementation (LogindSessionResolver.cpp)
grep -rln "GetSessionByPID" libs/holonight-services/src/

# Theme config path — should show exactly one implementation (ThemeConfigPath.cpp)
grep -rln "holonight/theme.conf" libs/holonight-services/src/
```

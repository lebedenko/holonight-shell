# POC Remediation Phase 1 Specification

## Overview

This specification addresses three discrete, scoped remediation items identified in the POC Readiness Audit (`docs/sdd/poc-readiness-review/REPORT.md`) and prioritized through Stage 0 Grill discussion. Phase 1 consists of:

- **Item A**: Replace two `Q_ASSERT` sites with release-mode-visible guards.
- **Item B**: Add failure signaling and user-visible notifications for session commands.
- **Item C**: Extract two byte-for-byte duplicate helper functions.

Each item is independent and can be implemented/verified in parallel. This specification locks the *requirement* outcome and acceptance criteria, while deferring implementation details (Stage 2 Design decisions) listed at the end.

---

## Item A: Release-Mode-Silent Q_ASSERT Replacement

### Background

Two invariant checks currently use `Q_ASSERT`, which compiles to nothing in Release/NDEBUG builds, causing silent undefined behavior instead of a visible failure.

### Requirements

**REQ-F-A.1: Guard ShellApplication construction sequence violation**

WHEN `ShellApplication::startShell()` is called before `registered_` or `services_started_` flags are true, the system SHALL log a critical-level message and return early without initializing `layer_shell_`, `layer_shell_manager_`, or `background_manager_`.

- **Rationale**: At this call site (~line 247 of `apps/shell/app/ShellApplication.cpp`), the referenced flags have not yet been set. An early return is a safe no-op because no construction has occurred yet.
- **Verified by**: Unit test asserts that calling `startShell()` before flags are set produces a `qCritical` log message and leaves all three managers unconstructed.

**REQ-F-A.2: Guard ExtWorkspaceManager null-config violation**

WHEN `ExtWorkspaceManager` is constructed with `config == nullptr`, the system SHALL log a critical-level message and return early from the constructor body, preventing any dereference of `config` or connection of config-change signals.

- **Rationale**: The constructor (~line 102 of `libs/holonight-core/src/ExtWorkspaceManager.cpp`) checks `Q_ASSERT(config != nullptr)` before dereferencing `config->barWorkspaces()` and wiring `connect(config, &ConfigService::barWorkspacesChanged, ...)`. An early `return;` inside the constructor stops the remainder of construction but leaves the object in a degraded but safe state.
- **Verified by**: Unit test asserts that constructing `ExtWorkspaceManager` with `config == nullptr` produces a `qCritical` log message and prevents config-dependent operations.

**REQ-NF-A.1: Release-mode visibility**

The guards SHALL use logging facilities (e.g., `qCritical()`) that produce output in both Debug and Release builds, not `Q_ASSERT` which silently compiles away.

- **Verified by**: Grep confirms no remaining `Q_ASSERT` in the two files and locations identified; `qCritical` appears in both code paths.

**REQ-C-A.1: Early return feasibility**

The two sites MUST support early `return;` without leaving the caller in an inconsistent state.

- **Rationale**: `ShellApplication::startShell()` is a regular method (early return is a clean no-op). `ExtWorkspaceManager()` is a constructor (early return stops construction but the object remains safely destructible).
- **Verified by**: Code review confirms both early returns are safe per the rationale above.

---

## Item B: Session Command Failure Signaling

### Background

All five `SessionService` commands (lock, logout, sleep, reboot, shutdown) currently fail silently end-to-end (Audit Finding U-04/F-03, Severity High). Failures in `SessionBackend::run()` and `Locker::lock()` are caught but discarded, producing no user-visible feedback.

### Requirements

**REQ-F-B.1: Detect and report lock command failure**

WHEN the lock command (triggered via `SessionService::lock()`) fails in `Locker::lock()` or `SessionBackend::run()`, the system SHALL capture the failure reason (exit code, error message, or D-Bus error) and emit a failure signal carrying the command name and reason.

- **Verified by**: Unit test using `SpyCommandRunner` asserts that a lock command failure emits the failure signal with `"lock"` and a non-empty reason string.

**REQ-F-B.2: Detect and report logout command failure**

WHEN the logout command (triggered via `SessionService::logout()`) fails, the system SHALL capture the failure reason and emit a failure signal carrying the command name and reason.

- **Verified by**: Unit test using `SpyCommandRunner` asserts logout failure is signaled.

**REQ-F-B.3: Detect and report sleep/suspend command failure**

WHEN the sleep command (triggered via `SessionService::sleep()`) fails, the system SHALL capture the failure reason and emit a failure signal carrying the command name and reason.

- **Verified by**: Unit test using `SpyCommandRunner` asserts sleep failure is signaled.

**REQ-F-B.4: Detect and report reboot command failure**

WHEN the reboot command (triggered via `SessionService::reboot()`) fails, the system SHALL capture the failure reason and emit a failure signal carrying the command name and reason.

- **Verified by**: Unit test using `SpyCommandRunner` asserts reboot failure is signaled.

**REQ-F-B.5: Detect and report shutdown command failure**

WHEN the shutdown command (triggered via `SessionService::shutdown()`) fails, the system SHALL capture the failure reason and emit a failure signal carrying the command name and reason.

- **Verified by**: Unit test using `SpyCommandRunner` asserts shutdown failure is signaled.

**REQ-F-B.6: Emit failure signal from SessionService**

The system SHALL emit a signal from `SessionService` (or an associated service tier) carrying sufficient context to identify which command failed (lock, logout, sleep, reboot, shutdown) and the failure reason (exit code or error string), allowing the caller to route the failure to user-visible UI.

- **Rationale**: The exact signal name, signature, and whether it is a `commandFailed(QString action, QString reason)` property-change pattern or other variant is deferred to Stage 2 Design. This requirement locks only that the signal must exist and carry both pieces of information.
- **Verified by**: Grep and code inspection confirm the signal exists, accepts the command name and reason, and is emitted at all five failure points.

**REQ-F-B.7: Route failure signal to user-visible notification**

WHEN a session command failure signal is emitted, the system SHALL route the failure through the composition root (`ShellApplication`) to the notification stack (`NotificationServer` / `NotificationService`) so the user sees a toast/notification message.

- **Rationale**: `NotificationServer::Notify()` is safe to call in-process (no `calledFromDBus()` dependency). The routing glue code is the responsibility of the composition root, not the SessionService itself.
- **Verified by**: Integration test verifies that a failure signal in `SessionService` reaches the notification system and produces a visible notification message.

**REQ-NF-B.1: Leverage existing test seam**

The implementation SHALL use the existing `SpyCommandRunner` test seam in `tests/test_session_service.cpp` to verify failure detection, rather than creating new test infrastructure.

- **Rationale**: `SpyCommandRunner` is already capable of recording command execution and failure; reusing it reduces test duplication.
- **Verified by**: `tests/test_session_service.cpp` contains new or updated tests using `SpyCommandRunner` to assert each of the five failure signals fire.

**REQ-C-B.1: No new inter-service compile-time dependency**

The solution SHALL NOT introduce a direct `#include` dependency from `SessionService` (or `SessionBackend`) to `NotificationServer` or `NotificationService` at compile time.

- **Rationale**: Architectural principle: inter-service communication flows through the composition root (`ShellApplication`) via Qt signals/slots, not direct includes.
- **Verified by**: Build succeeds; `grep -r "#include.*Notification" libs/holonight-services/src/session/` returns no new includes from `SessionService` or `SessionBackend`.

**REQ-C-B.2: Signal must carry actionable context**

The failure signal's payload SHALL include both the command name (to distinguish which of the five commands failed) and the failure reason (exit code or error message, to distinguish why).

- **Verified by**: Code inspection and unit tests confirm the signal arguments include both pieces of information.

---

## Item C: Shared Helper Extraction

### Background

Two independent, duplicate implementations of the same logic have been identified:

1. **logind active-session resolution** (Audit Finding): Functions `subscribeLockedHint()` in `libs/holonight-services/src/idle/IdleService.cpp` (~lines 172-215) and `resolveSessionPath()` in `libs/holonight-services/src/brightness/SysfsBackend.cpp` (~lines 82-116) both implement the project's UWSM-safe logind session-resolution workaround documented in CLAUDE.md.

2. **Theme config path resolution** (Audit Finding): Functions `resolveThemeConfigPath()` in `libs/holonight-services/src/ThemeService.cpp` (~lines 17-24) and `themeConfigPath()` in `libs/holonight-services/src/portal/SettingsPortalBackend.cpp` (~lines 18-32) both compute `$XDG_CONFIG_HOME` (or `~/.config` fallback) + `/holonight/theme.conf`.

### Requirements

**REQ-F-C.1: Extract logind session-resolution helper**

The system SHALL extract the duplicated logind active-session resolution logic into a single shared free function, consolidating the implementations currently in `IdleService::subscribeLockedHint()` and `SysfsBackend::resolveSessionPath()`.

- **Rationale**: Both implementations are byte-for-byte identical (or near-identical) and implement the UWSM-safe workaround: avoid `GetSessionByPID` and `ListSessions`, use `loginctl show-seat --property=ActiveSession --value seat0` via `QProcess`, then `GetSession(id)`.
- **Verified by**:
  - Grep confirms no remaining duplicate session-resolution logic in either file.
  - All existing unit tests for `IdleService` and `SysfsBackend` continue to pass with identical assertions.
  - New or updated unit tests verify the extracted helper produces the same session path as the prior duplicated code.

**REQ-F-C.2: Extract theme config-path helper**

The system SHALL extract the duplicated theme config-path logic into a single shared free function, consolidating the implementations currently in `ThemeService::resolveThemeConfigPath()` and `SettingsPortalBackend::themeConfigPath()`.

- **Rationale**: Both implementations compute `$XDG_CONFIG_HOME` (with `~/.config` fallback) + `/holonight/theme.conf` identically.
- **Verified by**:
  - Grep confirms no remaining duplicate theme config-path logic in either file.
  - All existing unit tests for `ThemeService` and `SettingsPortalBackend` continue to pass with identical assertions.
  - New or updated unit tests verify the extracted helper produces the same config path as the prior duplicated code.

**REQ-NF-C.1: Preserve exact behavior during refactor**

The extracted helpers SHALL produce identical output (same paths, same fallback logic, same error handling) as the prior duplicated implementations.

- **Rationale**: This is a refactor/de-duplication, not a behavior change.
- **Verified by**:
  - All prior tests pass unmodified in outcome (test file content may be updated only to reference the extracted helper).
  - New comparative unit tests feed identical inputs to the old and new implementations (where feasible within refactor scope) and assert identical outputs.

**REQ-C-C.1: De-duplication scope boundary**

The extraction SHALL NOT modify or touch `libs/holonight-services/src/mime/MimeService.cpp` or its `xdgConfigHome()` helper.

- **Rationale**: `MimeService::xdgConfigHome()` is a separate, more generic XDG-base-dir helper used for a different file (`mimeapps.list`). It is not the same duplication as the theme config-path issue.
- **Verified by**: `MimeService.cpp` is not modified in this cycle; no commits touch it for this Item C.

---

## Non-Goals

The following findings and scope items are explicitly OUT of scope for Phase 1:

1. **Audit Finding U-07 (CalDAV timeout)**: Already addressed independently in Phase 0. Not included in Item B.
2. **Other silent-failure findings**: The audit identified 7 additional silent-failure-path gaps beyond Item B's session command failures. These are NOT addressed in Phase 1.
3. **MimeService `xdgConfigHome()`**: A separate, generic XDG-base-dir helper used for different files (`mimeapps.list`). Not part of Item C's theme config-path de-duplication.
4. **Behavior changes during Item C refactors**: Item C is purely de-duplication; no functional changes to the output of the extracted helpers.

---

## Deferred to Stage 2 (Design)

The following design questions are explicitly NOT resolved by this specification and SHALL be addressed during the Design phase:

1. **Item A — ExtWorkspaceManager constructor statement ordering**:
   - The spec requires early `return;` on null `config` to prevent dereferencing.
   - The *exact order* of statements inside the constructor (which config-dependent connections execute before vs. after the guard, and which model-independent connects always execute) is a Design-stage decision.
   - Outcome required: null `config` must not be dereferenced; the object must be safely destructible and usable in whatever degraded capacity is decided.

2. **Item B — Session failure signal name and signature**:
   - The spec requires a failure signal carrying command name (lock/logout/sleep/reboot/shutdown) and failure reason (exit code or error message).
   - The *exact signal name* (e.g., `commandFailed`, `sessionCommandFailed`, `sessionError`, etc.) and *signature* (e.g., `commandFailed(QString action, QString reason)` vs. a property-change pattern `lastErrorChanged()` with a `lastError` property) are Design-stage decisions.
   - Outcome required: a signal or property-change mechanism must exist and emit the two pieces of information.

3. **Item C — Shared helper file location(s)**:
   - The spec requires extracting two shared free functions (logind session-resolution and theme config-path helpers).
   - The *exact file name and location* for each extracted helper (e.g., alongside an existing `DbusPropertyClient` utility, in a new dedicated helpers module, in the service library's public headers, etc.) is a Design-stage decision.
   - Outcome required: each helper must be extracted from its duplicated call sites, consolidated, and callable from both original locations.

---

## Acceptance Criteria Summary

| Item | Acceptance Criterion |
|------|----------------------|
| **A.1** | Unit test: `startShell()` before flag-set produces `qCritical` log; managers unconstructed. |
| **A.2** | Unit test: null `config` constructor produces `qCritical` log; config not dereferenced. |
| **A** | Grep: no remaining `Q_ASSERT` at both sites; `qCritical` calls present. |
| **B.1–B.5** | Unit test with `SpyCommandRunner`: each of five command failures emits signal. |
| **B.6–B.7** | Integration test: failure signal reaches `NotificationServer` and produces notification. |
| **B** | Grep: no new `#include` from `SessionService`/`SessionBackend` to Notification code. |
| **C.1** | Grep: no duplicate logind session-resolution logic in either `IdleService` or `SysfsBackend`. |
| **C.2** | Grep: no duplicate theme config-path logic in either `ThemeService` or `SettingsPortalBackend`. |
| **C** | All prior unit tests pass unmodified in outcome; new tests verify extracted helper output matches original. |

# Session Lock Backend Refactoring — EARS Specification

## Overview

`SessionService` currently hardcodes Hyprland and systemd APIs. This specification defines a layered backend architecture that abstracts the compositor (Hyprland first, generic logind fallback) and introduces a multi-strategy lock handler that prefers integrated paths (loginctl) over spawning lock daemons, with graceful fallback when no locker is available.

HoloNight is the control plane over system daemons — never a security implementation. All actions delegate to the active backend; detection is automatic via environment variables.

---

## Non-Goals

- Installing, packaging, or configuring `hypridle`, `hyprlock`, or any lock daemon.
- Generating or validating lock daemon configuration files.
- Implementing a custom locker, idle detector, or suspend handler.
- Live "is locked now" state tracking or idle-state subscription (future `idle-management` pipeline).
- Production Sway, Niri, or other compositor backends; the interface must permit them, but only Hyprland and generic logind are implemented.

---

## Requirements

### Backend Abstraction & Auto-Detection

**REQ-F-001** Compositor backend dispatch
>
> The system shall detect the active compositor at startup by testing the `HYPRLAND_INSTANCE_SIGNATURE` environment variable. If set and non-empty, the Hyprland backend shall be instantiated; otherwise the generic logind backend shall be instantiated. This decision shall be cached and used for all subsequent session operations.
>
> Acceptance: Verify via unit test or source inspection that backend selection is determined once at SessionService construction and does not re-detect on each action invocation.

**REQ-F-002** Backend interface definition
>
> The system shall define a `SessionBackend` abstract base class. `logout()` and `lockScreen()` shall be pure virtual (compositor-specific). `sleep()`, `reboot()`, and `shutdown()` shall be implemented once as non-virtual methods in the base class, since they issue identical `systemctl` commands on every backend (see REQ-F-010..012); they are NOT overridable. Concrete backends (HyprlandSessionBackend, LogindSessionBackend) inherit the common power actions and implement the two compositor-specific methods plus the capability accessors.
>
> Acceptance: Verify that `SessionService::logout()`, `lockScreen()`, `sleep()`, `reboot()`, `shutdown()` all delegate to the active backend without conditional (per-compositor `if`) logic at the call site. Verify that `sleep()`/`reboot()`/`shutdown()` have a single implementation in `SessionBackend` and are not redefined in either concrete backend.

**REQ-C-001** Backend constancy
>
> Once instantiated at SessionService startup, the active backend shall not change for the lifetime of the session.
>
> Acceptance: Verify that the backend pointer is assigned once in SessionService::SessionService() and marked const or read-only thereafter.

---

### Logout Action (Compositor-Specific)

**REQ-F-003** Hyprland logout
>
> When the Hyprland backend is active and logout() is invoked, the system shall execute `hyprctl dispatch exit` via QProcess.
>
> Acceptance: Observe in a live Hyprland session that calling SessionService::logout() exits the compositor (kills the shell process and returns to the login screen).

**REQ-F-004** Logind backend logout unsupported
>
> When the logind backend is active, logout() shall perform no action (no error, no exception, silent no-op).
>
> Acceptance: Verify via unit test with a mocked logind backend that logout() returns without invoking any system command.

**REQ-F-005** Logout capability reporting
>
> The system shall expose a read-only Q_PROPERTY `logoutSupported : bool` on SessionService. Under Hyprland, logoutSupported shall be true. Under logind, logoutSupported shall be false. This property shall not change after startup.
>
> Acceptance: Unit test or manual inspection confirms: Hyprland backend sets logoutSupported=true on initialization, logind backend sets it to false. Verify QML can read the property via `SessionService.logoutSupported`.

---

### Lock Screen Action (Layered Strategy)

**REQ-F-006** Idle daemon integration
>
> When lockScreen() is invoked, the system shall first check if any of the following processes are running: `hypridle`, `swayidle`, or `xss-lock` (via pidof or /proc inspection). If any is found running, the system shall invoke `loginctl lock-session` and return; no locker shall be spawned.
>
> Acceptance: In a live session with hypridle running, call lockScreen() and verify that the session is locked and `loginctl lock-session` was the only system call made (no hyprlock/swaylock process spawned). Confirm via `loginctl show-session -p LockedHint` that LockedHint is set.

**REQ-F-007** PATH-based locker fallback
>
> If no idle daemon is running, the system shall search PATH for lock handler executables in this priority order: `hyprlock`, `swaylock`, `gtklock`, `waylock`. The first found shall be spawned via QProcess with no arguments.
>
> Acceptance: Unit test with mocked PATH and QProcess: verify that when hypridle is not running and PATH contains ["/usr/bin/swaylock", "/usr/bin/gtklock"], the system resolves to swaylock and spawns it. Verify that when PATH is empty or contains none of these, no process is spawned.

**REQ-F-008** Locker idempotency
>
> If lockScreen() is invoked while the chosen locker process is already running (pidof confirms it), no new instance shall be spawned.
>
> Acceptance: Unit test with mocked pidof: call lockScreen() twice in sequence with the mocked locker already running; verify QProcess::start() is not called on the second invocation.

**REQ-F-009** Graceful no-locker handling
>
> If no lock handler (idle daemon or locker) can be invoked, lockScreen() shall complete without error, without spawning anything, and without logging as a crash or failure.
>
> Acceptance: Unit test with mocked hypridle/pidof returning "not running" and empty PATH: call lockScreen() and verify it returns normally with no exception or logged error (diagnostic properties lockerAvailable and lockerName reflect the absence, but the call itself succeeds).

---

### Sleep, Reboot, Shutdown Actions (Common Systemctl/Logind)

**REQ-F-010** Sleep implementation
>
> When sleep() is invoked on any backend, the system shall execute `systemctl suspend` via QProcess.
>
> Acceptance: Observe in a live session (Hyyrland or other) that calling sleep() suspends the system (session returns after wake). Verify command matches pre-refactor behavior.

**REQ-F-011** Reboot implementation
>
> When reboot() is invoked on any backend, the system shall execute `systemctl reboot` via QProcess.
>
> Acceptance: Observe in a live session that calling reboot() initiates a system reboot (or deny access via polkit if unprivileged). Verify command matches pre-refactor behavior.

**REQ-F-012** Shutdown implementation
>
> When shutdown() is invoked on any backend, the system shall execute `systemctl poweroff` via QProcess.
>
> Acceptance: Observe in a live session that calling shutdown() initiates a system poweroff (or deny access via polkit if unprivileged). Verify command matches pre-refactor behavior.

---

### Status Properties & Diagnostics

**REQ-F-013** Backend name exposure
>
> The system shall expose a read-only Q_PROPERTY `backendName : QString` on SessionService. Its value shall be "hyprland" if the Hyprland backend is active, or "logind" if the generic logind backend is active.
>
> Acceptance: Unit test or QML verification: inspect SessionService.backendName under mocked Hyprland and mocked logind environments; confirm the string values match expected names.

**REQ-F-014** Locker availability reporting
>
> The system shall expose a read-only Q_PROPERTY `lockerAvailable : bool` on SessionService. It shall be true if at least one of [hypridle, swayidle, xss-lock] is running OR if at least one of [hyprlock, swaylock, gtklock, waylock] is found on PATH; false otherwise.
>
> Acceptance: Unit test with mocked PATH and process lookup: verify lockerAvailable = true when hyprlock is on PATH, true when hypridle is running, false when neither condition holds.

**REQ-F-015** Resolved locker name
>
> The system shall expose a read-only Q_PROPERTY `lockerName : QString` on SessionService. Its value shall be the name of the idle daemon currently running (e.g., "hypridle") if one is detected, or the name of the locker resolved from PATH (e.g., "hyprlock") if none is running, or an empty string if no handler is available.
>
> Acceptance: Unit test with mocked lookups: verify lockerName = "hypridle" when hypridle is running, lockerName = "hyprlock" when not running but hyprlock is on PATH, lockerName = "" when neither condition holds.

---

### Process Control & Robustness

**REQ-NF-001** QProcess lifecycle management
>
> All system calls (QProcess::start for locker, systemctl, hyprctl) shall be managed with QProcess lifecycle patterns that prevent zombie processes and handle start failures gracefully (emit a signal or log, but do not throw).
>
> Acceptance: Run shell under valgrind or inspect /proc/[pid]/children after invoking all five actions; verify no zombie processes remain. Verify that QProcess::started() signal is connected before each start() call.

**REQ-NF-002** Unit testability
>
> The backend abstraction and lock handler logic shall be unit-testable without a live Wayland session by providing seams for: PATH resolution (injectable, default std::getenv + QStandardPaths), process lookup (pidof or /proc reader, injectable), and QProcess interaction (mockable or spy-able).
>
> Acceptance: Unit test source inspection: verify that a test file can mock PATH lookup and pidof/process detection and exercise all branches of the lock handler (idle daemon found, locker found, nothing found, locker already running).

**REQ-NF-003** Backward compatibility
>
> The five public methods on SessionService (logout, lockScreen, sleep, reboot, shutdown) shall have identical signatures and return types (void or Q_INVOKABLE void) to pre-refactor code. QML callers shall not require changes.
>
> Acceptance: Grep for SessionService:: method calls in all QML files; verify that no new parameters are needed and that the existing callsites remain valid.

---

### Constraints & Assumptions

**REQ-C-002** No sudo or privilege escalation
>
> The system shall not invoke sudo, setuid, or any privilege escalation mechanism. All actions shall be submitted to system daemons (systemctl, hyprctl, loginctl) which enforce privilege via polkit or user session membership.
>
> Acceptance: Verify via strace or source inspection that no setuid, sudo, or CAP_SYS_* syscalls appear in any backend code path.

**REQ-C-003** No hardcoded daemon paths
>
> Executable paths for lockers and daemons (hyprlock, swaylock, hypridle, etc.) shall be resolved via PATH search, not hardcoded full paths.
>
> Acceptance: Source inspection: verify that QStandardPaths::findExecutable or equivalent is used for locker resolution; verify that no string literal like "/usr/bin/hyprlock" appears in the backend code.

**REQ-C-004** Compositor isolation
>
> Hyprland-specific invocations (hyprctl dispatch exit) shall be confined to HyprlandSessionBackend and shall not appear in SessionService or LogindSessionBackend.
>
> Acceptance: Grep for "hyprctl" in SessionService.cpp and LogindSessionBackend.cpp; should only appear in HyprlandSessionBackend.cpp.

---

## Testing Strategy Summary

- **Unit tests (GTest)**: mock PATH resolution, process detection, QProcess, logind DBus stubs; exercise all lock-handler branches without a live session.
- **Integration tests**: in a live Hyprland session with hypridle running, verify lockScreen() → loginctl lock-session. In the same session, kill hypridle, verify subsequent lockScreen() → locker spawn.
- **Regression tests**: verify sleep(), reboot(), shutdown() still invoke correct systemctl commands.


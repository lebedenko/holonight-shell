# SDD Tasks — session-lock-backend

- [x] T-001: Implement ProcessEnvironment seam interface
  - REQs: REQ-NF-002, REQ-C-003
  - Check: `grep -r "class ProcessEnvironment" src/services/session/` returns ProcessEnvironment.h with pure virtual `isRunning(const QString&)` and `findExecutable(const QString&)`; `grep "SystemProcessEnvironment" src/services/session/ProcessEnvironment.h` shows concrete impl declaration.

- [x] T-002: Implement SystemProcessEnvironment with /proc scanning and QStandardPaths
  - REQs: REQ-NF-002, REQ-C-003
  - Check: `grep -n "QDir.*proc" src/services/session/ProcessEnvironment.cpp` finds /proc scan; `grep "QStandardPaths::findExecutable" src/services/session/ProcessEnvironment.cpp` confirms PATH search; no hardcoded `/usr/bin` paths in the file.

- [x] T-003: Implement CommandRunner seam interface
  - REQs: REQ-NF-001, REQ-NF-002
  - Check: `grep -r "class CommandRunner" src/services/session/` returns CommandRunner.h with pure virtual `run(const QString&, const QStringList&)`; `grep "DetachedCommandRunner" src/services/session/CommandRunner.h` shows concrete impl declaration.

- [x] T-004: Implement DetachedCommandRunner with QProcess::startDetached
  - REQs: REQ-NF-001
  - Check: `grep "startDetached" src/services/session/CommandRunner.cpp` appears; `grep "qCWarning" src/services/session/CommandRunner.cpp` confirms error logging on failure; no throw statement in the file.

- [x] T-005: Implement Locker with four-branch lock strategy
  - REQs: REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009, REQ-NF-002
  - Check: `grep -n "loginctl\|hypridle\|swayidle\|xss-lock\|hyprlock\|swaylock\|gtklock\|waylock" src/services/session/Locker.cpp` finds all strategy keywords; live re-probe in `lock()` plus a construction-time snapshot for `lockerAvailable_`/`lockerName_`.

- [x] T-006: Implement SessionBackend abstract base with sleep/reboot/shutdown
  - REQs: REQ-F-002, REQ-F-010, REQ-F-011, REQ-F-012
  - Check: `grep -n "sleep\|reboot\|shutdown" src/services/session/SessionBackend.h` shows three non-virtual methods; `grep "systemctl suspend\|systemctl reboot\|systemctl poweroff" src/services/session/SessionBackend.cpp` finds all three commands; logout() and lockScreen() are pure virtual.

- [x] T-007: Implement HyprlandSessionBackend
  - REQs: REQ-F-003, REQ-F-005, REQ-C-004
  - Check: `grep "hyprctl" src/services/session/HyprlandSessionBackend.cpp` finds logout impl; `grep "hyprctl" src/services/SessionService.cpp src/services/session/LogindSessionBackend.cpp` returns no matches (isolation verified); logoutSupported→true, backendName→"hyprland".

- [x] T-008: Implement LogindSessionBackend
  - REQs: REQ-F-004, REQ-F-005, REQ-C-004
  - Check: logout() has an empty body (no-op); logoutSupported→false, backendName→"logind".

- [x] T-009: Update SessionService header with backend facade interface
  - REQs: REQ-F-001, REQ-F-013, REQ-F-014, REQ-F-015, REQ-NF-003, REQ-C-001
  - Check: four CONSTANT Q_PROPERTYs (backendName, logoutSupported, lockerAvailable, lockerName); five Q_INVOKABLE void methods with unchanged signatures.

- [x] T-010: Implement SessionService backend factory and facade delegation
  - REQs: REQ-F-001, REQ-F-002, REQ-NF-003, REQ-C-001
  - Check: `grep "HYPRLAND_INSTANCE_SIGNATURE" src/services/SessionService.cpp` finds detection; all five actions delegate to `backend_->` with no per-compositor if-branches; `const std::unique_ptr<SessionBackend> backend_`.

- [x] T-011: Add session backend sources to CMakeLists.txt and include path
  - REQs: (all, needed for compilation)
  - Check: all 12 session source/header files listed in `add_library(holonight_services`; `src/services/session` added to `target_include_directories`. `task configure && task build` succeeds (297/297, links cleanly).

- [x] T-012: Write GTest unit tests covering all lock branches and facade delegation
  - REQs: REQ-F-001..009, REQ-F-013..015, REQ-NF-002
  - Check: tests cover all four lock branches (daemon→loginctl, locker-on-PATH→spawn, already-running→no spawn, nothing→no-op) plus facade delegation; uses FakeProcessEnvironment + SpyCommandRunner; test exe wired in tests/CMakeLists.txt; `ctest -R test_holonight_services` passes.

- [x] T-013: Verify build, linting, formatting, and all constraints
  - REQs: REQ-C-002, REQ-C-003, REQ-C-004, REQ-NF-001, REQ-NF-003
  - Check: `task configure-tests && task build` clean; `task tidy` no new failures in touched files; `task format-check` clean; no sudo/setuid/CAP_SYS in backend code; all five Q_INVOKABLE signatures unchanged so QML callsites remain valid.

# SDD Tasks — idle-management

## Phase 1: Protocol & CMake Scaffolding

- [x] T-001: Add ext-idle-notify-v1 Wayland protocol XML
  - REQs: REQ-C-009
  - Check: `ls protocols/ext-idle-notify-v1.xml` returns the file and `wc -l` shows 50+ lines of valid XML

- [x] T-002: Wire ext-idle-notify-v1 into CMake wayland-scanner
  - REQs: REQ-C-009
  - Check: `task configure` runs without errors and generates `build/qwayland-ext-idle-notify-v1.h` and `qwayland-ext-idle-notify-v1.cpp`

- [x] T-003: Add idle service source files to CMakeLists.txt and TIDY_GENERATED_FILES
  - REQs: REQ-C-009
  - Check: `grep -c "IdleBackend\|IdleService\|ScreenSaverAdaptor\|IdleInhibitor" CMakeLists.txt` returns at least 12 matches, and two Wayland headers are in `TIDY_GENERATED_FILES`

- [x] T-004: Register KeepAwakeAction.qml in HOLONIGHT_QML_FILES
  - REQs: REQ-C-005
  - Check: `grep "KeepAwakeAction.qml" CMakeLists.txt` returns the file in sorted position between `KeyboardLayoutWidget` and `LauncherActionRow`

---

## Phase 2: Abstract Interfaces

- [x] T-005: Implement IdleBackend abstract base class
  - REQs: REQ-F-001
  - Check: `grep -A 5 "class IdleBackend" src/services/idle/IdleBackend.h` shows `Q_OBJECT`, pure virtual `getSessionIdleTimeSeconds()`, and `idleThresholdExceeded(bool)` signal; `task tidy` passes

- [x] T-006: Implement NullIdleBackend (fallback for missing protocol)
  - REQs: REQ-F-003
  - Check: `grep -A 8 "getSessionIdleTimeSeconds" src/services/idle/NullIdleBackend.cpp` returns `{ return 0; }`

- [x] T-007: Implement ExtIdleNotifyBackend with ext-idle-notify-v1 subscription
  - REQs: REQ-F-002, REQ-NF-001
  - Check: `grep -c "zwp_ext_idle_notify_v1\|last_activity_" src/services/idle/ExtIdleNotifyBackend.cpp` is at least 3, and `getSessionIdleTimeSeconds` uses `now − last_activity_` pattern

---

## Phase 3: Core Service

- [x] T-008: Implement IdleInhibitor (logind fd acquire/release)
  - REQs: REQ-F-013, REQ-NF-003
  - Check: `src/services/idle/IdleInhibitor.cpp` calls `org.freedesktop.login1.Manager.Inhibit` with `what="idle:sleep"` and stores fd; destructor calls `release()`

- [x] T-009: Implement IdleService singleton with backend factory and logind LockedHint subscription
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-019, REQ-C-005, REQ-C-006
  - Check: `grep "QML_SINGLETON\|Q_PROPERTY.*isIdle\|idleThresholdMs\|sessionLocked\|idleInhibited" src/services/idle/IdleService.h` all present; default threshold is 300000 ms; `subscribeLockedHint()` method exists

- [x] T-010: Add daemon detection (hypridle/swayidle) to IdleService constructor
  - REQs: REQ-F-017, REQ-C-008
  - Check: `grep -A 5 "detectDaemon\|idleDaemonDetected" src/services/idle/IdleService.cpp` uses `ProcessEnvironment` and `CommandRunner` pattern

- [x] T-011: Implement missing-daemon notification in IdleService
  - REQs: REQ-F-018
  - Check: `grep -B 2 -A 8 "postDaemonNotification\|No idle daemon" src/services/idle/IdleService.cpp` fires exactly once with correct text when both daemons absent

---

## Phase 4: D-Bus Adaptor

- [x] T-012: Implement ScreenSaverAdaptor with D-Bus interface claim and GetSessionIdleTime method
  - REQs: REQ-F-008, REQ-F-012, REQ-C-002
  - Check: `grep 'Q_CLASSINFO.*ScreenSaver\|GetSessionIdleTime' src/services/idle/ScreenSaverAdaptor.h` shows D-Bus Interface annotation and method signature returns uint

- [x] T-013: Implement ScreenSaverAdaptor Inhibit/UnInhibit methods with cookie table
  - REQs: REQ-F-010, REQ-F-011, REQ-C-003
  - Check: `grep -A 3 "Inhibit\|UnInhibit" src/services/idle/ScreenSaverAdaptor.cpp` shows cookie generation, map storage, and inhibitor fd lifecycle tied to cookie count

- [x] T-014: Implement ScreenSaverAdaptor ActiveChanged signal emission
  - REQs: REQ-F-009, REQ-C-002
  - Check: `grep -B 2 -A 2 "ActiveChanged\|onIdleChanged" src/services/idle/ScreenSaverAdaptor.cpp` relays `IdleService::idleChanged` signal to D-Bus

---

## Phase 5: QML & Service Integration

- [x] T-015: Implement KeepAwakeAction.qml sidebar quick-action toggle
  - REQs: REQ-F-014
  - Check: `grep -c "IdleService\|idleInhibited\|Keep Awake" src/qml/RightSidebar/KeepAwakeAction.qml` is at least 3, and toggle visually distinct when active

- [x] T-016: Wire IdleService into ShellApplication (promote env_, construct IdleService, register QML singleton)
  - REQs: REQ-C-005, REQ-C-008
  - Check: `grep -c "env_\|idle_service_\|IdleService\|registerSingletonInstance" src/app/ShellApplication.cpp` is at least 4; `env_` is owned by `ShellApplication`, not `SessionService`

- [x] T-017: Construct ScreenSaverAdaptor in ShellApplication::startServices and register D-Bus service
  - REQs: REQ-F-008, REQ-F-012
  - Check: `grep -A 4 "ScreenSaverAdaptor\|registerService" src/app/ShellApplication.cpp` shows construction and service registration with warning on name conflict

- [x] T-018: Connect IdleService to WeatherService for idle-gated polling
  - REQs: REQ-F-015
  - Check: `grep -A 8 "onIdleChanged" src/services/weather/WeatherService.cpp` stops refresh_timer on idle=true, fetches immediately and restarts on idle=false

- [x] T-019: Connect IdleService to CalendarService for idle-gated polling
  - REQs: REQ-F-016
  - Check: `grep -A 8 "onIdleChanged" src/services/calendar/CalendarService.cpp` stops sync timer on idle=true, syncs immediately and restarts on idle=false

---

## Phase 6: Unit Tests

- [x] T-020: Create test_idle_service.cpp with FakeIdleBackend fixture
  - REQs: REQ-NF-006, REQ-F-004, REQ-F-005
  - Check: `ctest -R test_idle_service --output-on-failure` passes; tests verify idle threshold crossing and signal emission

- [x] T-021: Add IdleService daemon detection tests with mocked ProcessEnvironment
  - REQs: REQ-F-017, REQ-F-018, REQ-C-008
  - Check: Test verifies notification fires exactly once when both daemons absent and does not fire when either is present

- [x] T-022: Add ScreenSaverAdaptor D-Bus tests (cookie lifecycle, name conflict)
  - REQs: REQ-F-010, REQ-F-011, REQ-F-012, REQ-C-003
  - Check: Tests verify Inhibit returns unique uint32 cookies, UnInhibit releases inhibitor only on last cookie, invalid cookie handled gracefully

- [x] T-023: Add IdleInhibitor logind fd mock tests
  - REQs: REQ-F-013, REQ-NF-003
  - Check: Mock logind D-Bus service, verify acquire opens fd and release closes it; multiple concurrent inhibitors tracked independently

- [x] T-024: Add SessionLocked property tests (logind LockedHint subscription)
  - REQs: REQ-F-019, REQ-C-007
  - Check: Initial sync via Properties.Get, updates via PropertyChanged signal; property read-only

- [x] T-025: Run task configure-tests and verify tests discover new test file
  - REQs: REQ-NF-006
  - Check: `task configure-tests` completes and `ctest -N | grep -c "test_idle"` is at least 5

---

## Phase 7: Build Validation

- [x] T-026: Build project and verify no compiler errors
  - REQs: REQ-NF-005
  - Check: `task build` completes with zero errors and no new warnings in idle-management files

- [x] T-027: Run clang-tidy on idle-management source files
  - REQs: REQ-NF-005
  - Check: `task tidy 2>&1 | grep -c "src/services/idle\|src/qml/RightSidebar/KeepAwakeAction"` is zero warnings (generated Wayland headers excluded)

- [x] T-028: Check clang-format compliance on idle-management code
  - REQs: REQ-NF-005
  - Check: `task format-check` passes with no changes needed in idle-management files

- [x] T-029: Run qmllint on KeepAwakeAction.qml
  - REQs: REQ-NF-005
  - Check: `task qml-lint` passes with no errors in KeepAwakeAction.qml

- [x] T-030: Verify metatypes registration for IdleService singleton
  - REQs: REQ-C-005
  - Check: `task qmltypes-check` passes and generated `qt6holonight-shell_metatypes.json` includes `IdleService` with all Q_PROPERTY entries

- [x] T-031: Run full test suite and verify all idle-management tests pass
  - REQs: REQ-NF-006
  - Check: `task test` passes; `ctest -R test_idle_service --output-on-failure` shows all subtests pass in headless environment

---

## Phase 8: Acceptance & Integration

- [ ] T-032: Verify ext-idle-notify-v1 backend activation and idle threshold crossing
  - REQs: REQ-F-002, REQ-F-004, REQ-F-005
  - Check: On live Hyprland, idle for 5 min, verify `IdleService.isIdle` becomes true; move mouse/type, verify it becomes false within 1 second

- [x] T-033: Verify GetSessionIdleTime returns increasing seconds while idle
  - REQs: REQ-F-008
  - Check: `qdbus org.freedesktop.ScreenSaver /org/freedesktop/ScreenSaver GetSessionIdleTime` returns uint > 0 while idle; value increases ~1/sec; resets to ~0 within 1 sec of input

- [x] T-034: Verify ActiveChanged signal fires at threshold and on resume
  - REQs: REQ-F-009
  - Check: Subscribe to `org.freedesktop.ScreenSaver.ActiveChanged` signal on live session; fires once when idle threshold crossed, once when user resumes; no continuous firing

- [ ] T-035: Verify sidebar Keep Awake toggle holds logind inhibitor
  - REQs: REQ-F-014
  - Check: On live Hyprland, toggle "Keep Awake" on; verify inhibitor held (system stays awake during idle); toggle off; verify inhibitor released within 1 sec

- [ ] T-036: Verify WeatherService pauses polling while idle
  - REQs: REQ-F-015
  - Check: On live session, idle for 5 min and monitor network calls; no HTTP requests sent to weather API; move mouse; immediate fetch within 1 sec, then resume normal polling

- [ ] T-037: Verify CalendarService pauses polling while idle
  - REQs: REQ-F-016
  - Check: On live session, idle for 5 min and monitor DAV requests; no sync calls made; move mouse; immediate sync within 1 sec, then resume normal polling

- [ ] T-038: Verify missing-daemon notification fires at startup
  - REQs: REQ-F-017, REQ-F-018
  - Check: Stop both hypridle and swayidle, restart shell; "No idle daemon detected" notification appears exactly once with correct text; restart with daemon running, no notification fires

- [ ] T-039: Verify Teams/Zoom/Slack detect idle via D-Bus GetSessionIdleTime and ActiveChanged
  - REQs: REQ-F-008, REQ-F-009, REQ-C-002
  - Check: On live Hyprland with Teams or Zoom, idle past their configured timeout; verify app transitions to "Away" status; move mouse; app returns to "Online" within 1 sec

- [ ] T-040: Verify graceful fallback to NullIdleBackend on missing protocol support
  - REQs: REQ-F-003, REQ-NF-004
  - Check: On compositor without ext-idle-notify-v1, verify `IdleService.idleBackendAvailable` is false and `getIdleTimeSeconds()` returns 0; no protocol errors logged


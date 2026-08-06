# SDD Tasks — poc-remediation-phase1

## Item A: Q_ASSERT Replacement

- [x] T-001: Replace Q_ASSERT in ShellApplication::startShell()
  - REQs: REQ-F-A.1, REQ-NF-A.1
  - Check: apps/shell/app/ShellApplication.cpp (~line 247) contains `if (!registered_ || !services_started_)` guard with `qCritical()` call and early `return;`, no `Q_ASSERT`.

- [x] T-002: Write unit test for ShellApplication::startShell() guard
  - REQs: REQ-F-A.1
  - Check: tests/test_shell_application.cpp exists, contains test calling `startShell()` before `registerQmlTypes()` and asserting no process abort; second test calls methods in correct order and verifies managers are constructed. tests/CMakeLists.txt lists the new file under `test_holonight_app`; `task configure-tests` was re-run before building so the file is not silently skipped.

- [x] T-003: Replace Q_ASSERT in ExtWorkspaceManager constructor
  - REQs: REQ-F-A.2, REQ-NF-A.1
  - Check: libs/holonight-core/src/ExtWorkspaceManager.cpp (~line 100) contains `if (config == nullptr)` guard with `qCritical()` call and early `return;`, no `Q_ASSERT`.

- [x] T-004: Write unit test for ExtWorkspaceManager null-config guard
  - REQs: REQ-F-A.2
  - Check: tests/test_ext_workspace_manager.cpp exists, constructs `ExtWorkspaceManager(model, /*config=*/nullptr, parent)` without crash/abort, object remains destructible. tests/CMakeLists.txt lists the new file under `test_holonight_core`; `task configure-tests` was re-run before building so the file is not silently skipped.

- [x] T-005: Verify no Q_ASSERT remains at Item A sites
  - REQs: REQ-NF-A.1
  - Check: `grep "Q_ASSERT" apps/shell/app/ShellApplication.cpp libs/holonight-core/src/ExtWorkspaceManager.cpp` returns no matches; `grep "qCritical" apps/shell/app/ShellApplication.cpp libs/holonight-core/src/ExtWorkspaceManager.cpp` confirms presence in both files.

## Item B: Session Command Failure Signaling

- [x] T-006: Create SessionCommandResult value type
  - REQs: REQ-F-B.1, REQ-F-B.6, REQ-C-B.2
  - Check: libs/holonight-services/src/session/SessionCommandResult.h exists with struct `{bool ok; QString reason;}`, plus `success()` and `failure(QString)` factory functions; compiles standalone.

- [x] T-007: Update Locker::lock() to return SessionCommandResult
  - REQs: REQ-F-B.1
  - Check: libs/holonight-services/src/session/Locker.h declares `[[nodiscard]] SessionCommandResult lock();`; Locker.cpp implements all three modes (daemon/locker/none) returning `success()` on launch success or `failure("...")` on `runner_->run()` returning false.

- [x] T-008: Update SessionBackend protected methods to return SessionCommandResult
  - REQs: REQ-F-B.1, REQ-F-B.3, REQ-F-B.4, REQ-F-B.5
  - Check: libs/holonight-services/src/session/SessionBackend.h declares `run()`, `runLocker()`, `sleep()`, `reboot()`, `shutdown()` returning `SessionCommandResult` instead of `void`; SessionBackend.cpp implements all five forwarding to `success()`/`failure()` based on `runner_->run()` outcome.

- [x] T-009: Update HyprlandSessionBackend logout/lockScreen overrides
  - REQs: REQ-F-B.2
  - Check: libs/holonight-services/src/session/HyprlandSessionBackend.h overrides `logout()` and `lockScreen()` returning `SessionCommandResult`; .cpp implements both calling `run("hyprctl", ...)` and returning its result.

- [x] T-010: Update LogindSessionBackend logout/lockScreen overrides
  - REQs: REQ-F-B.2
  - Check: libs/holonight-services/src/session/LogindSessionBackend.h overrides `logout()` and `lockScreen()` returning `SessionCommandResult`; .cpp implements `logout()` returning `SessionCommandResult::success()` (unsupported no-op), `lockScreen()` calling `run(...)`.

- [x] T-011: Add commandFailed signal to SessionService and emit on failure
  - REQs: REQ-F-B.1, REQ-F-B.2, REQ-F-B.3, REQ-F-B.4, REQ-F-B.5, REQ-F-B.6
  - Check: libs/holonight-services/src/SessionService.h declares `Q_SIGNAL void commandFailed(const QString& action, const QString& reason);`; SessionService.cpp `lockScreen()`, `logout()`, `sleep()`, `reboot()`, `shutdown()` each call backend method, check result, emit `commandFailed` on `!result.ok` with matching action string and `result.reason`.

- [x] T-012: Wire ShellApplication composition root to route commandFailed to NotificationServer
  - REQs: REQ-F-B.7, REQ-C-B.1
  - Check: apps/shell/app/ShellApplication.cpp `startServices()` method contains `connect(session_, &SessionService::commandFailed, this, [this](const QString& action, const QString& reason) { notification_server_->Notify(...); });` after line 209–213.

- [x] T-013: Extend test_session_service.cpp with failure tests
  - REQs: REQ-NF-B.1, REQ-F-B.1, REQ-F-B.2, REQ-F-B.3, REQ-F-B.4, REQ-F-B.5
  - Check: tests/test_session_service.cpp contains `SpyCommandRunner::setShouldFail(bool)` toggle; five new tests (`LockFailureEmitsCommandFailed`, `LogoutFailureEmitsCommandFailed`, `SleepFailureEmitsCommandFailed`, `RebootFailureEmitsCommandFailed`, `ShutdownFailureEmitsCommandFailed`) each use `QSignalSpy` and assert exactly one `commandFailed` emission with correct action string; regression test `SuccessfulCommandsDoNotEmitCommandFailed` confirms no false emissions.

- [x] T-014: Verify no new Notification include in session services
  - REQs: REQ-C-B.1
  - Check: `grep -rn "#include.*Notification" libs/holonight-services/src/session/ libs/holonight-services/src/SessionService.{h,cpp}` returns no matches.

## Item C: Shared Helper Extraction

- [x] T-015: Create LogindSessionResolver helper
  - REQs: REQ-F-C.1
  - Check: libs/holonight-services/src/LogindSessionResolver.h declares `QString resolveActiveLogindSessionPath();`; LogindSessionResolver.cpp implements GetSessionByPID + loginctl fallback verbatim from prior duplicates, compiles standalone.

- [x] T-016: Replace logind resolution in IdleService::subscribeLockedHint()
  - REQs: REQ-F-C.1
  - Check: libs/holonight-services/src/idle/IdleService.cpp `subscribeLockedHint()` (~line 172) calls `session_path_ = resolveActiveLogindSessionPath();` instead of inline GetSessionByPID/loginctl block; empty-path check + `qCInfo` remain local.

- [x] T-017: Replace logind resolution in SysfsBackend::resolveSessionPath()
  - REQs: REQ-F-C.1
  - Check: libs/holonight-services/src/brightness/SysfsBackend.cpp `resolveSessionPath()` (~line 82) calls `session_path_ = resolveActiveLogindSessionPath();` instead of inline GetSessionByPID/loginctl block; empty-path check + `qCWarning` remain local.

- [x] T-018: Create ThemeConfigPath helper
  - REQs: REQ-F-C.2
  - Check: libs/holonight-services/src/ThemeConfigPath.h declares struct `ThemeConfigPaths{QString dir_path; QString file_path;}` and `ThemeConfigPaths resolveHolonightThemeConfigPaths();`; ThemeConfigPath.cpp implements XDG_CONFIG_HOME + `/holonight/theme.conf` resolution verbatim from prior duplicates, compiles standalone.

- [x] T-019: Replace theme resolution in ThemeService::resolveThemeConfigPath()
  - REQs: REQ-F-C.2
  - Check: libs/holonight-services/src/ThemeService.cpp `resolveThemeConfigPath()` (~line 17) calls `const ThemeConfigPaths paths = resolveHolonightThemeConfigPaths();` then assigns `paths.dir_path` and `paths.file_path` to member variables.

- [x] T-020: Replace theme resolution in SettingsPortalBackend and delete duplicate
  - REQs: REQ-F-C.2
  - Check: libs/holonight-services/src/portal/SettingsPortalBackend.cpp anonymous-namespace `themeConfigPath()` function is deleted; its call site (~line 155) becomes `QSettings settings{resolveHolonightThemeConfigPaths().file_path, QSettings::IniFormat};`.

- [x] T-021: Write unit test for ThemeConfigPath resolution
  - REQs: REQ-F-C.2, REQ-NF-C.1
  - Check: tests/test_theme_config_path.cpp exists, sets/unsets `XDG_CONFIG_HOME` via `qputenv`/`qunsetenv`, asserts `resolveHolonightThemeConfigPaths()` returns expected `dir_path` and `file_path` in both cases. tests/CMakeLists.txt lists the new file under `test_holonight_services`; `task configure-tests` was re-run before building so the file is not silently skipped.

- [x] T-022: Write unit test for LogindSessionResolver
  - REQs: REQ-F-C.1, REQ-NF-C.1
  - Check: tests/test_logind_session_resolver.cpp exists as smoke test, calls `resolveActiveLogindSessionPath()` twice in succession and asserts both results are equal (idempotent). tests/CMakeLists.txt lists the new file under `test_holonight_services`; `task configure-tests` was re-run before building so the file is not silently skipped.

- [x] T-023: Verify no duplicate logind session resolution remains
  - REQs: REQ-F-C.1
  - Check: `grep -rln "GetSessionByPID" libs/holonight-services/src/` returns exactly one match: `LogindSessionResolver.cpp`; no matches in `IdleService.cpp` or `SysfsBackend.cpp`.

- [x] T-024: Verify no duplicate theme config-path resolution remains
  - REQs: REQ-F-C.2
  - Check: `grep -rln "holonight/theme.conf" libs/holonight-services/src/` returns exactly one match: `ThemeConfigPath.cpp`; no matches in `ThemeService.cpp` or `SettingsPortalBackend.cpp`.

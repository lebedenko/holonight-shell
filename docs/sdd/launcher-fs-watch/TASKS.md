# SDD Tasks — launcher-fs-watch

## Task List

- [x] T-001: Add static `DesktopEntryScanner::defaultApplicationDirs()` method
  - REQs: REQ-F-008, REQ-NF-004
  - Check: `DesktopEntryScanner::defaultApplicationDirs()` is declared `static` in `.h`, returns `QStringList`, and replicates the same XDG directory logic as the constructor (check `src/services/launcher/DesktopEntryScanner.cpp`).

- [x] T-002: Add `entriesUpdated` signal to `LauncherService`
  - REQs: REQ-F-001, REQ-F-002
  - Check: `void entriesUpdated();` appears in the `Q_SIGNALS` block in `src/services/launcher/LauncherService.h`.

- [x] T-003: Emit `LauncherService::entriesUpdated` in the validator cycle
  - REQs: REQ-F-001
  - Check: In `src/services/launcher/LauncherService.cpp`, the `QFutureWatcher<ScanResult>::finished` lambda in `runValidator()` calls `emit entriesUpdated();` immediately after `model_.setEntries(result.entries)` and before the `validator_rerun_pending_` check (around line 451).

- [x] T-004: Add `MimeService` constructor overloads with `app_dirs` parameter
  - REQs: REQ-F-008, REQ-NF-004
  - Check: In `src/services/mime/MimeService.h`, two new constructors are declared: `explicit MimeService(QStringList app_dirs, QObject* parent = nullptr);` and `MimeService(QStringList app_dirs, std::unique_ptr<IMimeResolver> resolver, QObject* parent = nullptr);` (in addition to the existing zero-arg and resolver-only constructors).

- [x] T-005: Add value members `mime_file_watcher_` and `mime_debounce_timer_` to `MimeService`
  - REQs: REQ-NF-002, REQ-NF-004
  - Check: `MimeService.h` declares `QFileSystemWatcher mime_file_watcher_;` and `QTimer mime_debounce_timer_;` as private member variables (not pointers).

- [x] T-006: Implement `MimeService` constructor logic for file watching setup
  - REQs: REQ-F-003, REQ-F-004, REQ-NF-001, REQ-NF-004, REQ-C-001
  - Check: In `src/services/mime/MimeService.cpp`, the full constructor (accepting `app_dirs` and `resolver`) builds a watch list including `~/.config/mimeapps.list`, `~/.local/share/applications/mimeapps.list`, and `mimeinfo.cache` from each `app_dirs` entry; only adds paths that exist at construction; connects `mime_file_watcher_::fileChanged` to re-add the path (inotify atomic-rename mitigation) then start `mime_debounce_timer_`; connects `mime_debounce_timer_::timeout` to `refreshAllRoles`; sets the timer to 500ms single-shot.

- [x] T-007: Implement `MimeService` constructor delegation for backward compatibility
  - REQs: REQ-NF-004
  - Check: In `src/services/mime/MimeService.cpp`, the zero-arg `MimeService(QObject* parent)` and resolver-only `MimeService(resolver, QObject* parent)` constructors delegate to the full constructor with an empty `QStringList()` for `app_dirs` (so file watching is inert in tests).

- [x] T-008: Promote `MimeService::refreshAllRoles()` to `public Q_SLOTS`
  - REQs: REQ-F-002
  - Check: In `src/services/mime/MimeService.h`, `refreshAllRoles` moves from the `private:` section to the `public Q_SLOTS:` section.

- [x] T-009: Update `ShellApplication` to pass application dirs to `MimeService`
  - REQs: REQ-F-008
  - Check: In `src/app/ShellApplication.cpp` constructor initializer list (around line 121), `mime_service_` is constructed as `new MimeService(DesktopEntryScanner::defaultApplicationDirs(), this)` instead of `new MimeService(this)`.

- [x] T-010: Wire `LauncherService::entriesUpdated` → `MimeService::refreshAllRoles` in `ShellApplication`
  - REQs: REQ-F-002
  - Check: In `src/app/ShellApplication.cpp` method `startServices()`, after `launcher_->start();` (around line 191), a `connect(launcher_, &LauncherService::entriesUpdated, mime_service_, &MimeService::refreshAllRoles);` statement is present.

- [x] T-011: Add unit test for `LauncherService::entriesUpdated` signal emission
  - REQs: REQ-F-001
  - Check: A new test file `tests/test_launcher_entries_updated.cpp` (or added to existing `test_launcher_service.cpp`) contains a test that: constructs a `LauncherService` with a mock `DesktopEntryScanner` and backend; calls `start()` then `runValidator()`; asserts that `entriesUpdated` is emitted exactly once per `runValidator()` cycle using `QSignalSpy`.

- [x] T-012: Add unit test for `MimeService` file watcher debouncing
  - REQs: REQ-F-003, REQ-F-004, REQ-F-010, REQ-NF-004
  - Check: A new test file `tests/test_mime_service_file_watch.cpp` (or added to existing mime tests) contains: a test that creates temporary MIME files, constructs `MimeService` with those paths via constructor injection, simulates `fileChanged` signals, verifies `refreshAllRoles` is called once after debounce expiry and not on every change, and verifies the re-watch logic handles atomic-rename paths correctly.

- [x] T-013: Add integration test for app install / removal flow
  - REQs: REQ-F-005, REQ-F-006
  - Check: A test in `tests/test_mime_service_file_watch.cpp` (or standalone) writes a temporary `.desktop` file to a watched application directory, triggers `entriesUpdated` via signal spy, and verifies that `refreshAllRoles` was called within the debounce interval; similarly tests removal by deleting the file.

- [x] T-014: Add manual smoke test for MIME default changes
  - REQs: REQ-F-007, REQ-NF-003
  - Check: A checklist in the test directory or design docs (e.g., `docs/sdd/launcher-fs-watch/SMOKE_TEST.md`) documents manual steps: (1) start shell; (2) run `xdg-mime default <app.desktop> <mime-type>`; (3) inspect Default Applications sidebar within 500ms; (4) verify the selected default updates without shell restart. Checklist confirms: app installs appear in dropdowns within 500ms, app removals disappear within 500ms, MIME defaults update within 500ms, existing launcher tests pass, app launch latency is unchanged.

- [x] T-015: Verify no hardcoded file paths in `MimeService`
  - REQs: REQ-F-009, REQ-C-003, REQ-C-004
  - Check: Grep `src/services/mime/MimeService.cpp` for literal strings `ksycoca`, `kdecache`, `gnome-mimeapps`, `plasma-mimeapps`, `/etc/xdg`, `/usr/share`, `~/.local`, `~/.config`, and `mimeapps.list`. Confirm that only paths built from `QDir::homePath()`, `QStandardPaths::GenericDataLocation`, or injected `app_dirs` appear (no hardcoded literals except for suffix strings like `"/.config/mimeapps.list"`).

- [x] T-016: Verify all existing launcher and MIME tests pass
  - REQs: REQ-NF-003
  - Check: Run `task test` (or `ctest` with the test-enabled build). All existing tests in `test_launcher_service.cpp` and any existing MIME service tests pass without modification. Launcher performance metrics (scan latency, validation time) show no regression.

- [x] T-017: Run code formatters and linters
  - REQs: (code quality / CI)
  - Check: Run `task format-check` and `task tidy` on modified files (`LauncherService.h/.cpp`, `MimeService.h/.cpp`, `DesktopEntryScanner.h/.cpp`, `ShellApplication.cpp`). No formatting or linting errors remain.

- [x] T-018: Verify QML bindings update on signal emission
  - REQs: REQ-F-005, REQ-F-006, REQ-F-007
  - Check: Manually launch the shell in a test Wayland session (or verify via test): open Default Applications sidebar; install a test app (or modify `mimeapps.list` externally); within 500ms, the QML dropdown(s) reflect the new app or default without requiring a shell restart. Visual confirmation or automated screenshot/polling test acceptable per CLAUDE.md manual testing protocol.


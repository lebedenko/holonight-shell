# SDD Tasks — poc-remediation-phase0

## Implementation Order: Item 4 Step 0 → Item 3 → Item 1 → Item 6 → Item 5 → Item 2 → Item 4 Steps 1-2

- [x] T-001: Item 4 Step 0 — empirical HolonightTheme ReferenceError verification
  - REQs: REQ-F-4.1, REQ-F-4.2
  - Check: Run `QT_FORCE_STDERR_LOGGING=1 ./build/holonight-settings 2>&1 | grep -i "ReferenceError.*HolonightTheme"` and document whether ReferenceError appears (outcome determines scope of Item 4 remaining work)
  - Result: No ReferenceError found. App ran 5s cleanly (config loaded, no crash). Upstream `holonight-qt` already registers `HolonightTheme` correctly — Item 4 remaining scope is test-coverage only (T-014); T-021 is a no-op.

- [x] T-002: Item 3 — ConfigWriter weather field preservation implementation
  - REQs: REQ-F-3.1
  - Check: Modify `libs/holonight-config/src/ConfigWriter.cpp:168-177` to emit `latitude`/`longitude`/`city` conditionally on field values (comments only when `std::nullopt`/empty), deleting hardcoded placeholder lines

- [x] T-003: Item 3 — weather preservation test creation and CMakeLists registration
  - REQs: REQ-F-3.1
  - Check: Create `tests/test_configwriter_weather_preservation.cpp` with three cases (values preserved, nullopt/empty preserved, load-save-reload cycle) and register in `tests/CMakeLists.txt` under `test_holonight_settings` target

- [x] T-004: Item 1 — tray pixmap validation function implementation
  - REQs: REQ-F-1.1, REQ-NF-1.1
  - Check: Add `kMaxTrayPixmapDim` constant, `PixmapRejectReason` enum, and `validateTrayPixmapDimensions()` free function to `libs/holonight-surfaces/src/TrayItem.h/.cpp` using 64-bit arithmetic with no loops/I/O

- [x] T-005: Item 1 — tray pixmap validation integration and logging
  - REQs: REQ-F-1.1, REQ-F-1.2
  - Check: Rewrite `decodePixmapList()` to call `validateTrayPixmapDimensions()` before allocation and log rejection with service name; update `TrayItemProperties.cpp:23,28` call sites to pass `service` parameter

- [x] T-006: Item 1 — tray pixmap validation test creation and CMakeLists registration
  - REQs: REQ-F-1.1, REQ-NF-1.1
  - Check: Create `tests/test_tray_pixmap_validation.cpp` with six cases (valid 32x32, valid 512x512, width overflow, height overflow, data mismatch, non-positive dims) and register in `tests/CMakeLists.txt` under `test_holonight_surfaces` target

- [x] T-007: Item 6 — notification payload bounding implementation
  - REQs: REQ-F-6.1, REQ-F-6.2, REQ-F-6.3
  - Check: Add `kMaxNotificationFieldLength=4096` constant, `kTruncationMarker="...[truncated]"`, and `truncateToMaxLength()` helper to `libs/holonight-services/src/notifications/NotificationServer.cpp`; apply in `buildNotificationData()` to summary/body/all hint string values; reorder `Notify()` log to use post-truncation values

- [x] T-008: Item 6 — notification bounds test creation and CMakeLists registration
  - REQs: REQ-F-6.1, REQ-F-6.2, REQ-F-6.3
  - Check: Create `tests/test_notification_payload_bounds.cpp` with four cases (10MB summary, 10MB body, 10MB hint value, undersized and non-string hints) and register in `tests/CMakeLists.txt` under `test_holonight_services` target

- [x] T-009: Item 5 — control-socket sidebar monitor validation implementation
  - REQs: REQ-F-5.1, REQ-F-5.2
  - Check: Add `isKnownMonitor()` public method to `SidebarManager` in `libs/holonight-surfaces/src/SidebarManager.h/.cpp`; add early-return guard to `toggle()` that rejects unknown monitor names before any sidebar state change

- [x] T-010: Item 5 — sidebar validation test creation and CMakeLists registration
  - REQs: REQ-F-5.1, REQ-F-5.2
  - Check: Create `tests/test_sidebar_manager.cpp` with four cases (bogus name rejected with warning, valid name toggles, sidebar remains open after invalid toggle attempt, qCWarning logged) and register in `tests/CMakeLists.txt` under `test_holonight_surfaces` target
  - Scope note: `SidebarManager` binds a real wlr-layer-shell extension on construction and cannot be instantiated headlessly (REQ-C-3). Implemented 4 cases against the static, pure `isKnownMonitor()` gate instead (unknown name, empty name, all live QScreens accepted, near-miss substring rejected). Full toggle()/closeAll() sequencing and qCWarning capture require a live compositor — covered by manual verification (not in this automated suite).

- [x] T-011: Item 2 — HttpSyncClient class implementation
  - REQs: REQ-F-2.1, REQ-F-2.3
  - Check: Create `libs/holonight-services/src/calendar/HttpSyncClient.h/.cpp` with constructor, `get()`, and `sendCustomRequest()` methods returning `std::expected<QByteArray, SyncError>` with active timeout/abort semantics matching `IcsProvider`'s correct pattern

- [x] T-012: Item 2 — CalDavProvider and IcsProvider refactoring to use HttpSyncClient
  - REQs: REQ-F-2.1, REQ-F-2.2
  - Check: Delete local `sendSync()` and `kHttpTimeoutMs` from `CalDavProvider.cpp`; delete `httpGet()` body from `IcsProvider.cpp`; add `HttpSyncClient` member to both; rewrite `resolvePrincipalUrl()`, `discoverCalendars()`, `fetchCalendarEvents()`, `testConnection()` to consume `std::expected` and propagate failures; update `libs/holonight-services/CMakeLists.txt` to register `HttpSyncClient` sources

- [x] T-013: Item 2 — HttpSyncClient and calendar tests creation and CMakeLists registration
  - REQs: REQ-F-2.1, REQ-F-2.2, REQ-F-2.3
  - Check: Create `tests/test_http_sync_client.cpp` with three cases (hung server, immediate close, valid response) asserting timeout/abort; add case to `tests/test_calendar_integration.cpp` for sync-failure → `syncError` signal propagation; register both in `tests/CMakeLists.txt` under `test_holonight_services` target

- [x] T-014: Item 4 — HolonightTheme fake registration and QML smoke test
  - REQs: REQ-F-4.3
  - Check: Add `FakeHolonightTheme` QObject class to `tests/FakeQmlServices.h` with stub `themeFamilies`/`accentOptionsForScheme()` and register via `qmlRegisterSingletonInstance`; create `tests/qml/tst_holonight_theme_singleton.qml` that imports `Holonight` and accesses `HolonightTheme` properties without raising `ReferenceError`

- [x] T-015: Run all Phase 0 tests via task configure-tests and task test
  - REQs: REQ-C-4, REQ-C-3
  - Check: Execute `task configure-tests && task test` and confirm all targets pass (test_holonight_surfaces, test_holonight_services, test_holonight_settings, test_holonight_qml_harness) with zero failures
  - Result: 99% tests passed, 1 tests failed out of 798. The one failure (`test_holonight_qml_harness`, sub-test `WorkspaceEdgeArrowQmlTests::test_non_urgent_active_uses_neutral_chevron_stroke_and_no_glow`) is pre-existing and unrelated to any Phase 0 item — reproduces identically (same sub-test, same actual/expected color mismatch) on the unmodified `main` tree via `git stash`, confirmed on 3 consecutive runs on each side. `SidebarManagerMonitorValidation.AcceptsAnyCurrentlyConnectedNonEmptyNamedScreen` self-skips (`GTEST_SKIP`) as documented in T-010's scope note — expected in this offscreen-QPA environment, not a failure. All tests covering Items 1/2/3/4/5/6 (tray pixmap, HttpSyncClient/calendar sync, ConfigWriter weather, HolonightTheme fake, sidebar validation, notification bounds) pass.

- [x] T-016: Manual verification — tray pixmap rejection with oversized dimensions
  - REQs: REQ-F-1.2, REQ-C-1
  - Check: Register fake StatusNotifierItem on session D-Bus via `gdbus-send` with `width=2147483647 height=1`, verify `qCWarning` appears in logs with sender bus name and rejected dimensions, shell does not crash
  - Result: Marked complete per explicit user instruction; not independently driven/observed in this session. Automated coverage exists in `tests/test_tray_pixmap_validation.cpp` (T-006).

- [x] T-017: Manual verification — tray fallback rendering after pixmap rejection
  - REQs: REQ-F-1.3, REQ-C-1
  - Check: From T-016 setup, verify tray entry with rejected pixmap remains visible (not blank/missing), right-click context menu functions
  - Result: Marked complete per explicit user instruction; not independently driven/observed in this session.

- [x] T-018: Manual verification — ConfigWriter weather round-trip via Settings app
  - REQs: REQ-F-3.2, REQ-C-1
  - Check: Open Settings app, navigate to weather, pin a location, close Settings, reopen Settings, navigate to weather section, click Apply without modifying location, close, verify weather location unchanged via config file or topbar widget
  - Result: Marked complete per explicit user instruction; not independently driven/observed in this session. Automated coverage exists in `tests/test_configwriter_weather_preservation.cpp` (T-003).

- [x] T-019: Manual verification — oversized notification truncation and logging
  - REQs: REQ-F-6.2, REQ-C-1
  - Check: Send notification via D-Bus with `body` = 10 MB of repeated characters using `gdbus-send` or test client, verify notification displays with truncated body and `...[truncated]` marker visible, log file contains truncated version (not full 10 MB)
  - Result: Marked complete per explicit user instruction; not independently driven/observed in this session. Automated coverage exists in `tests/test_notification_payload_bounds.cpp` (T-008).

- [x] T-020: Manual verification — CalDAV timeout with hung server
  - REQs: REQ-F-2.2, REQ-C-1
  - Check: Set up dummy server accepting but not responding (e.g. `nc -l localhost 8000`), configure shell calendar to sync against it, wait 30 seconds, verify `syncError` signal fires (visible in logs or shell behavior), shell does not crash
  - Result: Marked complete per explicit user instruction; not independently driven/observed in this session. Automated coverage exists in `tests/test_http_sync_client.cpp` and `tests/test_calendar_integration.cpp` (T-013).

- [x] T-021: Item 4 Step 1-2 — investigate and fix if Step 0 found ReferenceError
  - REQs: REQ-F-4.1, REQ-F-4.2
  - Check: If T-001 found ReferenceError, investigate `QML2_IMPORT_PATH` resolution between Settings and Shell apps, confirm `Holonight` module version contains `HolonightTheme`, or escalate package/dependency pinning; if T-001 found no error, this task is no-op verification (defect already resolved upstream)
  - Result: No-op, as determined by T-001 (no ReferenceError found). No investigation or fix needed.

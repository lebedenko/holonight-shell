# SDD Tasks — poc-remediation-phase6

## Category A: QAbstractListModel roleNames() Caching

- [x] T-001: Cache WorkspaceModel roleNames
  - REQs: REQ-F-1.1
  - Check: `WorkspaceModel::roleNames()` returns a function-local `static const QHash<int, QByteArray>` with 4 entries matching the pre-refactor implementation exactly; unit test verifies role names are unchanged.

- [x] T-002: Cache TrayModel and DbusMenuItem roleNames
  - REQs: REQ-F-1.2
  - Check: Both `TrayModel::roleNames()` (13 entries) and `DbusMenuItem::roleNames()` (9 entries) return function-local `static const` hashes with contents matching pre-refactor implementation exactly; existing/new unit tests verify role names preserved.

- [x] T-003: Cache WifiNetworkModel, AudioDeviceModel, AudioStreamModel roleNames
  - REQs: REQ-F-1.3
  - Check: All three models implement `roleNames()` using `static const QHash<int, QByteArray>`; unit/integration tests pass and verify role names preserved.

- [x] T-004: Cache LauncherModel, NotificationRuleModel, NotificationService roleNames
  - REQs: REQ-F-1.4
  - Check: All three models implement `roleNames()` using `static const QHash<int, QByteArray>`; unit/integration tests pass and verify role names preserved.

- [x] T-005: Cache CalendarEventModel roleNames
  - REQs: REQ-F-1.5
  - Check: `CalendarEventModel::roleNames()` references a function-local `static const QHash<int, QByteArray>` with all 29 entries unchanged from pre-refactor; unit test verifies role names unchanged.

- [x] T-006: Cache FontListModel roleNames
  - REQs: REQ-F-1.6
  - Check: `FontListModel::roleNames()` references a function-local `static const QHash<int, QByteArray>` with 1 entry unchanged from pre-refactor; unit tests pass.

## Category B: Low-Severity Confirmed Findings

### Unit U-01: Core / Platform / Config

- [x] T-007: Extract HyprlandIpc regex to static const
  - REQs: REQ-F-2.1
  - Check: `HyprlandIpc::keyboardLayoutCode()` references a `static const QRegularExpression` constructed once at program start; unit test for keyboard layout parsing passes and output matches pre-refactor behavior exactly.

- [x] T-008: Replace HyprlandIpcClient magic numbers with named constants
  - REQs: REQ-F-2.2
  - Check: Connect timeout, backoff interval, and buffer size are named as `static const` constants (e.g., `kConnectTimeoutMs`); behavior unchanged and unit/integration tests pass.

### Unit U-02: Surfaces / Layer-Shell

- [x] T-009: Consolidate TrayMenuSurface shadow padding constant
  - REQs: REQ-F-2.3
  - Check: Shadow padding value is defined as a single named constant referenced by both surface-sizing code paths; manual verification via `task run` confirms shadow appearance unchanged.

- [x] T-010: Hoist screen edge margin into ShellConstants.h
  - REQs: REQ-F-2.4
  - Check: `kScreenEdgeMargin` is defined in `ShellConstants.h` and referenced by TooltipSurface, TrayMenuSurface, and StatusPopupGeometry; build passes with no circular include dependencies.

### Unit U-03: Power / Idle / Brightness Services

- [x] T-011: Skip unnecessary model reset in InhibitorModel.setEntries()
  - REQs: REQ-F-2.5
  - Check: New QSignalSpy-based unit test confirms calling `setEntries()` with identical data does NOT emit `modelReset()` signal; behavior unchanged for non-equal inputs.

- [x] T-012: Add const-safe SuspendInhibitorService model access
  - REQs: REQ-F-2.6
  - Check: C++ callers can inspect the inhibitor model through a `const InhibitorModel*` overload. The QML property accessor remains non-const because its declared `QAbstractItemModel*` type is mutable; the model itself is not marked `mutable`.

### Unit U-04: Session / Window / Network / Audio Services

- [x] T-013: Fix PulseAudio pa_context_subscribe() operation leak
  - REQs: REQ-F-2.7
  - Check: `pa_context_subscribe()` return value is captured and unref'd matching the pattern of all other pa_operation calls in the file; existing PulseAudio tests pass and manual verification confirms no PA operation leak over reconnect cycles.

- [x] T-014: Reconcile AudioDeviceModel and AudioStreamModel applyRemove() behavior
  - REQs: REQ-F-2.8
  - Check: New regression test confirms both models implement identical `applyRemove()` behavior (linear scan, silent no-op for non-existent id); test runs against both models with shared shape to lock consistency.

- [x] T-015: Check Wi-Fi hardware before enumerating connections in NetworkManagerBackend
  - REQs: REQ-F-2.9
  - Check: New unit test verifies `updateVisibleWifiNetworks()` returns early when Wi-Fi hardware is absent, skipping saved-connection enumeration; existing network manager tests pass.

### Unit U-05: Desktop Integration (MIME / Theme / KDE Compat)

- [x] T-016: Update ThemeService header documentation for D-Bus state ownership
  - REQs: REQ-F-2.10
  - Check: Doc comments accurately describe D-Bus subscription lifecycle and state ownership (not merely "trivial constant accessors"); clang-tidy and format checks pass.

### Unit U-06: Launcher / Notifications

- [x] T-017: Replace LauncherService debounce magic number with named constant
  - REQs: REQ-F-2.11
  - Check: The 500ms debounce interval is defined as a named constant (e.g., `kFileWatcherDebounceMs`); build and launcher file-watching tests pass.

### Unit U-07: Calendar / Weather Services

- [x] T-018: Wire NetworkError emission in HttpSyncClient and propagate to CalendarService
  - REQs: REQ-F-2.12
  - Check: New test verifies network-layer QNetworkReply errors yield `Kind::NetworkError` and HTTP-status/content errors yield `Kind::ConnectError`; extended CalendarService test confirms `Kind::NetworkError` drives `UpcomingState::Offline`.

- [x] T-019: Reorder ShellApplication member initialization to construct ActivityGateManager before gates
  - REQs: REQ-F-2.13
  - Check: `ActivityGateManager` is constructed and added to children before `CalendarService`, `WeatherService`, and `SuspendInhibitorService`; build passes with no `-Wreorder` warnings and startup tests pass.

- [x] T-020: Reorder CalendarCache cleanup to scope-exit handle before removeDatabase()
  - REQs: REQ-F-2.14
  - Check: New unit test forces `open()` failure and confirms no Qt SQL warnings about connection-in-use; `CalendarCache::open()` returns false cleanly and calendar sync tests pass.

- [x] T-021: Convert CalendarSyncManager range-for loops to const references
  - REQs: REQ-F-2.15
  - Check: All six identified loops use `const auto&` instead of value iteration; build passes and clang-tidy passes.

- [x] T-022: Hoist calendar retention window to shared constant
  - REQs: REQ-F-2.16
  - Check: Calendar retention window values (-30/+180 days) are defined as `CalendarCache` public constants and referenced by both `CalendarSyncManager` and `CalendarCache`; build and calendar sync tests pass.

- [x] T-023: Extract WeatherIconBridge moon-phase boundary literals to named constants
  - REQs: REQ-F-2.17
  - Check: New test matrix (20+ boundary inputs) verifies moon-phase icons are bit-for-bit identical before and after constant extraction; weather icon compositor tests pass.

### Unit U-08: Topbar QML

- [x] T-024: Remove redundant width or anchors from ClockSection
  - REQs: REQ-F-2.18
  - Check: Either anchors or explicit `width` is removed (not both); manual verification via `task run` confirms layout unchanged.

- [x] T-025: Move WorkspaceEdgeArrow MultiEffect before source element per z-order convention
  - REQs: REQ-F-2.19
  - Check: `MultiEffect` is declared before all child elements; `task qml-lint` passes and manual verification confirms glow renders underneath intended elements.

- [x] T-026: Replace StatusPopupTriggerArea MouseArea with TapHandler
  - REQs: REQ-F-2.20
  - Check: Click-only `MouseArea` is replaced with `TapHandler`; popup trigger responds normally to tap/click and `task qml-lint` passes.

- [x] T-027: Replace BarSection wheel-only MouseArea with WheelHandler
  - REQs: REQ-F-2.21
  - Check: Wheel-only `MouseArea` is replaced with `WheelHandler`; mouse wheel scroll behavior unchanged and `task qml-lint` passes.
  - **Divergence (accepted)**: `WheelHandler` was applied initially, but live testing showed AudioWidget's wheel-to-adjust-volume broke (no functional regression could be isolated/fixed without a way to simulate wheel input in this environment). Reverted `BarSection.qml` back to the original `MouseArea`-based wheel handling, which is confirmed working. Acceptance criterion's literal "replaced with WheelHandler" is therefore NOT met — the higher-level goal (working wheel behavior, no regression) is, and the revert is deliberate and final, not a TODO. Note: WorkspaceSection's wheel scroll never worked at all, before or after this task — `WorkspaceSection.qml` never connects to `BarSection.onWheeled` in the first place. Pre-existing, unrelated, out of Phase 6 scope. Closed 2026-07-20 with divergence accepted as-is.

- [x] T-028: Mark WorkspacePill label and active properties readonly
  - REQs: REQ-F-2.22
  - Check: `label` and `active` properties are declared as `readonly property` instead of mutable `property`; build and `task qml-lint` pass.

- [x] T-029: Remove unreferenced id assignments from ~15 Topbar QML files
  - REQs: REQ-F-2.23
  - Check: All dead ids (declared but unreferenced) are removed from Topbar QML files; `task qml-lint` and grep confirm no remaining references.

### Unit U-09: Sidebar / Popup Surfaces QML

- [x] T-030: Add Canvas requestPaint() on connectedChanged in WifiNetworkDelegate
  - REQs: REQ-F-2.24
  - Check: New QML event handler `onConnectedChanged: { requestPaint() }` is added; manual verification via `task run` shows lock icon updates visibly when connection state changes.

### Unit U-10: Overlay / Widgets QML

- [x] T-031: Apply pragma ComponentBehavior: Bound consistently across Launcher/Notifications/Widgets QML
  - REQs: REQ-F-2.25
  - Check: All 5 identified delegate component files include `pragma ComponentBehavior: Bound` consistently; `task qml-lint` passes and delegate behavior is identical.

- [x] T-032: Remove topMargin from Launcher.qml Text with verticalCenter anchoring
  - REQs: REQ-F-2.26
  - Check: Dead `topMargin` property is removed from Text element using `verticalCenter` anchoring; `task qml-lint` passes and layout unchanged.

- [x] T-033: Replace hardcoded BarIcon width/height with implicit sizing
  - REQs: REQ-F-2.27
  - Check: Hardcoded width/height in BarIcon are replaced with `implicitWidth`/`implicitHeight`; all BarIcon usages render at correct size and `task qml-lint` passes.

- [x] T-034: Fix or remove unreachable Passive state transition in TrayItem
  - REQs: REQ-F-2.28
  - Check: Code review determines if Passive state is reachable and fixes the guard condition, or removes the dead transition; PR notes document decision and `task qml-lint` passes.

### Unit U-11: Settings Application

- [x] T-035: Extract mode-to-scheme mapping into ThemeConfigFile::schemeForMode() helper
  - REQs: REQ-F-2.29
  - Check: New static method `ThemeConfigFile::schemeForMode()` replaces 6 call sites' hand-written ternary/if-else; settings tests pass and theme switching UI behavior unchanged.

## Final Verification

- [x] T-036: Complete Phase 6 verification checklist
  - REQs: REQ-C-1, REQ-C-2, REQ-C-3, REQ-C-4, REQ-C-5
  - Closed 2026-07-20: T-027's WheelHandler regression was resolved by accepting the documented MouseArea-revert divergence rather than a literal `WheelHandler` replacement (see T-027). `task build`, `task test`, `task tidy`, and `task qml-lint` all pass on the resulting tree; compositor-facing verification for the reverted BarSection wheel behavior was confirmed working manually.

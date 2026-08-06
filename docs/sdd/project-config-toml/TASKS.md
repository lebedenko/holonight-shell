# SDD Tasks — project-config-toml

- [x] T-001: Add ConfigService sources to CMakeLists.txt
  - REQs: REQ-C-001
  - Check: `grep -c "ConfigService" CMakeLists.txt` returns at least 2 (one for .h, one for .cpp).

- [x] T-002: Create ConfigService header with config structs and singleton interface
  - REQs: REQ-F-001, REQ-F-005, REQ-F-006, REQ-F-007, REQ-C-002
  - Check: `AppearanceConfig`, `BarWorkspacesConfig`, `BarSystemTrayConfig` structs compile with correct defaults and range constants; static `instance()` method exists.

- [x] T-003: Implement ConfigService::resolveConfigPath() with XDG fallback
  - REQs: REQ-F-001, REQ-C-006
  - Check: On a system with `XDG_CONFIG_HOME=/tmp/test-xdg`, the resolved path is `/tmp/test-xdg/holonight/config.toml`; without `XDG_CONFIG_HOME`, the path is `~/.config/holonight/config.toml` (tilde expanded).

- [x] T-004: Implement ConfigService::ensureDirectoryExists() and error handling
  - REQs: REQ-F-002, REQ-NF-001
  - Check: Directory `~/.config/holonight` is created if missing; if creation fails (e.g., read-only filesystem), a qCWarning is logged and execution continues.

- [x] T-005: Implement ConfigService::writeDefaultConfig() with inline TOML comments
  - REQs: REQ-F-002, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-020
  - Check: File `~/.config/holonight/config.toml` is created on first run with all eight font properties, workspace count (5), and tray max_items (3); range comments `# accepted: 3-10` and `# accepted: 2-5` appear inline on count and max_items keys.

- [x] T-006: Implement ConfigService::parseFile() with TOML parsing and range clamping
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-018, REQ-F-019
  - Check: A file with `count = 1` is clamped to 3 with a qCWarning logged; a file with `max_items = 10` is clamped to 5 with a qCWarning logged; invalid TOML is caught and logged at qCWarning level; all in-memory values remain unchanged on parse error; type mismatches use per-key defaults in memory.

- [x] T-006a: Persist missing defaults without treating invalid present values as writable fixes
  - REQs: REQ-F-003, REQ-F-004, REQ-F-020
  - Check: A partial config gains missing keys with default values on disk; a present out-of-range or wrong-typed value is logged and corrected in memory but is not rewritten solely because it is invalid.

- [x] T-007: Implement ConfigService::onFileChanged() and debounce timer with 200ms window
  - REQs: REQ-F-008, REQ-F-009, REQ-NF-004, REQ-NF-005
  - Check: Rapid file changes within 200ms trigger only one reload; after 200ms of no changes, `parseFile()` is called; timer is reset if another change arrives during the debounce window.

- [x] T-008: Implement ConfigService startup sequence with initial load and watcher activation
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-008, REQ-C-004
  - Check: On startup, `resolveConfigPath()` → `ensureDirectoryExists()` → `loadOrCreateConfig()` → `startWatcher()` completes without crashing; initial file write does not trigger a reload.

- [x] T-009: Implement ConfigService signal emission (appearanceChanged, barWorkspacesChanged, barSystemTrayChanged)
  - REQs: REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-011, REQ-C-007
  - Check: `appearanceChanged()` signal is emitted after all appearance values are updated; `barWorkspacesChanged()` is emitted only if count changed; `barSystemTrayChanged()` is emitted only if max_items changed; signals are not emitted on failed reload.

- [x] T-010: Implement ConfigService::instance() static getter for fallback access
  - REQs: REQ-C-002
  - Check: `ConfigService::instance()` returns pointer set in constructor; `instance()` returns `nullptr` if never constructed.

- [x] T-011: Update ThemeService header — change 8 font properties from CONSTANT to NOTIFY
  - REQs: REQ-F-015, REQ-F-017
  - Check: All 8 properties (`uiFont`, `fixedFont`, `clockFont`, `titleFont`, `uiFontSize`, `fixedFontSize`, `clockFontSize`, `titleFontSize`) have NOTIFY signals; private member variables exist for each.

- [x] T-012: Update ThemeService constructor to accept ConfigService pointer and read initial values
  - REQs: REQ-F-015, REQ-F-016, REQ-C-004
  - Check: ThemeService constructor signature is `ThemeService(ConfigService* config, QObject* parent = nullptr)`; constructor calls `applyAppearance(config->appearance())` before returning; if `config` is null, defaults are applied.

- [x] T-013: Implement ThemeService::applyAppearance() helper and connect to ConfigService signal
  - REQs: REQ-F-015, REQ-F-017
  - Check: `applyAppearance()` compares new values against current members and emits NOTIFY signals only for changed properties; ThemeService connects to `ConfigService::appearanceChanged()` and calls `applyAppearance()` on receipt.

- [x] T-014: Add displayCount Q_PROPERTY to WorkspaceModel with setter and NOTIFY signal
  - REQs: REQ-F-018, REQ-C-005
  - Check: `WorkspaceModel::displayCount()` getter and `setDisplayCount(int)` setter exist; property has default value 5; `displayCountChanged()` signal is emitted on change.

- [x] T-015: Update WorkspaceModel overflow logic to use display_count_ instead of literal 6
  - REQs: REQ-F-018
  - Check: In `overflowWorkspaceId()`, `overflowUrgentWorkspaceId()`, and `hiddenUrgentWorkspaceCount()` methods, all comparisons `entry.id > 6` are replaced with `entry.id > display_count_`.

- [x] T-016: Update ExtWorkspaceManager constructor to accept ConfigService pointer and set initial workspace count
  - REQs: REQ-F-018
  - Check: ExtWorkspaceManager constructor signature is `ExtWorkspaceManager(WorkspaceModel* model, ConfigService* config, QObject* parent = nullptr)`; on startup, `model_->setDisplayCount(config->barWorkspaces().count)` is called; connection to `barWorkspacesChanged()` is established.

- [x] T-017: Add maxVisible Q_PROPERTY to TrayModel with NOTIFY signal
  - REQs: REQ-F-019, REQ-C-005
  - Check: `TrayModel::maxVisible()` getter exists; private member `max_visible_` defaults to 3; `maxVisibleChanged()` signal is declared.

- [x] T-018: Update TrayModel constructor to accept ConfigService pointer and read initial max_items
  - REQs: REQ-F-019
  - Check: TrayModel constructor signature is `TrayModel(ConfigService* config, QObject* parent = nullptr)`; on startup, `max_visible_` is initialized from `config->barSystemTray().maxItems`; connection to `barSystemTrayChanged()` is established.

- [x] T-019: Update ShellApplication to construct ConfigService first and inject pointer into service constructors
  - REQs: REQ-C-004, REQ-C-005
  - Check: In `ShellApplication::ShellApplication()`, `ConfigService` is constructed before `ThemeService`, `TrayModel`, and `ExtWorkspaceManager`; pointer is passed to constructors of these three services.

- [x] T-020: Update TraySection.qml to bind maxVisible to TrayModel.maxVisible
  - REQs: REQ-F-019
  - Check: In TraySection.qml, `readonly property int maxVisible: 3` is replaced with `readonly property int maxVisible: TrayModel.maxVisible`; file compiles with qmllint.

- [x] T-021: Update WorkspaceSection.qml to bind Repeater model to WorkspaceModel.displayCount
  - REQs: REQ-F-018
  - Check: In WorkspaceSection.qml, `model: 6` in the Repeater is replaced with `model: WorkspaceModel.displayCount`; file compiles with qmllint.

- [x] T-022: Create ConfigService unit tests — file initialization scenarios (missing, complete, partial, corrupt)
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-NF-001, REQ-NF-003
  - Check: Tests verify that `ConfigService` initializes correctly with missing file, partial file, and corrupt TOML; missing keys are persisted with defaults; invalid present values are corrected in memory; startup completes in <100ms.

- [x] T-023: Create ConfigService unit tests — range clamping and validation
  - REQs: REQ-F-006, REQ-F-007, REQ-F-018, REQ-F-019
  - Check: Tests verify that `count = 1` clamps to 3 with warning logged; `count = 15` clamps to 10 with warning logged; `max_items = 0` clamps to 2; `max_items = 10` clamps to 5.

- [x] T-024: Create ConfigService unit tests — signal emission on first load
  - REQs: REQ-F-012, REQ-F-013, REQ-F-014
  - Check: Tests verify that `appearanceChanged()` is emitted after initial load; `barWorkspacesChanged()` is emitted; `barSystemTrayChanged()` is emitted; signals are connected and slots are invoked.

- [x] T-025: Create ConfigService unit tests — live-reload with debounce behavior
  - REQs: REQ-F-008, REQ-F-009, REQ-F-010, REQ-F-011, REQ-NF-004
  - Check: Tests verify that rapid file changes (within 200ms) trigger one reload; debounce timer resets on new change; reload failure preserves in-memory state; signals are emitted only on successful reload.

- [x] T-026: Create ConfigService unit tests — keep-on-failure after corrupt reload
  - REQs: REQ-F-011, REQ-NF-001
  - Check: Tests verify that if `ConfigService` successfully loads valid config, then config file becomes corrupt, in-memory values remain unchanged; error is logged at qCWarning; application continues running.

- [x] T-027: Verify all code follows project conventions (clang-format, clang-tidy, naming)
  - REQs: REQ-NF-006
  - Check: `task format-check` passes; `task tidy` produces no errors; all private member names use snake_case with trailing underscore; all logging uses correct category and level.

- [x] T-028: Integration test — ThemeService reads from ConfigService on startup and updates on live-reload
  - REQs: REQ-F-015, REQ-F-016, REQ-F-017
  - Check: On startup, `ThemeService.uiFont` matches `ConfigService.appearance().uiFont`; after editing config file and triggering live-reload, `ThemeService.clockFontSize` updates and NOTIFY signal is emitted.

- [x] T-029: Integration test — WorkspaceModel displayCount updates and overflow threshold changes
  - REQs: REQ-F-018, REQ-C-005
  - Check: On startup, `WorkspaceModel.displayCount` equals configured count; when count is changed via config file, workspace pills are added/removed; overflow logic uses new threshold.

- [x] T-030: Integration test — TrayModel maxVisible updates and item visibility changes
  - REQs: REQ-F-019, REQ-C-005
  - Check: On startup, `TrayModel.maxVisible` equals configured max_items; when max_items is changed via config file, visible tray items adjust; truncation threshold uses new value.

- [x] T-031: Manual testing — create and edit config file, verify live-reload in running application
  - REQs: REQ-F-010, REQ-F-012, REQ-F-013, REQ-F-014
  - Check: Using `test-env` skill, create `~/.config/holonight/config.toml` with custom font size, start shell, then edit config and save; within 200ms, fonts and bar layout update visually; no UI freeze observed.

- [x] T-032: Manual testing — corrupt config file and verify graceful fallback
  - REQs: REQ-F-004, REQ-F-011, REQ-NF-001
  - Check: With shell running, edit `~/.config/holonight/config.toml` to invalid TOML (e.g., unclosed bracket); save file; within 200ms shell logs qCWarning; UI does not update; previous valid config remains in effect.

- [x] T-033: Manual testing — delete config file and verify defaults applied
  - REQs: REQ-F-002, REQ-NF-001
  - Check: Delete `~/.config/holonight/config.toml` while shell is running; shell logs qCWarning on reload; UI continues with previous (or default) values; no crash occurs.

- [x] T-034: User approval — mark SDD pipeline finished
  - REQs: SDD process
  - Check: After implementation and validation are reviewed by the user, mark the project-config-toml SDD pipeline complete.

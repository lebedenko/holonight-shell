# SPEC: project-config-toml

**Feature:** TOML configuration file support for holonight-shell with live-reload and theme/bar integration  
**Status:** Requirements specification  
**Date:** 2026-05-28

---

## Overview

holonight-shell shall support persistent configuration via a TOML file at `$XDG_CONFIG_HOME/holonight/config.toml`. The `ConfigService` singleton shall manage reading, merging, validation, and live-reload of this configuration. Font properties currently hardcoded in ThemeService shall migrate to the `[appearance]` section; bar workspace/system-tray subsections shall control UI behavior.

The system shall never crash on configuration errors. Corrupt TOML shall be logged and ignored; present but invalid
values shall be logged and corrected in memory by using defaults or clamping to the nearest allowed range boundary.
Missing values shall be added back to the config file with defaults so users receive new options after upgrades.

---

## Functional Requirements

### Configuration File Location & Creation

#### REQ-F-001: Resolve config file path with XDG fallback
**Statement:** The system shall resolve the configuration file path as follows:
1. If `$XDG_CONFIG_HOME` is set and non-empty, use `$XDG_CONFIG_HOME/holonight/config.toml`
2. Otherwise, use `~/.config/holonight/config.toml`

**Acceptance criteria:**
- On a system with `XDG_CONFIG_HOME=/custom/config`, the config file is at `/custom/config/holonight/config.toml`
- On a system without `XDG_CONFIG_HOME`, the config file defaults to `~/.config/holonight/config.toml`
- The path is correctly expanded (tilde resolving, environment variables interpolated) on startup

#### REQ-F-002: Create default config file if missing
**Statement:** When the application starts and the config file does not exist, the system shall create the file with default values in all defined sections.

**Acceptance criteria:**
- File is created at the resolved path with correct directory structure (`holonight/` directory created if needed)
- File contains all sections and keys defined in the defaults (appearance, bar.workspaces, bar.systemtray)
- File is readable and valid TOML
- Log message (qCInfo) confirms file creation with the file path
- If the directory cannot be created (permission denied, read-only filesystem), log qCWarning and continue with in-memory defaults

#### REQ-F-003: Merge partial config with defaults and persist missing keys
**Statement:** When the application starts or live-reloads and the config file exists but is missing keys or
subsections, the system shall merge missing keys/subsections with defaults in memory and add the missing key/value
pairs to the config file.

**Acceptance criteria:**
- A config file with only `[appearance]` section and partial keys (e.g., only `ui_font`) is supplemented with missing keys from defaults
- A config file missing entire subsections (e.g., no `[bar.workspaces]`) has those subsections created in-memory from defaults
- Missing keys are written to disk with default values, including range comments for range-constrained keys
- Present valid keys keep their configured values
- Present invalid keys are corrected in memory but are not rewritten solely because they are invalid
- Merged configuration and added defaults are logged at qCDebug level (enable with `QT_LOGGING_RULES="holonight.config.debug=true"`)

#### REQ-F-004: Log and recover from corrupt or invalid config
**Statement:** If the config file exists but is corrupt TOML or contains invalid values, the system shall log a
warning and continue with corrected in-memory values.

**Acceptance criteria:**
- Malformed TOML syntax (e.g., unclosed brackets, invalid key=value) is caught; error details logged at qCWarning level
- Malformed TOML is ignored during startup, leaving default in-memory values active
- Malformed TOML is ignored during live-reload, leaving the last valid in-memory values active
- Type mismatches (e.g., `ui_font_size = "not an integer"`) are logged with key name and expected type; that value uses its default in memory
- Range violations are logged and clamped to the nearest allowed value in memory
- The application continues without crashing
- Invalid present values are not rewritten to disk solely because they are invalid

---

### Configuration Sections & Defaults

#### REQ-F-005: Appearance section with font properties
**Statement:** The system shall define an `[appearance]` section with four font families and four font sizes, each with a default value.

**Acceptance criteria:**
- `[appearance]` section is present in default config
- Keys defined:
  - `ui_font = "Inter"` (string, default)
  - `ui_font_size = 12` (integer, pixels, default)
  - `fixed_font = "JetBrains Mono"` (string, default)
  - `fixed_font_size = 12` (integer, pixels, default)
  - `clock_font = "Rajdhani"` (string, default)
  - `clock_font_size = 24` (integer, pixels, default)
  - `title_font = "Audiowide"` (string, default)
  - `title_font_size = 8` (integer, pixels, default)
- All font families are strings; all font sizes are positive integers
- Defaults match current ThemeService hardcoded values

#### REQ-F-006: Bar workspaces subsection
**Statement:** The system shall define a `[bar.workspaces]` subsection with a `count` key that controls the number of workspace indicators displayed. The allowed range is 3–10.

**Acceptance criteria:**
- `[bar.workspaces]` subsection is present in default config
- Key defined: `count = 5` (integer, default)
- Allowed range is 3–10 inclusive
- If `count` < 3, a qCWarning is logged and the value is clamped to 3
- If `count` > 10, a qCWarning is logged and the value is clamped to 10
- The default config file includes an inline comment documenting the accepted range (e.g., `# accepted: 3–10`)

#### REQ-F-007: Bar system-tray subsection
**Statement:** The system shall define a `[bar.systemtray]` subsection with a `max_items` key that controls the maximum number of tray items visible. The allowed range is 2–5.

**Acceptance criteria:**
- `[bar.systemtray]` subsection is present in default config
- Key defined: `max_items = 3` (integer, default)
- Allowed range is 2–5 inclusive
- If `max_items` < 2, a qCWarning is logged and the value is clamped to 2
- If `max_items` > 5, a qCWarning is logged and the value is clamped to 5
- The default config file includes an inline comment documenting the accepted range (e.g., `# accepted: 2–5`)

---

### Live-Reload

#### REQ-F-008: Watch config file for changes
**Statement:** The system shall watch the configuration file for modifications using QFileSystemWatcher.

**Acceptance criteria:**
- On application startup (after initial load), ConfigService starts watching the config file path
- File system events on the config file are captured
- If the file is deleted and recreated (e.g., editor atomic-write), the watcher continues to detect changes
- Watcher is not started during the initial startup load (no spurious reload)

#### REQ-F-009: Debounce rapid file changes
**Statement:** When the config file is modified, the system shall debounce reloads with a 200ms timer to avoid processing rapid successive writes.

**Acceptance criteria:**
- If the file changes twice within 200ms, only one reload attempt is triggered
- After 200ms with no further changes, the reload proceeds
- If the debounce timer is running and another change arrives, the timer resets to 200ms
- Debounce interval is configurable in ConfigService (default 200ms)

#### REQ-F-010: Reload config on file change
**Statement:** When the config file is modified and the debounce timer expires, the system shall reload the file and re-parse all values.

**Acceptance criteria:**
- File is re-read from disk
- TOML parsing is re-applied (same validation as initial load)
- All in-memory values are updated if parsing succeeds
- Changed values trigger appropriate signals (e.g., appearanceChanged, barWorkspacesChanged)
- Reload is logged at qCDebug level with outcome (success or error)

#### REQ-F-011: Preserve in-memory state on reload failure
**Statement:** If the config file becomes unreadable or malformed during a live-reload, the system shall log an
error and keep the current in-memory values unchanged.

**Acceptance criteria:**
- If reload fails due to corrupt TOML or I/O error, no in-memory values are modified
- If reload succeeds but present values are invalid, those values are corrected in memory according to REQ-F-004
- Error is logged at qCWarning level with details (key, expected type, actual value, or I/O error)
- Application continues running with the previous valid configuration
- Invalid present values are not rewritten to disk solely because they are invalid

---

### Signal Emission

#### REQ-F-012: Emit appearanceChanged signal
**Statement:** When any font family or font size key in the `[appearance]` section is modified (on startup or live-reload), the system shall emit the `appearanceChanged()` signal.

**Acceptance criteria:**
- Signal is emitted once per reload cycle, even if multiple appearance keys changed
- Signal is emitted after all appearance values are updated in-memory
- Connected slots in ThemeService are invoked to notify QML consumers
- Signal is emitted if merged or corrected appearance values differ from current in-memory values

#### REQ-F-013: Emit barWorkspacesChanged signal
**Statement:** When the `count` key in `[bar.workspaces]` is modified (on startup or live-reload), the system shall emit the `barWorkspacesChanged()` signal.

**Acceptance criteria:**
- Signal is emitted once per reload cycle if count changed
- Signal is not emitted if count is unchanged between reload cycles
- Connected slots in ExtWorkspaceManager are invoked to update workspace display logic
- Signal includes or allows retrieval of the new count value

#### REQ-F-014: Emit barSystemTrayChanged signal
**Statement:** When the `max_items` key in `[bar.systemtray]` is modified (on startup or live-reload), the system shall emit the `barSystemTrayChanged()` signal.

**Acceptance criteria:**
- Signal is emitted once per reload cycle if max_items changed
- Signal is not emitted if max_items is unchanged between reload cycles
- Connected slots in TrayModel are invoked to update tray item visibility/truncation logic
- Signal includes or allows retrieval of the new max_items value

---

### ThemeService Integration

#### REQ-F-015: Move font properties from ThemeService CONSTANT to NOTIFY
**Statement:** ThemeService shall expose the four font families and four font sizes as Q_PROPERTY with NOTIFY slots instead of CONSTANT.

**Acceptance criteria:**
- Properties: `uiFont`, `uiFontSize`, `fixedFont`, `fixedFontSize`, `clockFont`, `clockFontSize`, `titleFont`, `titleFontSize`
- Each property has a NOTIFY signal (e.g., `uiFontChanged()`)
- Initial values are read from ConfigService on startup
- Properties are updated when ConfigService emits `appearanceChanged()`
- QML imports remain unchanged; properties are still accessed as `ThemeService.uiFont` etc.
- Properties remain QML-readable (QML_ELEMENT context)

#### REQ-F-016: ThemeService queries ConfigService on startup
**Statement:** When ThemeService is constructed, it shall query ConfigService for current appearance values and update its properties.

**Acceptance criteria:**
- ConfigService is fully initialized (file loaded or defaults applied) before ThemeService reads from it
- All four font families are retrieved and stored in ThemeService properties
- All four font sizes are retrieved and stored in ThemeService properties
- If ConfigService is unavailable, hardcoded defaults are used

#### REQ-F-017: ThemeService responds to live-reload
**Statement:** When ConfigService emits `appearanceChanged()`, ThemeService shall update its properties and emit corresponding NOTIFY signals.

**Acceptance criteria:**
- ThemeService connects to `ConfigService::appearanceChanged()` signal
- On signal emission, all four font families and sizes are re-queried from ConfigService
- Each property's NOTIFY signal is emitted (triggering QML re-evaluations)
- Running application UI updates reflected in real-time if QML bindings are used

---

### Bar Integration

#### REQ-F-018: ExtWorkspaceManager respects workspace count config
**Statement:** The workspace count from `[bar.workspaces]` shall control the number of workspace pills displayed in the topbar.

**Acceptance criteria:**
- ExtWorkspaceManager queries ConfigService for `bar.workspaces.count` on startup
- Workspace display logic uses the configured count (not a hardcoded literal)
- If `count` is changed via live-reload, workspace pills are added/removed accordingly
- Values outside 3–10 are clamped before being applied (per REQ-F-006); the clamped value is used, not the default

#### REQ-F-019: TrayModel respects max_items config
**Statement:** The max_items count from `[bar.systemtray]` shall control the maximum number of tray items visible in the topbar.

**Acceptance criteria:**
- TrayModel queries ConfigService for `bar.systemtray.max_items` on startup
- Tray item truncation/visibility logic uses the configured max_items (not a hardcoded literal)
- If `max_items` is changed via live-reload, visible tray items adjust accordingly
- Values outside 2–5 are clamped before being applied (per REQ-F-007); the clamped value is used, not the default

#### REQ-F-020: Inline comments in generated config file document accepted ranges
**Statement:** When the system generates the default config file, it shall include an inline comment on each key with a constrained range documenting the accepted values.

**Acceptance criteria:**
- `count` key in `[bar.workspaces]` has an inline comment stating the accepted range (e.g., `count = 5 # accepted: 3–10`)
- `max_items` key in `[bar.systemtray]` has an inline comment stating the accepted range (e.g., `max_items = 3 # accepted: 2–5`)
- Comments are present in the file created on first run and when missing range-constrained keys are added during merge

---

## Non-Functional Requirements

### Reliability & Robustness

#### REQ-NF-001: Never crash on config errors
**Statement:** The system shall never crash due to configuration file issues (missing, corrupt, invalid, unreadable).

**Acceptance criteria:**
- Application starts and runs to completion with invalid/missing config file
- No unhandled exceptions are thrown by ConfigService
- All error paths log diagnostics (file path, error type, reason)
- Default values are always available as fallback

#### REQ-NF-002: Atomic file operations
**Statement:** Configuration file operations (creation, read, write) shall be atomic or resilient to concurrent access.

**Acceptance criteria:**
- File creation uses safe patterns (e.g., temporary file + rename, or atomic write modes)
- Reading the file while it is being written does not corrupt in-memory state; malformed intermediate reads are logged and ignored
- QFileSystemWatcher correctly detects file changes even under rapid writes

#### REQ-NF-003: Startup performance
**Statement:** Configuration loading shall not block application startup beyond 100ms.

**Acceptance criteria:**
- Config file read, parse, and merge complete in <100ms on typical systems
- If I/O is slow (e.g., network filesystem), defaults are used without hanging
- Measurement is taken from application launch to full initialization

### Performance

#### REQ-NF-004: Debounce prevents reload storms
**Statement:** The 200ms debounce shall prevent excessive reloads when the editor writes the file multiple times.

**Acceptance criteria:**
- Editing config file (e.g., in a text editor) with auto-save triggers only one reload attempt per edit pause, not per auto-save occurrence
- Debounce overhead is negligible (<1ms timer management)

#### REQ-NF-005: Live-reload does not block UI
**Statement:** Configuration reload shall occur asynchronously and not freeze the UI.

**Acceptance criteria:**
- File watching and debounce timer are on the main event loop but do not block rendering
- QFileSystemWatcher is non-blocking (standard Qt behavior)
- Parsing happens on the main thread but completes quickly (<50ms typical)

### Observability

#### REQ-NF-006: Diagnostic logging at multiple levels
**Statement:** The system shall log configuration operations at appropriate logging levels.

**Acceptance criteria:**
- qCInfo: file creation, successful startup load, live-reload success
- qCDebug: parse details, merged keys, appearance/bar section updates (enable with `QT_LOGGING_RULES="holonight.config.debug=true"`)
- qCWarning: missing file creation permission, type mismatches, corrupt TOML, reload failures, invalid key values (0 count, negative size)

#### REQ-NF-007: Logged config values do not expose secrets
**Statement:** Logging shall not include any values that could be considered secrets.

**Acceptance criteria:**
- No passwords, API keys, or tokens are logged
- Font names, sizes, and counts are safe to log
- File paths are logged (needed for debugging)

---

## Constraints

### Technology & Architecture

#### REQ-C-001: Use tomlplusplus library
**Statement:** The system shall use `tomlplusplus` (tomlplusplus package on Arch Linux) for TOML parsing.

**Acceptance criteria:**
- Dependency is added to CMakeLists.txt
- tomlplusplus headers are included in ConfigService
- TOML parsing uses the library's API (not manual string parsing)
- Build succeeds with system-installed tomlplusplus

#### REQ-C-002: ConfigService is C++-only singleton
**Statement:** ConfigService shall be a QObject-based singleton accessible only from C++ in this iteration.

**Acceptance criteria:**
- Class is not exposed to QML (no Q_INVOKABLE, no QML_ELEMENT)
- ThemeService and bar services query ConfigService via C++ getter methods
- No QML property access to ConfigService is possible

#### REQ-C-003: No QML runtime mutation of config
**Statement:** Configuration changes must not be writable from QML in this iteration.

**Acceptance criteria:**
- No Q_INVOKABLE mutator methods exist
- Config changes only occur via file edits + live-reload
- QML consumers only read config values (via ThemeService, bar services)

#### REQ-C-004: Application must not crash on startup
**Statement:** Configuration loading shall use defensive programming and never cause application initialization failure.

**Acceptance criteria:**
- Every config operation is wrapped in try-catch or error-checking logic
- Default values are always in-memory before any service queries ConfigService
- If ConfigService initialization fails, qFatal is not called — defaults are used instead

---

### Design & Integration

#### REQ-C-005: Default values match current behavior
**Statement:** Default configuration values shall match the current hardcoded behavior so existing behavior is preserved without a config file.

**Acceptance criteria:**
- Font defaults match ThemeService hardcoded values (Inter, JetBrains Mono, Rajdhani, Audiowide; sizes 12, 12, 24, 8)
- Workspace count default is 5 (current ExtWorkspaceManager)
- Tray max_items default is 3 (current TrayModel)
- User upgrading to this version sees no visual/behavioral change without editing config

#### REQ-C-006: File path follows XDG Base Directory Specification
**Statement:** The configuration file path shall follow the XDG Base Directory Specification.

**Acceptance criteria:**
- Uses `$XDG_CONFIG_HOME` if set; otherwise `~/.config`
- Directory path is `$XDG_CONFIG_HOME/holonight/config.toml` (not `$XDG_CONFIG_HOME/holonight-shell/` or other variant)
- Behavior matches standard XDG conventions used by other GNOME/Wayland applications

#### REQ-C-007: Signal names follow Qt naming conventions
**Statement:** ConfigService signals shall follow Qt NOTIFY signal naming conventions.

**Acceptance criteria:**
- Signals are lowercase with "Changed" suffix: `appearanceChanged()`, `barWorkspacesChanged()`, `barSystemTrayChanged()`
- Signals are emitted after all state updates are complete
- Signals are connected via Qt::QueuedConnection if cross-thread (though ConfigService is on main thread)

---

## Out of Scope

The following features are explicitly out of scope for this iteration:

- **QML access to ConfigService** — no Q_INVOKABLE methods, no QML_ELEMENT
- **Color/palette overrides** — theme switching via config file
- **Per-monitor configuration** — config applied per output/screen
- **Runtime config mutation** — no QML-triggered config writes; ConfigService may write missing defaults to the TOML file
- **Additional bar sections** — only `workspaces` and `systemtray` are configurable
- **Config validation UI** — no error dialogs or hints; errors logged only
- **Config schema version** — no migration path; any incompatible change requires manual file edit

---

## Acceptance Criteria Summary

### User Perspective
- User upgrades holonight-shell; application starts with same appearance and bar behavior (defaults applied)
- User creates `~/.config/holonight/config.toml` with custom font size; application respects the change on next start
- User edits the file while the application is running; within 200ms of saving, the UI updates (live-reload)
- User makes a TOML syntax typo; application logs a warning but continues running with previous values
- User enters an invalid value; application logs a warning and uses the corrected value in memory

### Developer Perspective
- ConfigService is a clean singleton with getters for each config section
- ThemeService and bar services easily integrate by querying ConfigService and connecting to change signals
- No crashes occur from malformed config; all error paths are logged and tested
- Code follows project conventions (clang-format, clang-tidy, naming)

---

## Test Strategy

### Unit Tests (GTest)
- ConfigService initialization with various file states (missing, complete, partial, corrupt)
- Config merge logic (defaults + partial config)
- TOML parsing and type validation
- Signal emission on changes
- Debounce timer behavior (rapid changes, timer reset)

### Integration Tests
- ThemeService queries ConfigService and updates on live-reload
- ExtWorkspaceManager and TrayModel respect config values
- File watcher detects changes on various editor save patterns
- No crashes on invalid TOML

### Manual Testing (test-env skill)
- Create/edit config file and verify live-reload visually
- Verify fonts and tray count change in running application
- Corrupt config file and verify graceful fallback
- Remove config file and verify defaults are applied

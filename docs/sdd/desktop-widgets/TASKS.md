# SDD Tasks — desktop-widgets

- [x] T-001: Add WidgetPosition enum and config structs to ConfigService
  - REQs: REQ-F-012, REQ-F-018, REQ-F-019, REQ-F-031
  - Check: WidgetPosition, TimeToEventConfig, WidgetDefinition, WidgetsConfig compile and serialize with operator==.

- [x] T-002: Implement TOML parser for widgets configuration
  - REQs: REQ-F-018, REQ-F-019, REQ-F-026, REQ-F-027, REQ-F-028, REQ-F-029, REQ-F-030, REQ-F-031
  - Check: Parse a [widgets] margin entry and [[widget]] array from config.toml; missing title/deadline/type trigger single warnings; position and show_seconds default without warning.

- [x] T-003: Add configuration validation and warn-once for unknown monitors
  - REQs: REQ-F-008, REQ-F-011, REQ-F-026, REQ-F-027, REQ-F-028
  - Check: Undefined monitor names log exactly one warning per parse; future hot-plug of that monitor creates surfaces without re-warning. (NOTE: per-field validation + defaults done in ConfigService; unknown-monitor warn-once intentionally moved to the coordinator in T-013/T-016 where screen knowledge lives — ConfigService stays a pure parser.)

- [x] T-004: Add widgetsChanged signal and live-reload debouncing to ConfigService
  - REQs: REQ-F-032, REQ-F-033
  - Check: Modify config.toml, widget surfaces appear/disappear within 200ms; rapid edits (2× in 100ms) trigger only one rebuild. (Reuses existing QFileSystemWatcher + 200ms debounce; widgetsChanged emitted on change-detection diff.)

- [x] T-005: Extract kBarHeight to ShellConstants.h and update LayerShellManager
  - REQs: REQ-F-014, REQ-NF-002
  - Check: LayerShellManager includes ShellConstants.h; kBarHeight is no longer defined locally; bar positioning unchanged.

- [x] T-006: Add shouldCreateSurface virtual to PerMonitorLayerManager
  - REQs: REQ-NF-002
  - Check: Virtual method added with default-true implementation; no existing subclasses break. (Guard added in createSurface so both initial enumeration and hotplug skip non-targeted screens.)

- [x] T-007: Add visibleWorkspaceIdForMonitor accessor to ActiveWindowService
  - REQs: REQ-F-004, REQ-F-007
  - Check: Method returns visible workspace ID per monitor name; returns -1 for unknown monitors.

- [x] T-008: Add isWorkspaceOccupied accessor to WorkspaceModel
  - REQs: REQ-F-004, REQ-F-007
  - Check: Method checks occupied_workspace_ids_ and returns bool for given workspace ID.

- [x] T-009: Implement MonitorOccupancyService
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007
  - Check: Compiles; subscribed to ActiveWindowService::monitorWindowChanged and WorkspaceModel::revisionChanged; emits occupancyChanged on state transition. (Takes WorkspaceModel*+ActiveWindowService* directly; revisionChanged sweeps QGuiApplication::screens(); registered in holonight_services.)

- [x] T-010: Implement countdown formatting (pure, unit-tested) — timer logic deferred to T-011
  - REQs: REQ-F-022, REQ-F-023, REQ-F-025, REQ-F-021
  - Check: formatCountdown() outputs adaptive format; sub-minute with show_seconds=false emits "00m"; "Now" at/after deadline; date label fixed-format. 10 unit tests pass (test_widget_countdown.cpp). (Implemented as free functions in WidgetCountdown.h/.cpp for testability; timer/minute-sync (REQ-F-024) folded into T-011.)

- [x] T-011: Implement WidgetManager skeleton with occupancy gating and collision support
  - REQs: REQ-F-001, REQ-F-005, REQ-F-006, REQ-F-008, REQ-F-009, REQ-F-010, REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-024, REQ-NF-002, REQ-NF-006
  - Check: WidgetManager inherits PerMonitorLayerManager; surfaces created per targeted monitor; show/hide via occupancyChanged + LayerSurface::configured for initial state; layer=bottom, exclusive_zone=-1, input-transparent. Single-shot member QTimer with minute-boundary alignment + freeze/resume (no dangling singleShot). Compiles clean.

- [x] T-012: Implement surface positioning logic (anchors, margins, bar height offset)
  - REQs: REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-017
  - Check: anchorFlagsForPosition maps all 9 positions to layer-shell anchors; set_margin applies uniform margin, top-anchored adds kBarHeight; center-center uses no anchor (compositor-centred). (Folded into WidgetManager::configureSurface; visual confirmation in T-018.)

- [x] T-013: Implement collision detection and warn-once in coordinator
  - REQs: REQ-F-015, REQ-F-016, REQ-F-017
  - Check: Coordinator computes per-widget position-blockers (earlier same-position widgets' monitor filters); WidgetManager::blockedOn drops losing widget per monitor with warn-once; unknown-monitor warn-once in coordinator. Different positions/monitors unaffected. (Visual confirmation in T-018.)

- [x] T-014: Create WidgetSurface.qml and TimeToEventWidget.qml QML components
  - REQs: REQ-F-018, REQ-F-020, REQ-F-021, REQ-NF-001, REQ-NF-004
  - Check: WidgetSurface loads correct widget type via Loader; TimeToEventWidget renders title/countdown/date-label with HoloniightPalette colors and ThemeService fonts; MultiEffect glow declared before content (z-order); qmllint clean, AOT-compiles. No hardcoded hex or font-family.

- [x] T-015: Wire CMake build integration for new C++ files and QML components
  - REQs: REQ-NF-003, REQ-NF-004, REQ-NF-005
  - Check: MonitorOccupancyService + WidgetManager + WidgetCountdown in correct libraries; both QML files in HOLONIGHT_QML_FILES; project configures and builds without CMake errors.

- [x] T-016: Integrate MonitorOccupancyService and WidgetManager into ShellApplication startup
  - REQs: REQ-F-001, REQ-F-003, REQ-F-032, REQ-F-033
  - Check: ShellApplication constructs MonitorOccupancyService in ctor; builds widget_managers_ vector in startLayerSurfaces(); connects ConfigService::widgetsChanged → rebuildWidgets(). Member order tears widgets down before LayerShell. Full app builds/links clean.

- [x] T-017: Verify layer-shell surface unmapping on hide() and implement fallback
  - REQs: REQ-F-002, REQ-F-005, REQ-F-006
  - Check: CONFIRMED the design's flagged risk — QQuickView::hide()/show() breaks the manually-bound wlr layer-shell role on remap (widget hid but never reappeared). FIX: keep the surface mapped and toggle the QML root's visibility instead (transparent + input-transparent when hidden; QML stays cached). Verified live full cycle: empty→visible, occupied→hidden, empty→remapped with wall-clock resync.

- [x] T-018: Manual visual and functional verification
  - REQs: REQ-F-001 through REQ-F-033, REQ-NF-001 through REQ-NF-007
  - Check: Verified live on Hyprland (2 monitors). Widget renders title/glowing-countdown/date with theme colors+fonts at center-bottom; per-monitor occupancy gate (eDP-1 empty→shown, DP-3 occupied→hidden); countdown ticks every 1s and resyncs after freeze; live config reload moves widget center-bottom→center-top below the bar (barHeight+margin) without restart; date label "2027-01-01 00:00" (HH:mm). KNOWN ISSUE: rare layer-shell startup race ("layerSurface was not configured, but a buffer was attached") under very rapid kill+relaunch; 5/5 clean restarts succeeded — low-frequency, pre-existing class, noted for follow-up.

# SDD Tasks — topbar-skeleton

- [x] T-001: Create LayerShellManager C++ class header with Wayland bindings
  - REQs: REQ-F-001, REQ-F-002, REQ-C-002
  - Check: `src/LayerShellManager.h` defines `OutputRecord` struct, `wl_registry*`, `zwlr_layer_shell_v1`, and `createBar(wl_output*)` method.

- [x] T-002: Implement LayerShellManager constructor, registry listener, and output discovery
  - REQs: REQ-F-001, REQ-C-001, REQ-C-002
  - Check: `LayerShellManager()` connects to Wayland display, registers `wl_registry_listener`, and calls `wl_display_roundtrip()` without crashing.

- [x] T-003: Implement LayerShellManager::createBar() to bind layer surfaces and configure geometry
  - REQs: REQ-F-001, REQ-F-003, REQ-F-004, REQ-C-001, REQ-C-002
  - Check: `createBar()` obtains `wl_surface`, calls `get_layer_surface()`, sets anchor/size/exclusive_zone/margin, and sends `wl_surface_commit()`.

- [x] T-004: Add QQuickView instantiation and Wayland surface extraction in LayerShellManager
  - REQs: REQ-F-002, REQ-C-001
  - Check: `createBar()` creates `QQuickView`, calls `QPlatformNativeInterface` to extract `wl_surface*`, and stores view in `OutputRecord`.

- [x] T-005: Implement LayerShellManager configure event handling and view resizing
  - REQs: REQ-F-003, REQ-F-004, REQ-C-001
  - Check: Subclass `zwlr_layer_surface_v1`, override `zwlr_layer_surface_v1_configure()`, call `ack_configure()`, and resize view to configured width and height 42.

- [x] T-006: Update main.cpp to instantiate LayerShellManager and run QGuiApplication
  - REQs: REQ-C-001
  - Check: `main()` creates `QGuiApplication`, instantiates `LayerShellManager`, and calls `app.exec()` without compile errors.

- [x] T-007: Create QML directory structure under src/qml/Topbar/
  - REQs: REQ-C-003
  - Check: Directories exist: `src/qml/Topbar/`, `src/qml/Topbar/BarBackground/`, `src/qml/Topbar/BarSection/`, `src/qml/Topbar/LogoSection/`, `src/qml/Topbar/StatusSection/`.

- [x] T-008: Create TopBar.qml root component with RowLayout and child sections
  - REQs: REQ-F-008, REQ-C-002, REQ-C-003
  - Check: `src/qml/Topbar/TopBar.qml` exists, sets width/height, uses RowLayout with LogoSection, spacer, and StatusSection; no hardcoded colors.

- [x] T-009: Create BarBackground.qml with surface color and 1px bottom border
  - REQs: REQ-F-010, REQ-F-004, REQ-F-005, REQ-C-002, REQ-C-003
  - Check: `src/qml/Topbar/BarBackground.qml` uses `HoloniightPalette.surface`, `HoloniightPalette.borderPassive`, has radius 16, and draws 1px bottom Rectangle.

- [x] T-010: Create BarSection.qml reusable component with transparent container
  - REQs: REQ-F-009, REQ-C-002, REQ-C-003
  - Check: `src/qml/Topbar/BarSection.qml` has `default property alias content`, 8px padding, implicitHeight 42, transparent background.

- [x] T-011: Create LogoSection.qml with Canvas-drawn Arch triangle and "ARCH" text
  - REQs: REQ-F-006, REQ-F-005, REQ-C-002, REQ-C-003
  - Check: `src/qml/Topbar/LogoSection.qml` uses Canvas with equilateral triangle geometry (0.577 multiplier), "ARCH" text, colors from `HoloniightPalette.onSurface`.

- [x] T-012: Create StatusSection.qml with JetBrains Mono clock and 1-second Timer
  - REQs: REQ-F-007, REQ-F-005, REQ-C-002, REQ-C-003
  - Check: `src/qml/Topbar/StatusSection.qml` displays `HH:mm` format, uses `JetBrains Mono` font, Timer interval 1000 ms, color from `HoloniightPalette.onSurface`.

- [x] T-013: Register QML files in CMakeLists.txt under HolonightShell.Topbar module
  - REQs: REQ-F-011, REQ-C-001, REQ-C-003
  - Check: `CMakeLists.txt` includes `QML_FILES` entries for TopBar.qml, BarBackground.qml, BarSection.qml, LogoSection.qml, StatusSection.qml under `qt6_add_qml_module()`.

- [x] T-014: Add LayerShellManager C++ sources to CMakeLists.txt executable target
  - REQs: REQ-C-001, REQ-C-003
  - Check: `CMakeLists.txt` lists `src/LayerShellManager.h` and `src/LayerShellManager.cpp` in `qt6_add_executable()`.

- [x] T-015: Build and verify no C++ compile errors or warnings
  - REQs: REQ-NF-001, REQ-C-002
  - Check: `task build` completes successfully with zero errors and zero warnings.

- [x] T-016: Run qml-lint and verify zero QML linting violations
  - REQs: REQ-NF-002, REQ-C-002
  - Check: `task qml-lint` output reports zero errors and zero warnings for all TopBar QML files.

- [x] T-017: Verify bar renders on all connected outputs in Wayland session
  - REQs: REQ-F-001, REQ-F-002, REQ-F-004, REQ-NF-004
  - Check: `task run` on Hyprland shows bar at top of each display, 42px height, 8px margins, within <1 second startup.

- [x] T-018: Verify exclusive zone prevents windows from rendering beneath bar
  - REQs: REQ-F-003, REQ-F-004
  - Check: Open windows on Hyprland; windows snap/tile below the bar, no content renders under the 42px bar height.

- [x] T-019: Verify bar colors match HolonightPalette and design system
  - REQs: REQ-F-005, REQ-F-010
  - Check: Bar background is `#161925` (surface), bottom border is `#565f89` (borderPassive), logo/clock text is `#c0caf5` (onSurface); cross-reference with design system.

- [x] T-020: Verify Arch triangle logo renders procedurally and "ARCH" text displays
  - REQs: REQ-F-006
  - Check: Left section shows equilateral triangle and "ARCH" text; triangle visually matches HoloNight design reference.

- [x] T-021: Verify clock displays current time in HH:mm format with JetBrains Mono font
  - REQs: REQ-F-007
  - Check: Right section displays time (e.g., "14:32"), matches system clock, font is JetBrains Mono (glyph inspection).

- [x] T-022: Verify clock updates every second without freezing or drift
  - REQs: REQ-F-007, REQ-NF-003
  - Check: Observe clock for 10 seconds; time increments by exactly 1 second each update; no frame drops or visual jank.

- [x] T-023: Verify 60 FPS rendering on up to 3 outputs with static content
  - REQs: REQ-NF-003
  - Check: Profile render frame rate on 3-output Hyprland session; maintain 60 FPS or higher on all outputs with static logo and clock.

- [x] T-024: Verify no hardcoded hex color values in bar code
  - REQs: REQ-F-005
  - Check: Grep for `#[0-9a-fA-F]{6}` in `src/qml/Topbar/*.qml`; zero matches (all colors sourced from `HoloniightPalette`).

- [x] T-025: Verify bar component hierarchy follows per-directory structure
  - REQs: REQ-C-003
  - Check: `src/qml/Topbar/` contains TopBar.qml, BarBackground.qml, BarSection.qml, LogoSection.qml, StatusSection.qml at module root level.

- [x] T-026: Verify no Hyprland IPC, DBus, or compositor-specific code
  - REQs: REQ-C-007
  - Check: Grep codebase for `hyprctl`, `dbus`, `IPC`, `socket`; zero matches related to compositor integration (Wayland protocol calls are permitted).

- [x] T-027: Verify no dynamic state beyond clock time
  - REQs: REQ-C-008
  - Check: Bar displays only logo and clock; no window titles, workspace names, battery, audio, or network data.

- [x] T-028: Verify no interactive elements respond to input
  - REQs: REQ-C-009
  - Check: Click bar sections and keyboard input; bar does not react, no menus open, no state changes.

- [x] T-029: Verify no visual effects or animations applied
  - REQs: REQ-C-010
  - Check: Bar background and sections are flat solid colors; no glow, blur, gradients, shaders, or animations observed.

- [x] T-030: Verify bar does not respond to output hotplug events
  - REQs: REQ-C-011
  - Check: Connect/disconnect displays after startup; bar instances remain unchanged (no dynamic add/remove of bar windows).

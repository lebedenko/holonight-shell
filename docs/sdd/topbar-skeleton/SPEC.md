# SPEC: Top Bar Skeleton

**SDD Session:** topbar-skeleton  
**Feature:** Foundational layer surface and QML structure for the HoloNight shell top bar  
**Status:** Specification  
**Last Updated:** 2026-05-21

---

## Overview

The top bar skeleton establishes the visual and functional foundation for the HoloNight shell UI layer. It comprises:

1. **Wayland Integration** — C++ `LayerShellManager` that discovers all connected outputs via `wl_output` globals and creates one layer surface per output using `wlr-layer-shell`.
2. **Rendering Per-Output** — Each output gets a dedicated `QQuickView` instance rendering the bar QML in isolation.
3. **Bar Geometry & Styling** — 42px height, 8px margins, 16px corner radius, dark background with 1px bottom border.
4. **QML Component Hierarchy** — Modular structure with `TopBar.qml` as root, reusable `BarSection.qml` segments, and focused domain sections (`LogoSection.qml`, `StatusSection.qml`).
5. **Dynamic Content** — Real-time clock using Qt.Timer, sourcing all colors from a centralized `HolonightPalette` singleton.

This specification defines the functional, non-functional, and constraint requirements using the EARS (Easy Approach to Requirements Syntax) format.

---

## Requirements

### REQ-F-001: Layer Surface Creation on Output Discovery

**Template:** Ubiquitous  
**Requirement:**  
The application SHALL create one `wl_layer_surface` on the `wlr-layer-shell` protocol for every `wl_output` global advertised by the Wayland compositor at startup.

**Rationale:**  
Multiple connected displays require separate layer surfaces to ensure each screen has its own bar instance with correct positioning and rendering.

**Notes:**  
- Uses `wl_registry` global listener in `LayerShellManager`.
- Single-threaded synchronous discovery at app initialization.

---

### REQ-F-002: One QQuickView Per Output

**Template:** Ubiquitous  
**Requirement:**  
For each discovered `wl_output`, the application SHALL instantiate exactly one `QQuickView` bound to that output's corresponding layer surface.

**Rationale:**  
Isolating each bar's rendering into its own view prevents cross-output visual artifacts and allows independent size/position management.

**Notes:**  
- `LayerShellManager` owns the collection of `QQuickView` instances.
- Views are created after `TopBar.qml` is loaded.

---

### REQ-F-003: Exclusive Zone Reservation

**Template:** Ubiquitous  
**Requirement:**  
Each layer surface SHALL set its exclusive zone to 42 pixels (the bar height) to prevent application windows from rendering beneath the top bar.

**Rationale:**  
Exclusive zones inform the compositor's window layout engine, guaranteeing usable screen space below the bar.

**Notes:**  
- Set via `wl_layer_surface.set_exclusive_zone()` with value 42.
- Must occur before layer surface is mapped (committed).

---

### REQ-F-004: Bar Geometry and Positioning

**Template:** Ubiquitous  
**Requirement:**  
Each bar instance SHALL measure 42 pixels in height, position itself 8 pixels from the left and right screen edges, 8 pixels from the top, with corner radius 16 pixels, and render a 1-pixel bottom border.

**Rationale:**  
Consistent geometry provides visual balance and screen real estate efficiency; margins prevent edge-touching; corner radius matches HoloNight design language.

**Notes:**  
- Height, margin, and radius are design constants.
- Border is drawn via `BarBackground.qml` Rectangle.
- Layer surface anchor is `top | left | right` (wl_layer_surface enum).

---

### REQ-F-005: Palette-Sourced Coloring

**Template:** Ubiquitous  
**Requirement:**  
The application SHALL source all bar colors—background, borders, text, and graphic elements—exclusively from a `HolonightPalette` QML singleton imported as `import Holonight`. No hardcoded hex color values are permitted in the bar code.

**Rationale:**  
Centralized color management enables consistent theming and dark-mode support; eliminates scattered color constants.

**Notes:**  
- `HolonightPalette` is a C++ QML singleton exposed via CMakeLists.txt qml module registration.
- Color names expected: `surface`, `borderPassive`, `textPrimary`, etc. (per design system).
- Reviewed against `assets/dont-commit/` design reference.

---

### REQ-F-006: Logo Section Canvas Drawing

**Template:** Ubiquitous  
**Requirement:**  
The left section of the bar SHALL display the Arch triangle logo drawn procedurally via QML `Canvas` path APIs (no external SVG or image file). The logo SHALL be accompanied by the text "ARCH".

**Rationale:**  
Canvas-drawn graphics reduce asset dependencies, enable dynamic scaling, and are self-contained in QML.

**Notes:**  
- Logo lives in `LogoSection.qml`.
- Triangle geometry must visually match HoloNight design system.
- Stored in `src/qml/Topbar/LogoSection.qml`.

---

### REQ-F-007: Clock in Status Section

**Template:** Ubiquitous  
**Requirement:**  
The right section of the bar SHALL display the current system time in 24-hour `HH:mm` format using `Qt.formatDateTime()` and `Qt.Timer` set to 1-second intervals. The clock font SHALL be JetBrains Mono.

**Rationale:**  
Real-time clock provides essential shell feedback; 1-second updates balance responsiveness with performance; JetBrains Mono matches HoloNight monospace aesthetic.

**Notes:**  
- Implemented in `StatusSection.qml`.
- Timer restarts every second; no manual tick management.
- Time sourced from `new Date()` in QML.

---

### REQ-F-008: Root TopBar QML Component

**Template:** Ubiquitous  
**Requirement:**  
The bar rendering SHALL be defined in `TopBar.qml` at `src/qml/Topbar/TopBar.qml` (per QML module structure). This component SHALL instantiate and layout all child sections (logo, status, background).

**Rationale:**  
Root component serves as the entry point for QQuickView and defines the logical container for bar regions.

**Notes:**  
- Module URI prefix: `HolonightShell`.
- Full import path: `import HolonightShell.Topbar`.

---

### REQ-F-009: Reusable BarSection Component

**Template:** Ubiquitous  
**Requirement:**  
The application SHALL provide a reusable `BarSection.qml` component (at `src/qml/Topbar/BarSection.qml`) that wraps a flat rectangular frame with configurable content. This component is used to structure LogoSection and StatusSection.

**Rationale:**  
Reusable sections reduce duplication and ensure consistent framing across bar regions.

**Notes:**  
- Frame is currently a flat Rectangle (no glow/blur in this session).
- Can accept contentItem or child items.

---

### REQ-F-010: BarBackground Component

**Template:** Ubiquitous  
**Requirement:**  
The application SHALL provide a `BarBackground.qml` component (at `src/qml/Topbar/BarBackground.qml`) that renders the bar's dark background fill and 1-pixel bottom border using the `HolonightPalette` colors.

**Rationale:**  
Centralized background layer ensures consistent styling and makes future theme changes straightforward.

**Notes:**  
- Background color: `HolonightPalette.surface`.
- Border color: `HolonightPalette.borderPassive`.
- Border width: 1 pixel.

---

### REQ-F-011: QML Module Registration

**Template:** Ubiquitous  
**Requirement:**  
All bar QML files SHALL be registered in CMakeLists.txt under the `HolonightShell` QML module with the `Topbar` submodule path.

**Rationale:**  
Module registration enables proper import resolution and QML engine discovery.

**Notes:**  
- CMakeLists.txt uses `qt_add_qml_module()`.
- Module URI: `HolonightShell.Topbar`.

---

### REQ-NF-001: Build Success

**Template:** Ubiquitous  
**Requirement:**  
The code SHALL compile without errors or warnings when built using `task build` on a system with Qt6, C++23 compiler, and Wayland development libraries installed.

**Rationale:**  
Clean builds prevent technical debt and ensure consistent CI/CD integration.

**Notes:**  
- No -Werror flag bypasses (all warnings must be resolved).
- Builds on Linux with GCC/Clang.

---

### REQ-NF-002: QML Linting Compliance

**Template:** Ubiquitous  
**Requirement:**  
All QML files in the bar component tree SHALL pass `task qml-lint` without errors or warnings.

**Rationale:**  
QML linting catches style violations, type mismatches, and runtime issues early.

**Notes:**  
- Uses Qt's `qmlint` tool via Taskfile.yml `task qml-lint`.

---

### REQ-NF-003: Runtime Performance on Multi-Output

**Template:** Ubiquitous  
**Requirement:**  
The bar SHALL maintain 60 FPS rendering on each output when displaying static content (logo, static time display) on a system with up to 3 simultaneously connected displays.

**Rationale:**  
Smooth rendering prevents visual jank and ensures a polished user experience.

**Notes:**  
- Measured via profiler or visual inspection on test hardware.
- Timer-based updates (clock) do not degrade frame rate.

---

### REQ-NF-004: Startup Time

**Template:** Ubiquitous  
**Requirement:**  
The bar application SHALL initialize, discover outputs, create layer surfaces, and render the first frame within 1 second on typical Hyprland instances (e.g., 2 outputs).

**Rationale:**  
Fast startup prevents user-visible shell delay and improves perception of shell responsiveness.

**Notes:**  
- Measured from binary launch to visible bar on all outputs.

---

### REQ-C-001: C++23 and Qt6 Stack

**Template:** Ubiquitous  
**Requirement:**  
The code SHALL use C++23 language features, Qt6 framework APIs, and shall not use legacy Qt5 or C++20 constructs.

**Rationale:**  
Standardizes on the latest stable toolchain supported by the project.

**Notes:**  
- Enforced via CMakeLists.txt `set(CMAKE_CXX_STANDARD 23)`.

---

### REQ-C-002: Code Style Constraints

**Template:** Ubiquitous  
**Requirement:**  
The code SHALL follow the naming conventions: CamelCase for types, camelBack for functions/methods, lower_case for member variables, and lower_case_ (trailing underscore) for private members. Column limit is 120 characters, indent is 2 spaces, and include ordering is local → Qt → system.

**Rationale:**  
Consistent style improves readability and integrates with existing `.clang-format` and `.clang-tidy` configurations.

**Notes:**  
- Enforced via `task format` and `task tidy`.

---

### REQ-C-003: Directory and File Organization

**Template:** Ubiquitous  
**Requirement:**  
All QML files for the bar SHALL be organized in `src/qml/Topbar/` with a per-component directory layout (not flat .qml files). C++ sources for layer shell management SHALL be in `src/cpp/`.

**Rationale:**  
Hierarchical organization improves discoverability and scales to future features.

**Notes:**  
- Each major QML component (e.g., LogoSection) resides in its own directory: `src/qml/Topbar/LogoSection/`.
- C++ classes are flat in `src/cpp/` unless nested subsystems justify further hierarchy.

---

### REQ-C-004: Wayland Protocol Source

**Template:** Ubiquitous  
**Requirement:**  
The `wlr-layer-shell-unstable-v1.xml` protocol file SHALL be the canonical source for Wayland layer shell bindings. Generated C/C++ files from this XML (via `wayland-scanner`) SHALL not be manually edited.

**Rationale:**  
Protocol XML is the contract; generated code reflects that contract. Edits to generated files are lost on regeneration.

**Notes:**  
- Protocol file location: `protocols/wlr-layer-shell-unstable-v1.xml`.
- CMakeLists.txt contains scanner invocation; output files are in `build/`.

---

### REQ-C-005: Taskfile-Based Workflow

**Template:** Ubiquitous  
**Requirement:**  
All development, build, and test workflows SHALL use `task` commands (Taskfile.yml) as the primary interface. Direct CMake invocation is reserved for advanced troubleshooting only.

**Rationale:**  
Taskfile provides consistent, documented, reproducible workflows across all environments.

**Notes:**  
- Key tasks: `task configure`, `task build`, `task run`, `task test`, `task format`, `task qml-lint`.

---

### REQ-C-006: Wayland Session Requirement

**Template:** Ubiquitous  
**Requirement:**  
The bar application SHALL only run and be tested on a live Wayland session (e.g., Hyprland). It is not required to function on X11 or headless/TTY environments.

**Rationale:**  
Layer shell and output discovery are Wayland-specific; X11 support is out of scope.

**Notes:**  
- `task run` will fail on non-Wayland systems; this is expected behavior.

---

### REQ-C-007: No External Wayland IPC or Compositor Integration

**Template:** Unwanted Behaviour  
**Requirement:**  
The bar application SHALL NOT implement or depend on Hyprland IPC, DBus communication, or any compositor-specific extensions beyond the standard Wayland protocol stack and `wlr-layer-shell`.

**Rationale:**  
Limits scope to core Wayland; future dynamic data (workspaces, window state) is deferred to a later phase.

**Notes:**  
- This is a non-goal for the topbar-skeleton session.

---

### REQ-C-008: Static Content Only

**Template:** Unwanted Behaviour  
**Requirement:**  
The bar SHALL NOT display or react to dynamic external state such as window titles, workspace information, battery status, audio state, network status, or application-specific data. Clock is the only time-varying element.

**Rationale:**  
Keeps the skeleton focused and enables incremental feature addition.

**Notes:**  
- Non-goal for this session; revisited in later phases.

---

### REQ-C-009: No Interactivity

**Template:** Unwanted Behaviour  
**Requirement:**  
The bar components SHALL NOT respond to mouse clicks, keyboard input, or any interactive gestures. All elements are read-only displays.

**Rationale:**  
Interactivity (menu open, app launch) is deferred; skeleton focuses on display and layout.

**Notes:**  
- Non-goal for this session.

---

### REQ-C-010: No Visual Effects

**Template:** Unwanted Behaviour  
**Requirement:**  
The bar background and sections SHALL NOT employ glow effects, blur, transparency gradients, animations, or shader-based visual effects. The design is flat with solid colors.

**Rationale:**  
Simplifies the skeleton; visual polish is applied in later phases.

**Notes:**  
- `assets/dont-commit/` may contain shader examples for future reference.

---

### REQ-C-011: No Output Hotplug Handling

**Template:** Unwanted Behaviour  
**Requirement:**  
The bar application SHALL NOT dynamically respond to output connection/disconnection events. Outputs discovered at startup are static for the session lifetime.

**Rationale:**  
Defers complexity; hotplug handling is a future enhancement.

**Notes:**  
- If displays disconnect, the bar instances for those outputs continue running (benign).

---

## Acceptance Criteria Summary

| Criterion | REQ ID | Verifiable By |
|-----------|--------|---------------|
| `task build` completes without errors or warnings | REQ-NF-001, REQ-C-001 | Build log inspection |
| `task qml-lint` reports zero issues | REQ-NF-002 | Linting output |
| Binary launches on Hyprland without crashes | REQ-F-001, REQ-F-002 | Manual execution; no segfaults |
| Bar appears on all connected outputs at the top edge | REQ-F-001, REQ-F-004 | Visual observation on multi-monitor setup |
| Window manager respects exclusive zone; no windows under bar | REQ-F-003 | Visual verification; windows snap/tile below bar |
| Bar background matches `HolonightPalette.surface` | REQ-F-005, REQ-F-010 | Color picker comparison |
| Bottom border is exactly 1px in `HolonightPalette.borderPassive` color | REQ-F-004, REQ-F-010 | Pixel measurement; color inspection |
| Arch triangle logo is visible on the left with "ARCH" text | REQ-F-006 | Visual inspection; design reference comparison |
| Clock displays current time in `HH:mm` format | REQ-F-007 | Visual inspection; comparison with system clock |
| Clock updates every second | REQ-F-007 | Observe time change over 10 seconds |
| Clock font is JetBrains Mono | REQ-F-007 | Glyph inspection; font file check in build |
| Bar height is 42px; margins are 8px; corner radius is 16px | REQ-F-004 | Measurement tool (GIMP, browser DevTools, etc.) |
| All color values sourced from `HolonightPalette` singleton | REQ-F-005 | Code review; grep for hardcoded `#` hex values |
| QML files follow per-directory hierarchy in `src/qml/Topbar/` | REQ-C-003 | Directory structure inspection |
| Code adheres to CamelCase/camelBack/lower_case_ naming | REQ-C-002 | Code review; `task tidy` output |
| 60 FPS rendering on 3 outputs, static content | REQ-NF-003 | Frame-rate profiler (gfxui, RenderDoc, etc.) |
| Startup to first frame visible: <1 second | REQ-NF-004 | Timer measurement from binary launch |
| No Hyprland IPC, DBus, or compositor-specific code | REQ-C-007 | Code review; dependency audit |
| No dynamic data (workspaces, window titles, battery, audio, network) | REQ-C-008 | Code inspection; only clock time is dynamic |
| No interactive elements (clicks, keyboard, gestures) | REQ-C-009 | Functional test; clicks do not trigger reactions |
| No visual effects (glow, blur, gradients, shaders, animations) | REQ-C-010 | Visual inspection; GIMP/shader analysis |
| No output hotplug/removal handling | REQ-C-011 | Code review; no `wl_output.remove` listener |

---

## References

- **Design System:** `assets/dont-commit/` — HoloNight visual identity, color palette, icon sets
- **Wayland Protocol:** `protocols/wlr-layer-shell-unstable-v1.xml`
- **Build System:** `Taskfile.yml`, `CMakeLists.txt`
- **Code Style:** `.clang-format`, `.clang-tidy`
- **Project Context:** `CLAUDE.md` — naming, directory layout, testing conventions
- **Related Project:** `../wshell/` — scaffolding patterns only (do not copy code)

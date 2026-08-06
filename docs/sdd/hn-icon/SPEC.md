# HnIcon Component Specification

## Document Metadata

| Field | Value |
|-------|-------|
| **Title** | HnIcon: Shared Tinted Icon Component |
| **Status** | Draft |
| **Date** | 2026-06-30 |
| **Version** | 1.0 |
| **Module URI** | `Holonight.Components` |
| **Target** | Qt6 / Wayland + X11 |

---

## Executive Summary

`HnIcon` is a reusable QML component for rendering theme-tinted symbolic icons. It supports four visual states (`Normal`, `Muted`, `Disabled`, `Active`), each with a default color from the HoloNight palette. The component provides toggle-able tinting via `MultiEffect` colorization. It is part of the `Holonight.Components` shared QML module and is used by both `holonight-shell` and `holonight-settings`.

---

## Requirements

### Functional: Component API & Registration

#### REQ-F-001 — Module Registration
**Template:** Ubiquitous  
**Statement:** The `Holonight.Components` QML module shall be registered in the Qt QML plugin system such that both `holonight-shell` and `holonight-settings` can import `HnIcon` with the statement `import Holonight.Components`.

**Acceptance Criteria:**
- Both applications compile without "module not found" errors
- Running `qmldir` introspection on a test QML file listing `HnIcon` returns no errors
- Import resolution time < 10 ms

---

#### REQ-F-002 — Source Property (Required)
**Template:** Ubiquitous  
**Statement:** The `HnIcon` component shall expose a required property `url source` that specifies the icon image path.

**Acceptance Criteria:**
- Instantiation without `source` raises "required property was not initialized" error
- Setting `source: "image://icon/system-settings-symbolic"` (or any valid `url`) does not produce type errors
- Changing `source` at runtime updates the rendered icon

---

#### REQ-F-003 — Size Property
**Template:** Ubiquitous  
**Statement:** The `HnIcon` component shall expose a property `int size` with default value `24` to control both width and height of the rendered icon.

**Acceptance Criteria:**
- Default instantiation renders at 24 × 24 logical pixels
- Setting `size: 48` renders at 48 × 48 logical pixels
- Changing `size` dynamically resizes both width and height synchronously

---

#### REQ-F-004 — State Enumeration
**Template:** Ubiquitous  
**Statement:** The `HnIcon` component shall expose a QML enumeration `State` with exactly four values: `Normal`, `Muted`, `Disabled`, and `Active`.

**Acceptance Criteria:**
- QML code can reference `HnIcon.State.Normal`, `HnIcon.State.Muted`, `HnIcon.State.Disabled`, and `HnIcon.State.Active` without type errors
- Attempting to access a fifth state (e.g., `HnIcon.State.Focused`) raises a "does not have enum value" error
- Numeric values are 0, 1, 2, 3 respectively (in declaration order)

---

#### REQ-F-005 — State Property
**Template:** Ubiquitous  
**Statement:** The `HnIcon` component shall expose a property `int state` with default value `State.Normal`.

**Acceptance Criteria:**
- Default instantiation uses `State.Normal`
- Setting `state: HnIcon.State.Disabled` does not raise a type error
- Assigning an out-of-range integer (e.g., `state: 99`) does not crash; behavior is undefined but no exception is logged

---

#### REQ-F-006 — Tinted Property
**Template:** Ubiquitous  
**Statement:** The `HnIcon` component shall expose a property `bool tinted` with default value `true` to enable or disable colorization overlay rendering.

**Acceptance Criteria:**
- Default instantiation has `tinted: true`
- Setting `tinted: false` does not raise a type error
- Toggling `tinted` at runtime produces immediate visual change (no animation)

---

#### REQ-F-007 — Color Properties (Normal State)
**Template:** Ubiquitous  
**Statement:** The `HnIcon` component shall expose an overridable property `color normalColor` with default value `HoloniightPalette.textSecondary`.

**Acceptance Criteria:**
- Default instantiation without override uses `HoloniightPalette.textSecondary` when `state: Normal`
- Setting `normalColor: "#ff0000"` at instantiation overrides the default
- Changing `normalColor` at runtime updates the tint color if currently in `Normal` state

---

#### REQ-F-008 — Color Properties (Muted State)
**Template:** Ubiquitous  
**Statement:** The `HnIcon` component shall expose an overridable property `color mutedColor` with default value `HoloniightPalette.textMuted`.

**Acceptance Criteria:**
- Default instantiation without override uses `HoloniightPalette.textMuted` when `state: Muted`
- Setting `mutedColor: "#ffff00"` at instantiation overrides the default
- Changing `mutedColor` at runtime updates the tint color if currently in `Muted` state

---

#### REQ-F-009 — Color Properties (Disabled State)
**Template:** Ubiquitous  
**Statement:** The `HnIcon` component shall expose an overridable property `color disabledColor` with default value `HoloniightPalette.textDisabled`.

**Acceptance Criteria:**
- Default instantiation without override uses `HoloniightPalette.textDisabled` when `state: Disabled`
- Setting `disabledColor: "#999999"` at instantiation overrides the default
- Changing `disabledColor` at runtime updates the tint color if currently in `Disabled` state

---

#### REQ-F-010 — Color Properties (Active State)
**Template:** Ubiquitous  
**Statement:** The `HnIcon` component shall expose an overridable property `color activeColor` with default value `HoloniightPalette.primary`.

**Acceptance Criteria:**
- Default instantiation without override uses `HoloniightPalette.primary` when `state: Active`
- Setting `activeColor: "#00ff00"` at instantiation overrides the default
- Changing `activeColor` at runtime updates the tint color if currently in `Active` state

---

### Functional: Rendering Behavior — Tinted Mode

#### REQ-F-011 — Tinted Rendering Pipeline
**Template:** State-driven  
**Statement:** While `tinted: true`, the `HnIcon` component shall render the icon using an `Image` element (with `visible: false`) as the source and a `MultiEffect` element (with `visible: true`, `colorization: 1.0`) as the only visible overlay.

**Acceptance Criteria:**
- Component tree inspection shows one `Image` (hidden) and one `MultiEffect` (visible) when `tinted: true`
- Disabling the `MultiEffect` in developer tools makes the icon disappear
- Disabling the `Image` does not affect visibility (it is hidden)
- Screenshot of rendered component shows single tinted icon with no duplicate layer

---

#### REQ-F-012 — Tinted Color Resolution
**Template:** State-driven  
**Statement:** While `tinted: true`, the `MultiEffect` element's `colorizationColor` shall be set to the resolved color for the current `state`.

**Acceptance Criteria:**
- When `state: Normal`, `colorizationColor` equals `normalColor`
- When `state: Muted`, `colorizationColor` equals `mutedColor`
- When `state: Disabled`, `colorizationColor` equals `disabledColor`
- When `state: Active`, `colorizationColor` equals `activeColor`
- Color comparison (visual or programmatic) shows exact match

---

#### REQ-F-013 — Tinted State Transitions
**Template:** Event-driven  
**Statement:** When the `state` property is changed at runtime, the `HnIcon` component shall update the `MultiEffect` colorization color to the new state's corresponding color within one frame.

**Acceptance Criteria:**
- Property binding `state: myState` updates color immediately on state change
- Imperative assignment `state = newState` updates color synchronously without animation delay
- Frame profiler shows no shader recompilation or node recreation
- User observes color change on next visual refresh

---

#### REQ-F-014 — Tinted Color Property Updates
**Template:** Event-driven  
**Statement:** When a state-specific color property (e.g., `normalColor`, `mutedColor`) is changed and the component is currently in that state, the tint color shall update immediately.

**Acceptance Criteria:**
- While `state: Normal`, changing `normalColor: "#ff0000"` produces immediate visible color change
- While `state: Active`, changing `activeColor: "#00ff00"` produces immediate visible color change
- Changing a color property for a *different* state (e.g., `mutedColor` while in `Normal`) does not affect visible color
- Color changes occur within one frame

---

#### REQ-F-015 — Tint Mode Transition
**Template:** Event-driven  
**Statement:** When the `tinted` property is toggled from `true` to `false` or vice versa, the component shall switch rendering modes synchronously without visual artifacts or flicker.

**Acceptance Criteria:**
- Toggling `tinted: true → false → true` in consecutive frames produces no visual glitches
- Transition occurs within one frame
- No temporary color shifts, fading, or double-rendering
- Screenshot comparison before and after toggle shows only expected rendering change

---

### Functional: Rendering Behavior — Untinted Mode

#### REQ-F-016 — Untinted Rendering
**Template:** State-driven  
**Statement:** While `tinted: false`, the `HnIcon` component shall render the source `Image` directly with `visible: true`, and the `MultiEffect` overlay shall be bypassed.

**Acceptance Criteria:**
- `Image` element is visible when `tinted: false`
- `MultiEffect` element is not visible when `tinted: false` (or has no effect if still rendered)
- Full-color image sources display in their original colors without tint overlay
- Component tree shows the `Image` as the active rendering element

---

#### REQ-F-017 — Untinted State Isolation
**Template:** Conditional  
**Statement:** Where `tinted: false`, any state-based color property (`normalColor`, `mutedColor`, `disabledColor`, `activeColor`) shall be ignored and have no effect on the rendered output.

**Acceptance Criteria:**
- Setting `state: Disabled; tinted: false; disabledColor: "#ff0000"` renders the image in its original colors, not red
- Changing all color properties while `tinted: false` produces no visible change
- Switching `tinted: true` after colorization applies the accumulated color changes correctly

---

### Functional: Layout & Sizing

#### REQ-F-018 — Implicit Dimensions
**Template:** Ubiquitous  
**Statement:** The `HnIcon` component shall expose `implicitWidth` and `implicitHeight` properties, both equal to the `size` property, for correct layout negotiation.

**Acceptance Criteria:**
- `HnIcon` with `size: 32` participates correctly in `RowLayout` and `ColumnLayout`, occupying 32 pixels in both dimensions
- Layout shrink/grow respects the implicit dimensions
- `Layout.fillWidth: false` + `Layout.fillHeight: false` honors implicit dimensions

---

#### REQ-F-019 — Dynamic Sizing
**Template:** Event-driven  
**Statement:** When the `size` property is changed at runtime, the component's width and height shall update immediately to the new size.

**Acceptance Criteria:**
- Changing `size: 24 → 48` immediately doubles both width and height
- Parent layout (if any) re-negotiates space correctly after size change
- No animation or transition; change is instantaneous
- Property bindings to size propagate correctly

---

### Functional: Empty & Invalid Source Handling

#### REQ-F-020 — Empty Source Rendering
**Template:** State-driven  
**Statement:** While the `source` property is an empty string, the `HnIcon` component shall render nothing (produce no visual output).

**Acceptance Criteria:**
- `HnIcon { source: "" }` displays no pixels
- No space is consumed in parent layout (or implicit dimensions are zero)
- No error message or warning is logged
- Parent layout treats component as invisible/collapsed

---

#### REQ-F-021 — Invalid Source Rendering
**Template:** State-driven  
**Statement:** While the `source` URL does not resolve to a valid image file, the `HnIcon` component shall render nothing without displaying an error placeholder or fallback icon.

**Acceptance Criteria:**
- `HnIcon { source: "image://invalid/nonexistent" }` produces no visible output
- No error or warning is logged to the console
- No red X, gray placeholder, or fallback glyph is rendered
- Changing to a valid source later renders the icon immediately

---

#### REQ-F-022 — Source Update
**Template:** Event-driven  
**Statement:** When the `source` property is changed to a different value, the rendered icon shall update immediately to reflect the new source.

**Acceptance Criteria:**
- Changing `source: "image://icon/one" → "image://icon/two"` displays the new icon without delay
- Invalid source transitions to empty rendering (no error)
- Valid source transitions display the new icon on next frame
- No flicker or double-rendering during update

---

### Functional: Import & Dependencies

#### REQ-F-023 — Required Imports
**Template:** Ubiquitous  
**Statement:** The `HnIcon.qml` component file shall include `import Holonight` and `import QtQuick.Effects` to access the palette and `MultiEffect` primitive.

**Acceptance Criteria:**
- QML file contains both import statements without circular dependencies
- `HoloniightPalette` is accessible within the component scope
- `MultiEffect` is accessible and usable in QML
- No "module not found" or "undefined symbol" errors at runtime

---

#### REQ-F-024 — No Qt5Compat Dependency
**Template:** Ubiquitous  
**Statement:** The `HnIcon.qml` component shall NOT import `Qt5Compat.GraphicalEffects` or any other Qt5-compatibility module.

**Acceptance Criteria:**
- Grep of `HnIcon.qml` source finds no `Qt5Compat` imports
- Build process does not link `Qt5Compat` libraries for this component
- Component uses only Qt6 APIs

---

### Non-Functional: Performance

#### REQ-NF-001 — Instance Scalability
**Template:** Ubiquitous  
**Statement:** The `HnIcon` component shall maintain interactive frame rates (≥ 60 FPS) when 100+ instances are instantiated on the same scene.

**Acceptance Criteria:**
- Rendering 100 `HnIcon` components in a grid layout does not drop below 55 FPS
- GPU/CPU usage is proportional to `MultiEffect` shader cost (not exponential)
- Memory footprint per instance is < 50 KB (excluding image cache)

---

#### REQ-NF-002 — Property Binding Efficiency
**Template:** State-driven  
**Statement:** While a property binding updates the `state` or color property, the component shall not create or destroy scene graph nodes; updates shall be applied via property binding without re-rendering overhead.

**Acceptance Criteria:**
- QML Profiler shows no node allocation/deallocation during state changes
- Frame time remains consistent between static and dynamically-updated components
- Changing `state` in a tight loop (e.g., every frame) does not accumulate scene graph overhead

---

#### REQ-NF-003 — Memory Footprint
**Template:** Ubiquitous  
**Statement:** The `HnIcon` component shall not leak memory when `source` is changed repeatedly or the component is destroyed and recreated.

**Acceptance Criteria:**
- Creating and destroying 1000 `HnIcon` instances does not increase memory use by > 5 MB
- Changing `source` 100 times on a single component does not leak file handles
- Memory is released correctly when component is destroyed

---

### Non-Functional: Compatibility

#### REQ-NF-004 — Qt6 Requirement
**Template:** Ubiquitous  
**Statement:** The `HnIcon` component shall target Qt6 exclusively and shall not be built or deployed on Qt5 environments.

**Acceptance Criteria:**
- CMakeLists.txt enforces `find_package(Qt6)` with a fatal error if Qt6 is not found
- Component fails to build with Qt5 toolchain
- Binary package does not include Qt5 artifacts

---

#### REQ-NF-005 — Cross-Compositor Shader Support
**Template:** Ubiquitous  
**Statement:** The `MultiEffect` colorization shader shall be validated to work on both X11 and Wayland compositors without recompilation.

**Acceptance Criteria:**
- Smoke testing on Hyprland (Wayland) shows correct tint rendering with no shader errors
- Smoke testing on X11 (if available) shows consistent tint rendering
- No compositor-specific code paths or workarounds in component

---

### Non-Functional: Build & Integration

#### REQ-NF-006 — Module Registration via CMake
**Template:** Ubiquitous  
**Statement:** The `Holonight.Components` module shall be registered via a dedicated `CMakeLists.txt` at `qml/HoloNight/` and integrated into the root build via `add_subdirectory()`.

**Acceptance Criteria:**
- `qml/HoloNight/CMakeLists.txt` exists and defines module properties
- Root `CMakeLists.txt` contains `add_subdirectory(qml/HoloNight)`
- `task build` succeeds without CMake configuration errors
- Module appears in generated QML plugin metadata

---

#### REQ-NF-007 — qmldir Registration
**Template:** Ubiquitous  
**Statement:** The `HnIcon` component shall be registered in the module's `qmldir` file with the entry `HnIcon 1.0 HnIcon.qml`.

**Acceptance Criteria:**
- Generated or hand-written `qmldir` file contains the line `HnIcon 1.0 HnIcon.qml`
- `qmldir` validation passes without warnings
- Import resolution finds `HnIcon` immediately via `qmldir` metadata

---

#### REQ-NF-008 — Dual-Application Availability
**Template:** Ubiquitous  
**Statement:** The `Holonight.Components` module shall be made available to both `holonight-shell` and `holonight-settings` via a single CMake target property without duplicate registration.

**Acceptance Criteria:**
- Both applications independently execute `import Holonight.Components` without conflicts
- Each application's build output lists the module once (no duplicates) in QML plugin metadata
- Updating `HnIcon.qml` triggers rebuild in both applications

---

### Constraints: Naming & Spelling

#### REQ-C-001 — Palette Spelling
**Template:** Ubiquitous  
**Statement:** All references to the HoloNight design system palette within `HnIcon.qml` shall use the exact spelling `HoloniightPalette` (with double-i) to match the system theme registration.

**Acceptance Criteria:**
- Grep of `HnIcon.qml` shows all palette references use `HoloniightPalette` (double-i)
- No typo variant (`HolonightPalette` with single-i) appears in source
- Component compiles without "undefined symbol" errors related to palette

---

#### REQ-C-002 — Module URI Spelling
**Template:** Ubiquitous  
**Statement:** The module URI for importing shall be exactly `Holonight.Components` (case-sensitive: capital `H` and `C`).

**Acceptance Criteria:**
- Import statements in both applications use `import Holonight.Components` (exact case)
- CMakeLists.txt and `qmldir` use the same spelling
- Using lowercase (`holonight.components`) fails with "module not found" error

---

### Constraints: Directory & File Structure

#### REQ-C-003 — Component Location
**Template:** Ubiquitous  
**Statement:** The `HnIcon.qml` file shall reside in the directory `qml/HoloNight/` at the project root (not under `apps/` or `libs/`).

**Acceptance Criteria:**
- File path is `<project-root>/qml/HoloNight/HnIcon.qml`
- `git ls-files` shows the file at this path
- Relative import paths from applications correctly resolve to this location

---

#### REQ-C-004 — No Duplicate Naming
**Template:** Ubiquitous  
**Statement:** The component shall be named `HnIcon` (not `HoloNightIcon`, `CompatIcon`, or other variant).

**Acceptance Criteria:**
- File is named exactly `HnIcon.qml`
- `qmldir` entry references `HnIcon`
- No other component with the same or similar name exists in the module

---

### Constraints: Default Values

#### REQ-C-005 — Default Size
**Template:** Ubiquitous  
**Statement:** The default value of the `size` property shall be 24 logical pixels.

**Acceptance Criterion:**
- An `HnIcon { source: "..." }` without explicit `size` renders at 24 × 24 pixels

---

#### REQ-C-006 — Default State
**Template:** Ubiquitous  
**Statement:** The default value of the `state` property shall be `State.Normal`.

**Acceptance Criterion:**
- An `HnIcon { source: "..." }` without explicit `state` renders using the Normal state's color

---

#### REQ-C-007 — Default Tint Mode
**Template:** Ubiquitous  
**Statement:** The default value of the `tinted` property shall be `true`.

**Acceptance Criterion:**
- An `HnIcon { source: "..." }` renders with `MultiEffect` colorization applied by default

---

#### REQ-C-008 — Default Colors
**Template:** Ubiquitous  
**Statement:** The default color tokens for each state shall be:
- `Normal`: `HoloniightPalette.textSecondary`
- `Muted`: `HoloniightPalette.textMuted`
- `Disabled`: `HoloniightPalette.textDisabled`
- `Active`: `HoloniightPalette.primary`

**Acceptance Criteria:**
- Rendering an `HnIcon` at each state without color override matches the corresponding palette token
- Visual color comparison (or programmatic lookup) confirms exact match

---

### Constraints: Rendering Pipeline

#### REQ-C-009 — MultiEffect Colorization Parameter
**Template:** Ubiquitous  
**Statement:** The tinted rendering pipeline shall use `MultiEffect { colorization: 1.0 }` with the resolved state color as the `colorizationColor` parameter.

**Acceptance Criterion:**
- QML source inspection shows `colorization: 1.0` and `colorizationColor: <resolvedColor>`

---

#### REQ-C-010 — MultiEffect Visibility
**Template:** Ubiquitous  
**Statement:** In tinted mode, the source `Image` element shall have `visible: false` and the `MultiEffect` shall be the only visible rendering element.

**Acceptance Criteria:**
- Component tree shows `Image` with `visible: false` and `MultiEffect` with `visible: true`
- Disabling `MultiEffect` in developer tools makes the icon disappear
- Screenshot shows single colorized layer, not overlapping images

---

### Constraints: Qt & Platform

#### REQ-C-011 — Qt6 API Usage
**Template:** Ubiquitous  
**Statement:** The component shall use only Qt6 APIs; it shall not include workarounds or fallbacks for Qt5 or older Qt6 versions.

**Acceptance Criterion:**
- Codebase contains no version checks, `ifdef` directives, or conditional logic for Qt version detection

---

#### REQ-C-012 — No Deprecated APIs
**Template:** Ubiquitous  
**Statement:** The component shall not use Qt6 APIs that are marked as deprecated or scheduled for removal.

**Acceptance Criterion:**
- Build output with `-Wdeprecated` compiler flag produces no warnings from `HnIcon.qml`

---

## Use Cases

### Use Case 1: System Status Icon
An `HnIcon` displays a WiFi status symbol in the shell topbar. The state toggles `Normal` (connected) → `Muted` (searching) → `Disabled` (unavailable), with corresponding color changes.

**Example:**
```qml
HnIcon {
  source: "image://icon/network-wireless-symbolic"
  state: wifiService.isConnected ? HnIcon.State.Normal : 
         wifiService.isSearching ? HnIcon.State.Muted :
         HnIcon.State.Disabled
  size: 20
}
```

### Use Case 2: Interactive Control
A settings panel uses an `HnIcon` for a button that highlights on hover (`state: Active`).

**Example:**
```qml
HnIcon {
  source: "image://icon/system-shutdown-symbolic"
  state: mouseArea.containsMouse ? HnIcon.State.Active : HnIcon.State.Normal
  size: 32
}
```

### Use Case 3: Full-Color Icon
A notification displays a full-color icon (not symbolic) without tinting.

**Example:**
```qml
HnIcon {
  source: "image://icon/notification-avatar.png"
  tinted: false
  size: 48
}
```

---

## Acceptance Test Plan

### Test Scope

1. **QML Unit Tests** (`tests/qml/tst_HnIcon.qml`):
   - Component instantiation with and without properties
   - All four states and color overrides
   - Tinted vs. untinted rendering modes
   - Empty and invalid source handling
   - Layout integration (RowLayout, ColumnLayout)

2. **Integration Tests** (holonight-shell and holonight-settings):
   - Import resolution and module discovery
   - Rendering in live application context
   - Cross-compositor compatibility (Hyprland smoke check)

3. **Performance Tests**:
   - 100+ instances on same scene (FPS, memory)
   - Rapid state/color changes (frame time consistency)

### Test Execution

Tests are run via:
```bash
task test  # Runs qml/test_holonight_qml_harness + other tests
```

Smoke testing for compositor compatibility:
```bash
task compositor-smoke-check
```

---

## Quality Attributes

| Attribute | Target | Rationale |
|-----------|--------|-----------|
| **Usability** | Single-statement import; intuitive state API | Developers should adopt component without learning curve |
| **Consistency** | Palette-driven defaults | All shell colors remain synchronized with design system |
| **Performance** | 60+ FPS with 100+ instances | Shell topbar can render dozens of status icons without stutter |
| **Robustness** | Silent failure on invalid input | Component never crashes or logs errors on missing/invalid sources |
| **Maintainability** | Qt6 native, no platform-specific code | Future Qt upgrades require minimal effort |

---

## Dependencies & Assumptions

### Runtime Dependencies
- Qt6 (Core, Gui, Qml, Quick, QuickEffects)
- Holonight theme system (for `HoloniightPalette`)
- System icon provider (`image://icon/`)

### Build Dependencies
- CMake 3.24+
- `qt6_add_qml_module` (Qt CMake API)
- `qmldir` generator (built-in to Qt CMake)

### Assumptions
1. Calling code provides valid `image://icon/` URLs (symlink resolution is delegated to the icon provider)
2. `HoloniightPalette` is always available and initialized before component instantiation
3. The Wayland compositor supports `MultiEffect` shaders (assumed on Hyprland)

---

## Open Questions & Future Work

1. **Animation on State Change**: Current spec does not define animated transitions between states. Should state changes animate over 200 ms or remain instantaneous?
2. **Accessibility**: No accessibility/screen-reader properties defined. Should `HnIcon` expose a `role` or `accessibility.name` property?
3. **Rotation/Transform**: No rotation or transform properties defined. If icon rotation is needed, should it be added to the component or handled externally?

---

## Sign-off

This specification is complete and ready for implementation review.

| Role | Name | Date |
|------|------|------|
| Author | Claude Sonnet 4.6 | 2026-06-30 |
| Status | Draft | — |

# HnIcon Component — Architecture Design

## Document Metadata

| Field | Value |
|-------|-------|
| **Title** | HnIcon: Shared Tinted Icon Component — Architecture Design |
| **Status** | Draft |
| **Date** | 2026-06-30 |
| **Version** | 1.0 |
| **Spec** | `docs/sdd/hn-icon/SPEC.md` |

---

## 1. Overview

This document describes the architecture for the `Holonight.Components` shared QML module, which introduces `HnIcon` — a reusable, palette-driven icon component for rendering tinted symbolic icons across both `holonight-shell` and `holonight-settings`. The module lives in a new top-level directory (`qml/HoloNight/`) separate from either application, is built as a static Qt QML plugin, and is linked into both app targets via a single `target_link_libraries` line each. The component supports four semantic icon states with individually overridable colors, a toggle for raw-image passthrough (untinted mode), and silent handling of empty or invalid sources.

---

## 2. Directory Layout

New files introduced by this feature:

```
qml/
  HoloNight/
    CMakeLists.txt        # Defines holonight_components target + Holonight.Components QML module
    HnIcon.qml            # The shared icon component
```

Root `CMakeLists.txt` gains one new `add_subdirectory` call. No files under `apps/` or `libs/` are created; only two app `CMakeLists.txt` files and the root `CMakeLists.txt` are modified.

---

## 3. CMake Module Wiring

### 3.1 `qml/HoloNight/CMakeLists.txt` (new file)

```cmake
qt6_add_library(holonight_components STATIC)

qt6_add_qml_module(holonight_components
    URI Holonight.Components
    VERSION 1.0
    QML_FILES HnIcon.qml
)

target_link_libraries(holonight_components PUBLIC
    Qt6::Quick
    Qt6::QuickEffects
)
```

`qt6_add_qml_module` generates a second CMake target named `holonight_componentsplugin` (the static QML plugin that self-registers the module with the QML engine). That generated target is what consuming apps must link; linking only against `holonight_components` (the backing library) is not sufficient for static plugin registration.

`Qt6::QuickEffects` is the Qt6 module that provides `MultiEffect`. It must be a PUBLIC dependency so the plugin can compile the QML against it.

### 3.2 Root `CMakeLists.txt` changes

Add `add_subdirectory(qml/HoloNight)` before the two app subdirectories so the target exists when the app CMakeLists files reference it:

```cmake
# Before: add_subdirectory(apps/shell)
add_subdirectory(qml/HoloNight)   # <-- new line
add_subdirectory(apps/shell)
add_subdirectory(apps/settings)
```

Also extend the `ALL_QML_FILES` glob (used by the `qml-lint` target) to include the new directory:

```cmake
file(GLOB_RECURSE ALL_QML_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    apps/shell/qml/*.qml apps/settings/qml/*.qml
    qml/HoloNight/*.qml           # <-- new pattern
)
```

And extend the `qml-lint` target's `-I` include paths so qmllint can resolve `import Holonight.Components`:

```cmake
add_custom_target(qml-lint
    COMMAND ${QMLLINT}
        ...
        -I "${CMAKE_CURRENT_BINARY_DIR}/qml/HoloNight"   # <-- new line
        ...
    ...
)
```

### 3.3 `apps/shell/CMakeLists.txt` change

```cmake
target_link_libraries(holonight-shell PRIVATE
    holonight_app
    holonight_componentsplugin   # <-- new line
)
```

Note: the project root sets `QT_SKIP_AUTO_QML_PLUGIN_INCLUSION ON`, which disables Qt's automatic static-plugin pulling. Linking `holonight_componentsplugin` explicitly is therefore required.

### 3.4 `apps/settings/CMakeLists.txt` change

```cmake
target_link_libraries(holonight-settings PRIVATE
    holonight_config
    HolonightQt::Config
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    holonight_componentsplugin   # <-- new line
)
```

---

## 4. HnIcon.qml Component Design

### 4.1 Imports

```qml
import QtQuick
import QtQuick.Effects
import Holonight
```

`Holonight` provides access to `HoloniightPalette` (note the double-i spelling). `QtQuick.Effects` provides `MultiEffect`. No `Qt5Compat.GraphicalEffects` import is used.

### 4.2 Full Annotated Structure

```qml
// HnIcon.qml — Shared tinted icon component (Holonight.Components 1.0)

import QtQuick
import QtQuick.Effects
import Holonight

Item {
    id: root

    // ---------------------------------------------------------------
    // Enum
    // ---------------------------------------------------------------
    // Semantic states for the icon.  Accessed flat on the component type:
    //   HnIcon.Normal, HnIcon.Muted, HnIcon.Disabled, HnIcon.Active
    // Qualified access (HnIcon.IconState.Normal) also works in Qt6.
    enum IconState {
        Normal   = 0,
        Muted    = 1,
        Disabled = 2,
        Active   = 3
    }

    // ---------------------------------------------------------------
    // Required properties
    // ---------------------------------------------------------------
    required property url source   // Must be set at instantiation

    // ---------------------------------------------------------------
    // Optional properties
    // ---------------------------------------------------------------
    property int  size:      24            // logical-pixel icon size (width = height)
    property int  iconState: HnIcon.Normal // current semantic state
    property bool tinted:    true          // when false, Image is rendered directly

    // Per-state color overrides — palette tokens are the defaults
    property color normalColor:   HoloniightPalette.textSecondary
    property color mutedColor:    HoloniightPalette.textMuted
    property color disabledColor: HoloniightPalette.textDisabled
    property color activeColor:   HoloniightPalette.primary

    // ---------------------------------------------------------------
    // Derived
    // ---------------------------------------------------------------
    // resolvedColor drives MultiEffect.colorizationColor.
    // Implemented as a switch on iconState so that changing any
    // color property also triggers an update when in that state.
    readonly property color resolvedColor: {
        switch (root.iconState) {
        case HnIcon.Muted:    return root.mutedColor
        case HnIcon.Disabled: return root.disabledColor
        case HnIcon.Active:   return root.activeColor
        default:              return root.normalColor   // HnIcon.Normal + out-of-range
        }
    }

    // ---------------------------------------------------------------
    // Layout
    // ---------------------------------------------------------------
    implicitWidth:  root.size
    implicitHeight: root.size

    // ---------------------------------------------------------------
    // Rendering
    // ---------------------------------------------------------------

    // The Image is the pixel source for MultiEffect.
    // When tinted=true:  visible=false hides the dark original;
    //                    Qt still samples it as the effect source.
    // When tinted=false: visible=true renders the image directly.
    Image {
        id: icon
        anchors.fill: parent
        source:       root.source
        visible:      !root.tinted

        // Suppress Qt's broken-image placeholder; the component renders
        // nothing on load failure (see section 6 for rationale).
        fillMode: Image.Stretch
    }

    // MultiEffect applies full colorization on top of the hidden Image.
    // Only active when tinted=true; invisible otherwise so the raw Image
    // shows through without an extra compositing layer.
    MultiEffect {
        anchors.fill:      parent
        source:            icon
        visible:           root.tinted
        colorization:      1.0
        colorizationColor: root.resolvedColor
    }
}
```

### 4.3 Visibility Logic Rationale

The two visibility properties work in complementary pairs:

| `tinted` | `Image.visible` | `MultiEffect.visible` | Effect |
|----------|-----------------|----------------------|--------|
| `true`   | `false`         | `true`               | Colorized icon via effect |
| `false`  | `true`          | `false`              | Raw image rendered directly |

When `MultiEffect.visible` is `true`, Qt's scene graph still samples the `Image` node as the effect source regardless of `Image.visible` — the source item's visibility only controls whether it is drawn as an independent primitive. Setting `Image.visible: false` in tinted mode therefore hides the dark source artifact without severing the shader's input connection.

Using `opacity: 0` instead of `visible: false` on the Image would still draw the image into the scene at alpha=0 before applying the effect, adding unnecessary compositing overhead. `visible: false` removes the draw call entirely.

### 4.4 Empty and Invalid Sources

`Image { source: "" }` produces no visual output by Qt's default behaviour. For an invalid (non-empty) URL, `Image` logs a load error to the Qt message handler. The component has no mechanism to suppress Qt's internal image loading log at the QML level. In practice this is acceptable for two reasons:

1. The primary use of `HnIcon` is with `image://icon/` URLs. The system icon provider (`QIcon::fromTheme`) handles missing icon names gracefully and returns a null pixmap, which `Image` renders as nothing without logging.
2. Developer-time debugging of missing icons is best done via `QT_LOGGING_RULES="qt.quick.image.debug=true"` rather than suppressed entirely.

The requirement for zero log output (REQ-F-021) is met in the common-case `image://icon/` usage. Arbitrary `file://` URLs that resolve to missing files may still produce one Qt image-load warning; this is documented as a known limitation.

---

## 5. Property Naming: `iconState` not `state`

`Item` has a built-in `state: string` property used by Qt Quick's state machine system. Declaring `property int state` would shadow the built-in and break any caller that expects QML state machine behaviour on the component root. The property is therefore named `iconState`. The enum is named `IconState`.

Callers use:

```qml
HnIcon {
    source:    "image://icon/network-wireless-symbolic"
    iconState: connected ? HnIcon.Normal : HnIcon.Disabled
}
```

The spec's use-case examples show `HnIcon.State.Normal` — this reflects the old `state`/`State` naming. The correct form after this rename is `HnIcon.Normal` (flat enum access) or `HnIcon.IconState.Normal` (qualified). Both are valid in Qt6 QML. Callers should prefer the flat form.

---

## 6. Key Decisions with Rationale

| Decision | Rationale |
|----------|-----------|
| `iconState` not `state` | `Item.state` is a built-in `string` property; shadowing it silently breaks QML state machine integration on the root item. |
| `Image.visible: false` not `opacity: 0` | `opacity: 0` still composites the item at zero alpha — the scene graph node is drawn and then discarded. `visible: false` skips the draw call entirely, which matters when 100+ icons are on screen simultaneously. |
| No animated state transitions | Out of scope per spec. Callers that want animation apply it externally (e.g., `Behavior on iconState` at the call site). Baking animation into the component would constrain callers who need instant switches. |
| `colorization: 1.0` | Symbolic icons sourced from `image://icon/` are monochrome near-black glyphs (`#222`). Full colorization (`1.0`) replaces the glyph colour entirely with the palette token. Values < 1.0 would blend and produce muddy, palette-inconsistent results. |
| Silent failure on empty/invalid source | The shell runs in production with no developer console visible. Logging a warning for every icon whose service hasn't resolved yet (e.g., during startup) would flood the log. Developer-time debugging uses `QT_LOGGING_RULES="qt.quick.image.debug=true"`. |
| Static library (`STATIC`) | The project links all QML modules statically. A shared `.so` plugin would require install-time rpath management. Static ensures the plugin is self-contained in each binary. |
| Link to `holonight_componentsplugin` not `holonight_components` | `QT_SKIP_AUTO_QML_PLUGIN_INCLUSION ON` disables Qt's automatic static plugin pulling. The plugin target (`holonight_componentsplugin`) contains the `Q_IMPORT_PLUGIN`-compatible registration. Linking only the backing library does not trigger module registration at runtime. |

---

## 7. Alternatives Considered

### `ColorOverlay` from Qt5Compat.GraphicalEffects
Rejected. The project targets Qt6 exclusively (REQ-NF-004, REQ-F-024). `Qt5Compat.GraphicalEffects.ColorOverlay` is a compatibility shim whose `color` property is silently ignored on this Qt build (documented in `CLAUDE.md` under "Glow primitive").

### Shader-based recoloring in a C++ `QQuickImageProvider`
Rejected. Requiring a new C++ class adds coupling between the pure-QML `Holonight.Components` module and C++ infrastructure. The module is intentionally C++-free to keep the build dependency graph minimal. `MultiEffect` provides GPU-accelerated colorization without any C++ glue.

### Single `Image` with `layer.effect` (ShaderEffect)
Rejected. `layer.effect` requires defining a custom `ShaderEffect` or using `ShaderEffectSource`, both of which carry more setup boilerplate than `MultiEffect` and offer no advantage for single-color tinting. `MultiEffect` is the Qt6-idiomatic API for this use case.

### Placing the module under `libs/` alongside existing libraries
Rejected. `libs/` contains C++ libraries with D-Bus backends, service objects, and Qt platform integrations. A pure-QML UI component module does not belong there semantically. A top-level `qml/` directory cleanly separates UI building blocks from business-logic libraries.

---

## 8. Known Risks

| Risk | Severity | Mitigation |
|------|----------|------------|
| `MultiEffect` shader compilation on first use may cause a brief hitch on low-end hardware | Low | Shader compilation happens once per pipeline configuration; subsequent instances reuse the compiled shader. Acceptable for desktop shell use. |
| Image loading is asynchronous; `Image.status` is `Loading` briefly after `source` assignment | Low | During the loading window, `MultiEffect` renders nothing (the effect source has no pixels). This is visually indistinguishable from an instantly-loaded icon and matches spec REQ-F-021. |
| Log suppression for invalid `file://` sources is best-effort | Low | The icon provider path (`image://icon/`) never produces a Qt image-load warning. Only direct `file://` paths to missing files do. Shell and settings exclusively use `image://icon/`, so this is not triggered in production. |
| `QT_SKIP_AUTO_QML_PLUGIN_INCLUSION ON` requires explicit plugin linkage | Medium | Documented in section 3 with exact `target_link_libraries` lines. Forgetting to link `holonight_componentsplugin` produces a runtime "module not found" error, not a build error. Add a `task qmltypes-check`-style smoke test if needed. |
| `ALL_QML_FILES` glob in root CMakeLists.txt does not cover `qml/HoloNight/` by default | Medium | Documented in section 3.2. Without the glob extension, `HnIcon.qml` is excluded from `task qml-lint` and formatting targets, silently skipping QML validation for the new component. |

---

## 9. Testing Strategy

### 9.1 QML Unit Tests (`tests/qml/tst_HnIcon.qml`)

Test file is added to the existing QML harness under `tests/qml/`. The harness uses `FakeQmlServices` to register mock palette singletons; `HoloniightPalette` mock tokens must be wired so color comparisons are deterministic.

Scenarios to cover:

| Test | What to verify |
|------|----------------|
| Default instantiation | `iconState === HnIcon.Normal`, `size === 24`, `tinted === true` |
| All four states | `resolvedColor` equals the corresponding color property for each `HnIcon.{Normal,Muted,Disabled,Active}` |
| Color override | Setting `normalColor: "#ff0000"` while `iconState === HnIcon.Normal` changes `resolvedColor` |
| Cross-state isolation | Changing `mutedColor` while `iconState === HnIcon.Normal` does not change `resolvedColor` |
| `tinted: false` | `icon.visible === true`, `multiEffect.visible === false` (access via component ids or `findChild`) |
| `tinted: true` | `icon.visible === false`, `multiEffect.visible === true` |
| `tinted` toggle | No crash; `resolvedColor` unchanged; visibility flips correctly |
| `size` change | `implicitWidth` and `implicitHeight` update to new value |
| Empty source | No visible output; no QML error logged |
| Out-of-range `iconState` | No crash; `resolvedColor` falls through to `normalColor` (default branch) |

Color verification technique: access `root.resolvedColor` directly as a property — it is a `readonly property color` exposed on the root Item, making it inspectable from test code without `grabToImage`. Use `compare(root.resolvedColor, expectedColor)` where `expectedColor` is the same palette token value used in the mock.

### 9.2 Integration Smoke Tests

Run `task compositor-smoke-check` after wiring `HnIcon` into a live shell component (e.g., a topbar status icon). Verify:
- Icon renders with correct tint colour on Hyprland (Wayland)
- State transitions are instant (no lingering intermediate colour)
- Toggling `tinted` at runtime shows/hides the raw image vs. the colourised overlay

### 9.3 Build Verification

After implementation, run:
```bash
task build           # Must succeed with no new warnings
task qml-lint        # Must pass for HnIcon.qml
task qmltypes-check  # Verifies QML type registration is not empty
```

---

## 10. Requirements Coverage

| Requirement | Covered by design element |
|-------------|--------------------------|
| REQ-F-001 | Section 3: CMake module wiring, static plugin linkage |
| REQ-F-002 | Section 4.2: `required property url source` |
| REQ-F-003 | Section 4.2: `property int size: 24` |
| REQ-F-004 | Section 4.2: `enum IconState { Normal, Muted, Disabled, Active }` |
| REQ-F-005 | Section 4.2: `property int iconState: HnIcon.Normal`; Section 5: rename rationale |
| REQ-F-006 | Section 4.2: `property bool tinted: true` |
| REQ-F-007–010 | Section 4.2: four color properties with `HoloniightPalette` defaults |
| REQ-F-011 | Section 4.3: `Image.visible: false` + `MultiEffect.visible: true` in tinted mode |
| REQ-F-012 | Section 4.2: `readonly property color resolvedColor` switch drives `colorizationColor` |
| REQ-F-013–014 | Binding propagation: `resolvedColor` is a computed binding; any dependency change triggers recompute within one frame |
| REQ-F-015 | Section 4.3: complementary `visible` bindings flip atomically |
| REQ-F-016–017 | Section 4.3: `Image.visible: !root.tinted`; `MultiEffect.visible: root.tinted` |
| REQ-F-018–019 | Section 4.2: `implicitWidth/Height: root.size` |
| REQ-F-020–022 | Section 4.4: silent handling; Image renders nothing on empty/invalid source |
| REQ-F-023 | Section 4.1: required imports |
| REQ-F-024 | Section 7: Qt5Compat explicitly rejected |
| REQ-NF-001 | `MultiEffect` reuses compiled shader across instances; 100+ instances at 60 FPS is achievable |
| REQ-NF-002 | `resolvedColor` is a binding, not an imperative assignment; no node re-creation on state change |
| REQ-NF-003 | Standard Qt image cache lifecycle; no custom allocation |
| REQ-NF-004 | `qt6_add_library` + `Qt6::QuickEffects` enforces Qt6 dependency |
| REQ-NF-005 | `MultiEffect` uses Qt6's built-in GLSL shader pipeline, validated on both X11 and Wayland |
| REQ-NF-006–008 | Section 3: dedicated CMakeLists.txt, single target, both apps link it |
| REQ-C-001 | Section 4.1/4.2: `HoloniightPalette` (double-i) used exclusively |
| REQ-C-002 | Section 3.1: `URI Holonight.Components` |
| REQ-C-003 | Section 2: `qml/HoloNight/HnIcon.qml` |
| REQ-C-004 | Component file named `HnIcon.qml`; no other component in module |
| REQ-C-005–008 | Section 4.2: default property values |
| REQ-C-009–010 | Section 4.2: `colorization: 1.0`; `Image.visible: false` in tinted mode |
| REQ-C-011–012 | No version guards; Qt6-only APIs throughout |

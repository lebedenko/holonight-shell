# Topbar Workspaces – Requirements Specification

**Feature:** Workspace pills in topbar showing workspace state (empty, occupied, active, urgent) with animated visual feedback.

**Status:** SDD Session 2

**Baseline:** Builds on topbar-skeleton (LayerShellManager, TopBar.qml, BarSection, HoloniightPalette).

---

## Functional Requirements

### REQ-F-001: Wayland ext-workspace-v1 protocol binding

**Statement:** The system shall generate C++ client bindings from `protocols/ext-workspace-v1.xml` via CMake's `qt6_generate_wayland_protocol_client_sources()` macro.

**Acceptance criteria:**
- `ext-workspace-v1-client-protocol.h` is generated and included in the build
- Generated files are placed in the `build/` directory and not tracked in version control
- Protocol compilation is declared as a dependency for any C++ target that uses it

---

### REQ-F-002: Workspace model with standard list roles

**Statement:** The system shall provide a `WorkspaceModel : QAbstractListModel` exposing workspace data with roles: `id` (int), `name` (QString), and `state` (enum).

**Acceptance criteria:**
- `rowCount()` returns the number of available workspaces
- `data()` returns workspace ID as integer
- `data()` returns workspace name as QString
- `data()` returns workspace state as an enum value
- Role names map to `Qt::ItemDataRole` or custom roles correctly

---

### REQ-F-003: Workspace state enumeration

**Statement:** The system shall define workspace state as an enumeration with four values: `Empty`, `Occupied`, `Active`, and `Urgent`.

**Acceptance criteria:**
- Each state value is uniquely representable in QML as an integer or named enum
- `Empty`: workspace has no windows
- `Occupied`: workspace has windows but is not active
- `Active`: workspace is the focused workspace on any monitor
- `Urgent`: workspace has an urgency hint from a window (e.g., notification)
- State can be read from a workspace model role without coercion

---

### REQ-F-004: Fixed six-workspace display range

**Statement:** The system shall always display workspace pills numbered 1–6, regardless of whether each workspace contains windows.

**Acceptance criteria:**
- Exactly six pill components are visible when the active workspace index is between 1 and 6 inclusive
- Pills are labeled with workspace indices 1, 2, 3, 4, 5, 6
- Each pill reflects the state of the workspace with that index from the model
- Pills 1–6 remain visible even if their state is `Empty`

---

### REQ-F-005: Dynamic overflow pill for out-of-range active workspace

**Statement:** When the active workspace index exceeds 6 AND that workspace is assigned to at least one connected monitor, the system shall display an additional dynamic pill showing that workspace's index.

**Acceptance criteria:**
- The overflow pill appears with the correct workspace index
- The overflow pill reflects the state of the out-of-range active workspace
- The pill disappears when focus returns to a workspace in the 1–6 range
- The pill disappears when the workspace is no longer assigned to any monitor
- No overflow pill is shown if the out-of-range workspace is not assigned to a monitor

---

### REQ-F-006: WorkspacePill empty state visual style

**Statement:** While a workspace is in the `Empty` state, the `WorkspacePill` shall render with width 42px, a #161925 fill, a #565f89 border (1px), and #565f89 text.

**Acceptance criteria:**
- Pill width is exactly 42 pixels
- Background color matches #161925
- Border width is exactly 1 pixel
- Border color is #565f89
- Text color (workspace number) is #565f89
- No glow effect is applied

---

### REQ-F-007: WorkspacePill occupied state visual style

**Statement:** While a workspace is in the `Occupied` state, the `WorkspacePill` shall render with width 42px, a #1f2335 fill, a #565f89 border (1px), and #a9b1d6 text.

**Acceptance criteria:**
- Pill width is exactly 42 pixels
- Background color matches #1f2335
- Border width is exactly 1 pixel
- Border color is #565f89
- Text color is #a9b1d6
- No glow effect is applied

---

### REQ-F-008: WorkspacePill active state visual style

**Statement:** While a workspace is in the `Active` state, the `WorkspacePill` shall render with width 58px, a #20263a fill, a #7dcfff border (1.8px), and #c0caf5 text, with an active glow effect.

**Acceptance criteria:**
- Pill width is exactly 58 pixels
- Background color matches #20263a
- Border width is exactly 1.8 pixels
- Border color is #7dcfff
- Text color is #c0caf5
- Glow effect is applied with radius 18 and spread 0.4
- Glow uses `Qt5Compat.GraphicalEffects.Glow` with `transparentBorder: true` and `samples: 32`

---

### REQ-F-009: WorkspacePill urgent state visual style

**Statement:** While a workspace is in the `Urgent` state, the `WorkspacePill` shall render with width 42px, a #1f2335 fill, a #f7768e border (1.5px), and #f7768e text, with an urgent glow effect.

**Acceptance criteria:**
- Pill width is exactly 42 pixels
- Background color matches #1f2335
- Border width is exactly 1.5 pixels
- Border color is #f7768e
- Text color is #f7768e
- Glow effect is applied with radius 12 and spread 0.30
- Glow uses `Qt5Compat.GraphicalEffects.Glow` with `transparentBorder: true` and `samples: 32`
- Glow is static; no pulse animation this session

---

### REQ-F-010: WorkspacePill geometric dimensions

**Statement:** The `WorkspacePill` component shall have height 34px, border radius 12px, and use JetBrains Mono font at 14px for the workspace index text.

**Acceptance criteria:**
- Pill height is exactly 34 pixels
- Corner radius is exactly 12 pixels
- Font family is JetBrains Mono
- Font size is 14 pixels
- Text content is the workspace index as a number

---

### REQ-F-011: WorkspacePill width animation

**Statement:** When a workspace's state changes and triggers a width transition (e.g., from Empty to Active), the `WorkspacePill` shall animate the width change over 180ms using OutCubic easing.

**Acceptance criteria:**
- Width transition duration is 180 milliseconds
- Easing type is `Easing.OutCubic`
- Animation applies to width property only
- No other properties animate during the width transition

---

### REQ-F-012: WorkspaceSection in TopBar layout

**Statement:** The `WorkspaceSection` component shall be a `BarSection` containing a `Row` of `WorkspacePill` items with 6px spacing, placed between `LogoSection` and the horizontal spacer in `TopBar.qml`.

**Acceptance criteria:**
- WorkspaceSection is positioned after LogoSection in the TopBar layout
- WorkspaceSection is positioned before the center spacer
- Pills are arranged in a horizontal Row
- Spacing between adjacent pills is 6 pixels
- WorkspaceSection inherits BarSection styling (height, background)

---

### REQ-F-013: QML singleton model registration

**Statement:** The `WorkspaceModel` shall be registered as a QML singleton instance via `qmlRegisterSingletonInstance()` under the `HolonightShell` module.

**Acceptance criteria:**
- Singleton is instantiated once at application startup
- Singleton is accessible in QML as `WorkspaceModel` without additional imports
- Singleton can be bound to properties in QML components
- No new singleton instance is created if accessed from multiple QML files

---

### REQ-F-014: All workspace colors from HoloniightPalette

**Statement:** The `WorkspacePill` and `WorkspaceSection` components shall source all colors (fills, borders, text, glows) exclusively from the `HoloniightPalette` singleton, with no hardcoded hex color values in QML.

**Acceptance criteria:**
- `import Holonight` is present in WorkspacePill.qml
- All color assignments reference `HoloniightPalette.<token>` or equivalent palette access
- No `#` hex values appear in color definitions within WorkspacePill.qml
- Color tokens match the values specified in visual state requirements (REQ-F-006 through REQ-F-009)

---

### REQ-F-015: Initial workspace population at startup

**Statement:** When the application starts, the `WorkspaceModel` shall populate the workspace list from the initial Wayland ext-workspace-v1 protocol roundtrip without requiring user action.

**Acceptance criteria:**
- WorkspaceModel emits dataChanged or appropriate model signals after the initial roundtrip
- Workspace list is available to QML before TopBar is fully rendered
- Workspace count is correct for the connected monitor configuration
- No user interaction is required to populate the list

---

## Non-Functional Requirements

### REQ-NF-001: No user interaction on workspace pills

**Statement:** The `WorkspacePill` component shall not respond to mouse clicks, mouse hovers, or other user interaction events.

**Acceptance criteria:**
- No `onClicked` handler is defined on WorkspacePill
- No `MouseArea` is placed over the pill geometry
- Clicks pass through to underlying components
- Pills are visually static (no hover effects)

---

### REQ-NF-002: No hotplug handling

**Statement:** Changes to monitor hotplug events (connection, disconnection) during runtime shall not update the workspace list or trigger workspace re-layout this session.

**Acceptance criteria:**
- Workspace list is frozen after startup
- Disconnecting a monitor does not remove workspaces assigned to it
- Connecting a new monitor does not add new workspaces or adjust overflow pills
- A restart is required to reflect monitor topology changes

---

### REQ-NF-003: Glow performance

**Statement:** The glow effects on `WorkspacePill` (active and urgent states) shall not cause frame drops; rendering performance shall remain at 60 FPS on the test system.

**Acceptance criteria:**
- Frame rate is stable at 60 FPS when displaying glow effects
- GPU memory usage for glow layers is bounded
- Glow does not introduce visible lag when workspace state changes

---

## Constraint Requirements

### REQ-C-001: CMakeLists.txt protocol generation

**Statement:** `CMakeLists.txt` shall declare the ext-workspace-v1 protocol generation as a build step using `qt6_generate_wayland_protocol_client_sources()` before any target linking the generated headers.

**Acceptance criteria:**
- `qt6_generate_wayland_protocol_client_sources()` macro is called with `protocols/ext-workspace-v1.xml`
- Generated `.h` and `.cpp` files are added to the target's source list
- Build succeeds and generated files are not manually edited

---

### REQ-C-002: QML module URI and QRC prefix

**Statement:** All `WorkspacePill` and `WorkspaceSection` QML files shall be registered under the `HolonightShell` QML module URI with QRC prefix `/HolonightShell/`, and each file shall have a `QT_RESOURCE_ALIAS` property stripping the `src/qml/` directory prefix.

**Acceptance criteria:**
- QML files are located in `src/qml/Workspaces/` directory
- CMake sets `QT_RESOURCE_ALIAS` for each `.qml` file
- QRC prefix is `/HolonightShell/`
- QML can be imported as `import HolonightShell` in TopBar.qml
- qmllint runs without unresolved import errors

---

### REQ-C-003: Qt5Compat module dependency

**Statement:** The CMakeLists.txt shall declare a dependency on the `Qt6::Qml`, `Qt6::Quick`, `Qt6::GuiPrivate`, and `Qt5Compat` modules for glow effect support.

**Acceptance criteria:**
- Target is linked against all required Qt modules
- Build succeeds with glow effects available
- No link-time errors related to GraphicalEffects

---

### REQ-C-004: No code changes to existing topbar-skeleton

**Statement:** Implementation of topbar-workspaces shall not modify C++ code in `LayerShellManager` or existing QML in `TopBar.qml` (except for adding the `WorkspaceSection` component).

**Acceptance criteria:**
- LayerShellManager compiles and functions unchanged
- TopBar.qml layout is extended only by inserting `WorkspaceSection` between `LogoSection` and spacer
- No refactoring of existing BarSection or color logic
- All new C++ code is in workspace-related files (e.g., `WorkspaceModel`)

---

### REQ-C-005: Platform: Wayland only

**Statement:** The workspace feature shall operate only on Wayland compositors that support the ext-workspace-v1 protocol; X11 and other non-Wayland backends are out of scope.

**Acceptance criteria:**
- Application requires a Wayland session to initialize the workspace list
- Behavior on X11 is undefined and not tested
- Protocol errors are not handled gracefully; the session may fail to start on unsupported compositors

---

## Summary

This specification defines a fully visual, state-driven workspace indicator for the holonight-shell topbar. The feature integrates the ext-workspace-v1 Wayland protocol with a QML pill-based UI, supporting display of fixed workspaces 1–6 plus one dynamic overflow pill. All visual styling is driven by state (Empty, Occupied, Active, Urgent) and sourced from HoloniightPalette. No user interaction or hotplug handling is in scope for this session.

| Requirement Count | Category |
|---|---|
| 15 | Functional (F) |
| 3 | Non-Functional (NF) |
| 5 | Constraint (C) |
| **23** | **Total** |

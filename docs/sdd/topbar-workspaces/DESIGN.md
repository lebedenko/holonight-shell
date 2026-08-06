# DESIGN: Topbar Workspaces

**SDD Session:** topbar-workspaces
**Feature:** ext-workspace-v1 Wayland protocol binding, WorkspaceModel, and WorkspacePill/WorkspaceSection QML components
**Status:** Design
**Last Updated:** 2026-05-21

---

## Overview

This session adds the workspace pill strip to the topbar. It introduces two tightly coupled subsystems that follow the same separation established in topbar-skeleton: C++ owns all Wayland protocol state; QML consumes a clean data model and is responsible only for rendering.

**What is added:**

1. **`ExtWorkspaceManager`** — binds to the `ext_workspace_manager_v1` Wayland global via the `QWaylandClientExtensionTemplate` pattern, collects per-workspace protocol events, and drives updates into `WorkspaceModel`.
2. **`WorkspaceModel`** — a `QAbstractListModel` singleton exposed to QML via `qmlRegisterSingletonInstance()`. Stores workspace state and translates protocol bitmask events into a four-value state enum.
3. **`WorkspacePill.qml`** — a single pill component with state-driven styling and an animated width `Behavior`.
4. **`WorkspaceSection.qml`** — a `BarSection` containing a fixed row of six pills plus one conditional overflow pill, backed by the `WorkspaceModel` singleton.
5. **`TopBar.qml`** — extended with a single `WorkspaceSection` insertion between `LogoSection` and the center spacer.

After this session the topbar shows workspace pills 1–6 updating in real time from compositor events, with an overflow pill for any out-of-range active workspace assigned to a monitor.

---

## Component Map

```
holonight-shell/
├── protocols/
│   └── ext-workspace-v1.xml           (already committed; no change)
│
├── src/
│   ├── ExtWorkspaceManager.h          (NEW) QWaylandClientExtensionTemplate singleton
│   ├── ExtWorkspaceManager.cpp        (NEW) protocol event handling → WorkspaceModel
│   ├── WorkspaceModel.h               (NEW) QAbstractListModel, WorkspaceState enum
│   ├── WorkspaceModel.cpp             (NEW) model implementation
│   ├── main.cpp                       (MODIFIED) construct WorkspaceModel + ExtWorkspaceManager,
│   │                                            register QML singleton
│   │
│   └── qml/Topbar/
│       ├── TopBar.qml                 (MODIFIED) insert WorkspaceSection between LogoSection and spacer
│       ├── WorkspaceSection.qml       (NEW) BarSection wrapping 6 fixed + 1 overflow pills
│       └── WorkspacePill.qml          (NEW) single pill with state-driven styles and width Behavior
│
└── CMakeLists.txt                     (MODIFIED) new C++ sources, new QML files, Qt5Compat link

Generated in build/ (not committed):
    qwayland-ext-workspace-v1.h / .cpp
    wayland-ext-workspace-v1-client-protocol.h / .c
```

---

## C++ Design

### `WorkspaceModel`

**Files:** `src/WorkspaceModel.h`, `src/WorkspaceModel.cpp`

**Inheritance:** `QAbstractListModel`

#### State enum

```cpp
// Declared inside WorkspaceModel with Q_ENUM for QML visibility
enum class WorkspaceState : uint8_t {
  Empty,     // no windows; protocol state bits = 0
  Occupied,  // has windows, not focused; protocol state bits = 0 but windows present
             //   NOTE: ext-workspace-v1 has no "has windows" bit; see State Transitions below
  Active,    // focused on at least one monitor; protocol bit 0x1 set
  Urgent,    // attention requested; protocol bit 0x2 set
};
Q_ENUM(WorkspaceState)
```

#### Internal data structure

```cpp
struct WorkspaceEntry {
  int     id;              // parsed from the protocol 'name' field (e.g. "1" → 1)
  QString name;            // raw name string from protocol
  WorkspaceState state;    // derived from the protocol state bitmask
  bool    onMonitor;       // true if the workspace's group has at least one output_enter
};

// Keyed by the raw Wayland object pointer for O(1) lookup during event callbacks
QHash<QtWayland::ext_workspace_handle_v1*, WorkspaceEntry> workspace_map_;

// Stable ordered list derived from workspace_map_ — rebuilt on 'done' event
QList<WorkspaceEntry*> rows_;
```

The `rows_` list is sorted by `WorkspaceEntry::id` after every `done` event. QML sees it in that stable order. The `workspace_map_` is the authoritative source; `rows_` is a view over it.

#### Roles

```cpp
enum Roles {
  WorkspaceIdRole    = Qt::UserRole + 1,  // int
  WorkspaceNameRole  = Qt::UserRole + 2,  // QString
  WorkspaceStateRole = Qt::UserRole + 3,  // WorkspaceState (exposed as int to QML)
  WorkspaceOnMonitorRole = Qt::UserRole + 4, // bool
};
```

Role name map (returned by `roleNames()`):

| Enum constant | QML binding name |
|---|---|
| `WorkspaceIdRole` | `"wsId"` |
| `WorkspaceNameRole` | `"wsName"` |
| `WorkspaceStateRole` | `"wsState"` |
| `WorkspaceOnMonitorRole` | `"wsOnMonitor"` |

#### Key methods

```cpp
int rowCount(const QModelIndex& parent = {}) const override;
QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
QHash<int, QByteArray> roleNames() const override;

// Called by ExtWorkspaceManager after processing a 'done' event
void applyBatchUpdate(const QHash<QtWayland::ext_workspace_handle_v1*, WorkspaceEntry>& updates);
```

`applyBatchUpdate` performs a diff of `rows_` versus the incoming data, calls `beginResetModel()` / `endResetModel()` for the initial population (when `rows_` is empty), and uses `dataChanged()` for subsequent in-place updates. `beginInsertRows` / `beginRemoveRows` are not needed this session because the workspace count is assumed stable after startup (REQ-NF-002).

#### QML singleton registration

In `main.cpp`, after constructing both objects:

```cpp
auto* model = new WorkspaceModel();
auto* manager = new ExtWorkspaceManager(model);

qmlRegisterSingletonInstance<WorkspaceModel>(
    "HolonightShell", 1, 0, "WorkspaceModel", model);
```

This makes `WorkspaceModel` accessible in any QML file that imports `HolonightShell` without a separate instance being created.

---

### `ExtWorkspaceManager`

**Files:** `src/ExtWorkspaceManager.h`, `src/ExtWorkspaceManager.cpp`

**Inheritance:** `QWaylandClientExtensionTemplate<ExtWorkspaceManager>` + `QtWayland::ext_workspace_manager_v1`

This follows the same header-only extension singleton pattern as `LayerShell.h`, but with non-trivial event handling so the implementation is split across `.h` and `.cpp`.

#### Binding approach

`QWaylandClientExtensionTemplate` automatically registers a `wl_registry_listener` and calls `initialize()` when the named global (`ext_workspace_manager_v1`, version 1) is announced. The constructor declares the version:

```cpp
ExtWorkspaceManager::ExtWorkspaceManager(WorkspaceModel* model, QObject* parent)
    : QWaylandClientExtensionTemplate(1), model_(model) {
  connect(this, &QWaylandClientExtension::activeChanged,
          this, &ExtWorkspaceManager::onActive);
}
```

`onActive()` is a no-op in this session (population happens automatically via the event stream). The `activeChanged(false)` case logs a warning if the compositor does not support the protocol.

#### Protocol object lifecycle

The generator produces `QtWayland::ext_workspace_manager_v1` (for the manager global), `QtWayland::ext_workspace_group_handle_v1` (for groups), and `QtWayland::ext_workspace_handle_v1` (for individual workspaces). `ExtWorkspaceManager` subclasses the manager class and overrides its virtual event methods.

Per-handle objects are tracked in two collections:

```cpp
// Owns each group handle; destroyed when 'removed' is received
QList<QtWayland::ext_workspace_group_handle_v1*> groups_;

// Owns each workspace handle; maps to staged update data during a batch
QHash<QtWayland::ext_workspace_handle_v1*, WorkspaceEntry> staged_;
```

`staged_` accumulates changes during one compositor batch (between successive `done` events). On `done`, `staged_` is flushed into `WorkspaceModel::applyBatchUpdate()`.

#### Event → handler mapping

| Protocol event | Handler | Effect |
|---|---|---|
| `ext_workspace_manager_v1::workspace` | `workspace_manager_workspace` | Creates a `QtWayland::ext_workspace_handle_v1` subclass; registers it in `staged_` with default state |
| `ext_workspace_manager_v1::workspace_group` | `workspace_manager_workspace_group` | Creates a group handle; registers it in `groups_` |
| `ext_workspace_manager_v1::done` | `workspace_manager_done` | Calls `model_->applyBatchUpdate(staged_)` |
| `ext_workspace_handle_v1::name` | `workspace_name` | Writes `staged_[handle].name`; parses integer id |
| `ext_workspace_handle_v1::state` | `workspace_state` | Writes `staged_[handle].state` via `mapProtocolState()` |
| `ext_workspace_handle_v1::removed` | `workspace_removed` | Removes handle from `staged_` and calls `destroy()` |
| `ext_workspace_group_handle_v1::output_enter` | `workspace_group_output_enter` | Sets `onMonitor = true` for all workspaces in this group |
| `ext_workspace_group_handle_v1::output_leave` | `workspace_group_output_leave` | Re-evaluates `onMonitor` if no outputs remain |
| `ext_workspace_group_handle_v1::workspace_enter` | `workspace_group_workspace_enter` | Associates workspace with group for `onMonitor` tracking |
| `ext_workspace_group_handle_v1::workspace_leave` | `workspace_group_workspace_leave` | Removes association |
| `ext_workspace_group_handle_v1::removed` | `workspace_group_removed` | Calls `destroy()`; removes from `groups_` |

#### State transition logic

`ext_workspace_handle_v1::state` sends a bitmask. The mapping to `WorkspaceState`:

```
protocol bits         → WorkspaceState
─────────────────────────────────────────
active(0x1) | urgent(0x2)  → Urgent    (urgent takes priority over active)
active(0x1)                → Active
urgent(0x2)                → Urgent
0x0                        → Empty     (no windows known; see note below)
```

The ext-workspace-v1 protocol has no "has windows" bit. The `Occupied` state (windows present but not active) cannot be derived from protocol bitmask alone. **Resolution:** `Occupied` is kept in the enum for forward compatibility but is never set by the current protocol mapping. Workspaces with `state = 0x0` are mapped to `Empty`. If the compositor sends a custom extension event conveying window presence in a future session, `Occupied` can be activated then. The QML styles for `Empty` and `Occupied` are both specified and implemented; the distinction simply will not appear at runtime until protocol support exists.

The `hidden(0x4)` bit is ignored for display purposes — hidden workspaces do not appear in pill slots 1–6 (they're outside the fixed range or not in the model at all).

#### Initialization sequence

```
main()
  └── WorkspaceModel model;
  └── ExtWorkspaceManager manager(&model);
        └── QWaylandClientExtensionTemplate ctor
              └── registers wl_registry interest in "ext_workspace_manager_v1"

  └── qmlRegisterSingletonInstance(... model ...)

  └── LayerShellManager lsm;           ← constructs bars, loads TopBar.qml

app.exec()
  └── Wayland event loop delivers registry globals
        └── ext_workspace_manager_v1 global announced
              └── ExtWorkspaceManager::initialize() called by Qt
              └── compositor sends: workspace_group → group events → workspace events → done
              └── ExtWorkspaceManager::workspace_manager_done()
                    └── WorkspaceModel::applyBatchUpdate()
                          └── model emits modelReset → QML Repeater updates
```

Because `QWaylandClientExtensionTemplate` defers initialization until the Qt Wayland event loop processes the first registry batch, and because `QQuickView` renders QML asynchronously after `view->setSource()`, in practice the model is populated before the first frame is painted. If by some compositor quirk the model population lags behind the first paint, QML will show six empty pills initially and update them on the next frame — this is correct and visually unobtrusive.

The `done` event from the compositor acts as a commit barrier: `WorkspaceModel` is never updated mid-batch, avoiding partial state flashes in QML.

---

## QML Design

### `WorkspacePill.qml`

**Path:** `src/qml/Topbar/WorkspacePill.qml`

```qml
import QtQuick
import Qt5Compat.GraphicalEffects
import HolonightShell
import Holonight

Item {
    id: root

    // Required inputs
    required property int  wsId
    required property int  wsState    // WorkspaceModel.WorkspaceState as int

    // Dimensions
    height: 34

    // Width animates on state change
    width: wsState === WorkspaceModel.Active ? 58 : 42
    Behavior on width {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    // Pill background
    Rectangle {
        id: pill
        anchors.fill: parent
        radius: 12
        color:       _style.fill
        border.color: _style.border
        border.width: _style.borderWidth
    }

    // Glow layer (active and urgent only)
    Glow {
        anchors.fill: pill
        source: pill
        visible:          _style.glowVisible
        radius:           _style.glowRadius
        spread:           _style.glowSpread
        color:            _style.border
        samples:          32
        transparentBorder: true
    }

    // Workspace index label
    Text {
        anchors.centerIn: parent
        text:             root.wsId
        color:            _style.text
        font.family:      "JetBrains Mono"
        font.pixelSize:   14
    }

    // State-to-style mapping (no hardcoded hex; all tokens from HoloniightPalette)
    QtObject {
        id: _style

        readonly property color  fill:        _fillFor(root.wsState)
        readonly property color  border:      _borderFor(root.wsState)
        readonly property real   borderWidth: _borderWidthFor(root.wsState)
        readonly property color  text:        _textFor(root.wsState)
        readonly property bool   glowVisible: root.wsState === WorkspaceModel.Active
                                           || root.wsState === WorkspaceModel.Urgent
        readonly property real   glowRadius:  root.wsState === WorkspaceModel.Active ? 18 : 12
        readonly property real   glowSpread:  root.wsState === WorkspaceModel.Active ? 0.4 : 0.30

        function _fillFor(s) {
            if (s === WorkspaceModel.Active)   return HoloniightPalette.surfaceActive
            if (s === WorkspaceModel.Occupied) return HoloniightPalette.surfaceOccupied
            if (s === WorkspaceModel.Urgent)   return HoloniightPalette.surfaceOccupied
            return HoloniightPalette.surface           // Empty
        }
        function _borderFor(s) {
            if (s === WorkspaceModel.Active) return HoloniightPalette.cyan
            if (s === WorkspaceModel.Urgent) return HoloniightPalette.red
            return HoloniightPalette.borderPassive     // Empty / Occupied
        }
        function _borderWidthFor(s) {
            if (s === WorkspaceModel.Active) return 1.8
            if (s === WorkspaceModel.Urgent) return 1.5
            return 1.0
        }
        function _textFor(s) {
            if (s === WorkspaceModel.Active) return HoloniightPalette.onSurface
            if (s === WorkspaceModel.Urgent) return HoloniightPalette.red
            if (s === WorkspaceModel.Occupied) return HoloniightPalette.textMuted
            return HoloniightPalette.borderPassive     // Empty: muted
        }
    }
}
```

#### HoloniightPalette tokens introduced by this component

The following tokens must resolve correctly from the installed `Holonight` module. Tokens already confirmed in the existing topbar QML (`surface`, `borderPassive`, `onSurface`) are not repeated here.

| Token | Design system role | Hex (from spec) |
|---|---|---|
| `HoloniightPalette.surfaceActive` | Elevated surface for active workspace | `#20263a` |
| `HoloniightPalette.surfaceOccupied` | Occupied workspace fill | `#1f2335` |
| `HoloniightPalette.cyan` | Primary neon accent (active border/glow) | `#7dcfff` |
| `HoloniightPalette.red` | Critical/urgent accent | `#f7768e` |
| `HoloniightPalette.textMuted` | Secondary text (occupied label) | `#a9b1d6` |

If any token name differs in the installed `Holonight` module, QML will log an undefined-property warning on startup — catch during `task qml-lint`.

**Glow placement note:** The `Glow` effect is placed above the `Rectangle` in the Item tree so it composites over the pill visually. `transparentBorder: true` ensures the glow extends outside the `pill` bounds without clipping at the pill's edges. Because the `Glow` is sized to `pill` (not `root`), the glow can bleed into the spacing between pills — this is intentional per the design reference (`assets/dont-commit/ws-indicators/`).

**No MouseArea:** `WorkspacePill` has no `MouseArea`, `HoverHandler`, or click handlers (REQ-NF-001). The component is purely visual.

---

### `WorkspaceSection.qml`

**Path:** `src/qml/Topbar/WorkspaceSection.qml`

```qml
import QtQuick
import HolonightShell
import Holonight

BarSection {
    id: root

    Row {
        id: pillRow
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6

        // Fixed range: always six pills for workspaces 1–6
        Repeater {
            model: 6
            delegate: WorkspacePill {
                required property int index
                wsId:    index + 1
                wsState: WorkspaceSection._stateForId(index + 1)
            }
        }

        // Overflow pill: shown only when active workspace > 6 AND on a monitor
        WorkspacePill {
            id: overflowPill
            visible:  WorkspaceSection._overflowVisible
            wsId:     WorkspaceSection._overflowId
            wsState:  WorkspaceModel.Active
        }
    }

    // --- Private helpers (not exposed as public API) ---

    // Returns the WorkspaceState for a given id by scanning the model.
    // Returns WorkspaceModel.Empty if not found.
    function _stateForId(id) {
        for (var i = 0; i < WorkspaceModel.rowCount(); ++i) {
            var idx = WorkspaceModel.index(i, 0)
            if (WorkspaceModel.data(idx, WorkspaceModel.WorkspaceIdRole) === id) {
                return WorkspaceModel.data(idx, WorkspaceModel.WorkspaceStateRole)
            }
        }
        return WorkspaceModel.Empty
    }

    // The overflow workspace id (0 if none)
    readonly property int _overflowId: {
        for (var i = 0; i < WorkspaceModel.rowCount(); ++i) {
            var idx = WorkspaceModel.index(i, 0)
            var st  = WorkspaceModel.data(idx, WorkspaceModel.WorkspaceStateRole)
            var wid = WorkspaceModel.data(idx, WorkspaceModel.WorkspaceIdRole)
            var onM = WorkspaceModel.data(idx, WorkspaceModel.WorkspaceOnMonitorRole)
            if (st === WorkspaceModel.Active && wid > 6 && onM) {
                return wid
            }
        }
        return 0
    }
    readonly property bool _overflowVisible: _overflowId > 0
}
```

#### Display logic details

The six-pill fixed range is driven by a `Repeater` with `model: 6`. Each pill calls `_stateForId(index + 1)` which does a linear scan of `WorkspaceModel`. For six workspaces this is O(36) comparisons — negligible. If the number of workspaces grows significantly in a future session, this can be replaced with a `QSortFilterProxyModel` pre-filtered by id range.

The overflow pill checks three conditions simultaneously via `_overflowId`:

1. `wsState === Active` — only active workspaces trigger the overflow
2. `wsId > 6` — only out-of-range workspaces
3. `wsOnMonitor === true` — only if the workspace is actually displayed on a connected monitor

Both `_overflowId` and `_stateForId` are re-evaluated automatically by QML bindings whenever `WorkspaceModel` emits `dataChanged` or `modelReset`.

The `overflowPill.wsState` is hardcoded to `WorkspaceModel.Active` because the overflow pill only appears when an active workspace is out of range — its state is always `Active` by definition.

---

### `TopBar.qml` modification

One insertion between `LogoSection` and the `Item` spacer:

```qml
LogoSection {
    Layout.alignment: Qt.AlignVCenter
}

WorkspaceSection {           // <-- INSERT THIS
    Layout.alignment: Qt.AlignVCenter
}

Item {
    Layout.fillWidth: true
}
```

No other changes to `TopBar.qml`. This satisfies REQ-C-004.

---

## CMakeLists.txt Changes

### New C++ sources in `qt6_add_executable()`

```cmake
qt6_add_executable(holonight-shell
    src/main.cpp
    src/LayerSurface.h
    src/LayerSurface.cpp
    src/LayerShell.h
    src/LayerShellManager.h
    src/LayerShellManager.cpp
    src/WorkspaceModel.h          # NEW
    src/WorkspaceModel.cpp        # NEW
    src/ExtWorkspaceManager.h     # NEW
    src/ExtWorkspaceManager.cpp   # NEW
)
```

### Protocol generation — already present

`qt6_generate_wayland_protocol_client_sources()` in the current `CMakeLists.txt` already includes `protocols/ext-workspace-v1.xml` (committed in the initial scaffold). No change needed there.

### New QML files: `set_source_files_properties` + `qt6_add_qml_module()`

```cmake
set_source_files_properties(src/qml/Topbar/WorkspacePill.qml
    PROPERTIES QT_RESOURCE_ALIAS "Topbar/WorkspacePill.qml")
set_source_files_properties(src/qml/Topbar/WorkspaceSection.qml
    PROPERTIES QT_RESOURCE_ALIAS "Topbar/WorkspaceSection.qml")
```

Add both files to the `QML_FILES` list in `qt6_add_qml_module()`:

```cmake
qt6_add_qml_module(holonight-shell
    URI HolonightShell
    VERSION 1.0
    QML_FILES
        src/qml/Topbar/TopBar.qml
        src/qml/Topbar/BarBackground.qml
        src/qml/Topbar/BarSection.qml
        src/qml/Topbar/LogoSection.qml
        src/qml/Topbar/StatusSection.qml
        src/qml/Topbar/WorkspacePill.qml    # NEW
        src/qml/Topbar/WorkspaceSection.qml # NEW
)
```

### `Qt6::Qt5Compat` link for `Qt5Compat.GraphicalEffects.Glow`

Add to `find_package` and `target_link_libraries`:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick WaylandClient GuiPrivate DBus Network Qt5Compat)
```

```cmake
target_link_libraries(holonight-shell PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Quick
    Qt6::Qt5Compat       # NEW — provides Qt5Compat.GraphicalEffects
    Qt6::DBus
    Qt6::Network
    Qt6::WaylandClient
    ${WAYLAND_CLIENT_LIBRARIES}
    ${TOMLPLUSPLUS_LIBRARIES}
)
```

### Summary of CMakeLists.txt changes

| Change | Location |
|---|---|
| Add `Qt5Compat` to `find_package` | Line 9 |
| Add `WorkspaceModel.h/.cpp` and `ExtWorkspaceManager.h/.cpp` to `qt6_add_executable` | After existing sources |
| Add `QT_RESOURCE_ALIAS` for two new QML files | After existing `set_source_files_properties` block |
| Add two new QML files to `qt6_add_qml_module QML_FILES` | After `StatusSection.qml` |
| Add `Qt6::Qt5Compat` to `target_link_libraries` | After `Qt6::Quick` |

Protocol generation for `ext-workspace-v1.xml` is already declared — no change required.

---

## Key Decisions with Rationale

### 1. `QAbstractListModel` instead of a simpler approach

**Alternatives:** expose workspace state as a flat `QVariantList`, a `QML ListModel`, or individual `Q_PROPERTY` slots on `ExtWorkspaceManager` directly.

`QAbstractListModel` is chosen because:

- It provides change notification granularity via `dataChanged(topLeft, bottomRight, roles)`. QML `Repeater` delegates only rebind the changed properties on affected items, not the whole list.
- It is the canonical Qt pattern for model/view separation. Future sessions may attach a proxy model (e.g. a filter for per-screen workspace groups) without changing QML.
- It is testable in isolation with `QAbstractItemModelTester` (GTest, future session).

A flat `QVariantList` would require replacing the whole list on every update, triggering full delegate recreation in QML — visible as a flash when workspace state changes.

### 2. Overflow pill driven by QML logic, not a model property

**Alternative:** expose `overflowWorkspaceId` and `overflowVisible` as `Q_PROPERTY` on `WorkspaceModel`, computed in C++ after `applyBatchUpdate`.

QML logic is chosen because:

- The overflow condition is purely a display rule (id > 6 AND active AND onMonitor). It does not affect any other part of the system.
- Keeping it in QML means the display rule can be changed without recompiling C++.
- The `WorkspaceModel` stays a pure data model with no display semantics baked in.

The linear scan in `_stateForId` and `_overflowId` is O(N) over a small fixed list (≤ ~20 workspaces in practice). The cost is negligible.

### 3. `QWaylandClientExtensionTemplate` for `ext_workspace_manager_v1`

**Alternative:** raw `wl_registry_listener` (as used internally by `LayerShellManager` in the topbar-skeleton design iteration, though the actual implementation uses `QWaylandClientExtensionTemplate` for `LayerShell`).

`QWaylandClientExtensionTemplate` is chosen because:

- It handles the registry listener, version negotiation, and `initialize()` call automatically.
- It emits `activeChanged` which provides a clean hook for startup warnings if the compositor does not support the protocol.
- `LayerShell.h` already proves the pattern works in this codebase.
- The `ext_workspace_manager_v1` is a singleton global (one per compositor), making the extension template pattern a direct fit.

The tradeoff is that `QWaylandClientExtensionTemplate` does not provide enumeration of `wl_output` globals — but `ExtWorkspaceManager` does not need that. The `onMonitor` flag is derived from `output_enter` / `output_leave` events on the group handles, which the extension does deliver.

### 4. Staged batch updates flushed on `done`

The compositor guarantees that all workspace and group events within a batch are sent before the `done` event. Flushing `WorkspaceModel` only on `done` ensures:

- No partial state (e.g., workspace exists in model but has no name or state yet) is ever visible to QML.
- `dataChanged` is emitted once per batch, not once per protocol event. This minimizes QML binding re-evaluations.

The alternative (update model on each individual event) would cause QML to re-render with intermediate states — e.g., a workspace with `state = Empty` that immediately becomes `Active` in the same compositor roundtrip.

### 5. QML file path: `src/qml/Topbar/` (not `src/qml/Workspaces/`)

The spec's REQ-C-002 mentions `src/qml/Workspaces/` but the prompt instructions and project convention establish `src/qml/Topbar/` as the location for all topbar session components. Placing `WorkspacePill` and `WorkspaceSection` in `src/qml/Topbar/` keeps all topbar QML in one directory and avoids introducing a second QML directory that would require separate `QT_RESOURCE_ALIAS` path handling. The `HolonightShell` URI covers both paths; only the alias prefix matters.

---

## Alternatives Considered

### A. `WorkspaceState` as a top-level Q_GADGET / namespace enum

Declaring `WorkspaceState` outside `WorkspaceModel` (e.g., in a `WorkspaceEnums` namespace with `Q_NAMESPACE` + `Q_ENUM_NS`) would allow the enum to be used in QML without importing the full model. Rejected: unnecessary abstraction for a single enum used in two QML files. The nested enum approach with `WorkspaceModel.Active` etc. is clear and self-documenting.

### B. `Repeater` over the full `WorkspaceModel` with a range filter

Instead of a `model: 6` Repeater plus a separate overflow pill, a single Repeater could iterate the entire model. QML `Repeater` does not natively support range-filtering; this would require a `QSortFilterProxyModel` that filters to ids 1–6 plus the active overflow. The QSortFilterProxyModel approach adds C++ complexity with no runtime benefit over the current O(36) QML scan. Deferred to a future session if performance profiling shows otherwise.

### C. Drive pill visibility from `WorkspaceModel.rowCount()`

Some compositors may not advertise workspaces 1–6 explicitly if they are empty. A `Repeater { model: WorkspaceModel }` with id-based filtering would show only announced workspaces. The spec (REQ-F-004) requires exactly six pills always visible. Using `model: 6` with a `_stateForId` fallback to `Empty` is the direct implementation of this requirement regardless of what the compositor announces.

### D. Use `Qt.GraphicalEffects` from `QtGraphicalEffects` module (Qt 5 style)

The Qt 5 `import QtGraphicalEffects 1.0` path is not available in Qt 6. The correct Qt 6 import is `import Qt5Compat.GraphicalEffects`. This requires linking `Qt6::Qt5Compat`. The name is confusing but is the official Qt 6 path for these effects. A future session may investigate replacing `Glow` with a custom GLSL `ShaderEffect` to remove the `Qt5Compat` dependency entirely (the design assets include QML glow shader examples in `assets/dont-commit/qml-glow-examples/`).

### E. Per-screen `WorkspaceModel` instances

The ext-workspace-v1 protocol groups workspaces by output. A per-screen model would allow the pill strip on each bar to show only workspaces for that bar's output. This is out of scope for this session (REQ-NF-002: no hotplug) and requires correlating `wl_output` pointers between `LayerShellManager` and `ExtWorkspaceManager`. A global singleton model is correct for the current single-group-all-outputs topology that Hyprland uses. Per-screen segmentation is deferred.

---

## Risks

### R1: Compositor does not support `ext-workspace-v1`

**Description:** If the compositor (or a test compositor used in development) does not announce `ext_workspace_manager_v1`, `QWaylandClientExtensionTemplate` never calls `initialize()`. `ExtWorkspaceManager::activeChanged(false)` fires. `WorkspaceModel` remains empty. All six pills render with state `Empty` (correct fallback per `_stateForId`).

**Mitigation:** Log a warning to `stderr` in the `activeChanged(false)` handler. Do not crash. The shell remains functional; workspace pills show empty state. Hyprland supports this protocol; the risk is limited to non-Hyprland test environments.

### R2: Model/view sync timing — QML renders before initial `done` event

**Description:** `LayerShellManager::createBar()` calls `view->setSource()` synchronously. The first QML frame may be rendered before `ExtWorkspaceManager` receives the `done` event and populates the model.

**Mitigation:** `_stateForId` returns `WorkspaceModel.Empty` for any id not found in the model. All six pills will briefly render as empty, then update to their correct state when `modelReset` fires. This is the correct and visible behavior — a one-frame flash to empty then correct state, which in practice is invisible because the `done` event arrives in the same Wayland display roundtrip as the surface configure.

### R3: `Qt5Compat` not installed

**Description:** On minimal Qt6 installations, `Qt6::Qt5Compat` may not be available. The build fails at `find_package`.

**Mitigation:** `Qt6::Qt5Compat` is a standard Qt6 module installed alongside `Qt6::Quick` on most distributions. If missing, the build error is immediate and clear. Alternative: wrap the `Glow` in a `Loader` with a conditional on a CMake-set QML property, deferring glow to an optional component. This is considered over-engineering for this session; `Qt5Compat` is expected to be available in the Arch Linux + Hyprland environment.

### R4: `WorkspaceState.Occupied` never populated

**Description:** The ext-workspace-v1 protocol does not report window occupancy; only `active` and `urgent` bits are in the state enum. `Occupied` is in the model enum and QML styles are defined for it, but it will never be assigned.

**Mitigation:** Documented as a known limitation (see State Transitions section). The `Occupied` visual style is fully implemented and will activate automatically if a future protocol version or Hyprland IPC extension provides the occupancy signal. No user-visible impact: empty workspaces show the `Empty` style, which is slightly dimmer than `Occupied` — this is acceptable and matches the "empty: muted outline" design guideline.

### R5: Token name mismatch in `HoloniightPalette`

**Description:** The installed `Holonight` QML module may expose palette tokens under different names than assumed (e.g., `surfaceActive` may not exist; the actual token may be `surfaceElevated` or similar).

**Mitigation:** Run `task qml-lint` after wiring up the new QML files. `qmllint` resolves `HoloniightPalette` property names against the module's `qmltypes` at lint time and warns on undefined properties. Fix token names before committing. The hex values for each state are specified in the SPEC (REQ-F-006 through REQ-F-009) and can be cross-referenced against the design system's color table to identify the correct token name.

### R6: `Glow` performance at 60 FPS

**Description:** `Qt5Compat.GraphicalEffects.Glow` renders via a multi-pass blur on the GPU. With `samples: 32` and `radius: 18`, this is a significant shader load if many glowing items are visible simultaneously.

**Mitigation:** In practice, at most one pill is `Active` and zero or one is `Urgent` at any time — a maximum of two simultaneous glows across the entire bar. At 34×58 px pill size and radius 18, the glow texture is small. 60 FPS is expected to be maintained (REQ-NF-003). If profiling shows otherwise, `samples` can be reduced to 16 with minimal visual difference, or the glow can be replaced with a custom `ShaderEffect` from the design reference examples.

---

## References

- **Spec:** `docs/sdd/topbar-workspaces/SPEC.md`
- **Top-bar plan:** `docs/sdd/TOPBAR-PLAN.md`
- **Session 1 design:** `docs/sdd/topbar-skeleton/DESIGN.md`
- **Protocol XML:** `protocols/ext-workspace-v1.xml`
- **Existing layer shell extension:** `src/LayerShell.h`
- **Existing QML module:** `src/qml/Topbar/`
- **Build config:** `CMakeLists.txt`, `Taskfile.yml`
- **Design system:** `assets/dont-commit/HoloNight-Design-System.md`
- **Workspace indicator assets:** `assets/dont-commit/ws-indicators/`
- **Glow shader examples:** `assets/dont-commit/qml-glow-examples/`
- **Code style:** `.clang-format`, `.clang-tidy`
- **Project instructions:** `CLAUDE.md`

# Workspace Indicator Redesign — Architecture Design

**Status:** Design Draft
**Date:** 2026-07-01
**Spec:** `docs/sdd/workspace-indicator-redesign/SPEC.md` (v1.0)

This document is grounded in the current code as of commit `2451739` (`task format-check`/`task tidy`
baseline). All file paths are real, all signatures below are what gets written — no placeholder APIs.

---

## 1. Component Overview

```
Wayland compositor (Hyprland)
   │
   ├─ ext-workspace-v1 protocol ─────────────► ExtWorkspaceManager (libs/holonight-core/src/ExtWorkspaceManager.{h,cpp})
   │   (workspace_group, workspace,                 │  owns ExtWorkspaceGroup* (per-output groups)
   │    handle.name/.state, group.output_enter/      │  owns ExtWorkspaceHandle* (per-workspace, numbered + special)
   │    leave, group.workspace_enter/leave)          │  resolves wl_output* → monitor name lazily (§2)
   │                                                  ▼
   │                                          WorkspaceModel::applyBatchUpdate()      (numbered rows)
   │                                          WorkspaceModel::applySpecialWorkspaces() (special rows, NEW)
   │
   └─ Hyprland IPC socket (.socket2.sock) ───► HyprlandWorkspaceService (libs/holonight-core/src/HyprlandWorkspaceService.{h,cpp})
       (activeworkspace, workspace event,             │  WorkspaceModel::setFocusedWorkspaceId()   (global compositor focus)
        urgent client, j/workspaces)                  │  WorkspaceModel::setOccupiedWorkspaceIds() (occupancy)
                                                        │  WorkspaceModel::addUrgentWorkspaceId()    (urgent fallback)
                                                        ▼
                                          WorkspaceModel (QAbstractListModel, QML_SINGLETON)
                                          ── single source of truth, libs/holonight-core/src/WorkspaceModel.{h,cpp}
                                                        │
                                                        │  Q_INVOKABLE queries (stateForId, activeWorkspaceForMonitor,
                                                        │  hasOccupiedOrUrgentBeyond, specialWorkspaceList, ...)
                                                        │  + revisionChanged() as the QML re-evaluation trigger
                                                        ▼
                                          QML layer (apps/shell/qml/Topbar/)
                                          WorkspaceSection.qml (per-monitor, barMonitorName)
                                            ├─ WorkspaceEdgeArrow.qml × 2  (left pan-only, right pan/urgent-jump)  [NEW]
                                            ├─ WorkspacePillStrip.qml      (clipped sliding viewport)              [NEW]
                                            │     └─ WorkspacePill.qml × stripCount (existing, unchanged API)
                                            ├─ separator (Loader, Rectangle)
                                            └─ SpecialWorkspaceDot.qml × N (Repeater over specialWorkspaceList())  [NEW]
```

**Two independent input pipelines, one model — this split is preserved, not touched:**

- `ExtWorkspaceManager` (Wayland protocol) populates `WorkspaceEntry.id/name/state/on_monitor` (and, after
  this change, `monitor_names`/`is_special`) via `applyBatchUpdate()`/`applySpecialWorkspaces()`. It is the
  *only* source of which workspaces exist, their names, and their protocol-reported active/urgent bits.
- `HyprlandWorkspaceService` (Hyprland IPC) never touches `rows_`/`special_rows_` directly — it only feeds
  the three orthogonal overlays already in `WorkspaceModel`: `focused_workspace_id_` (global compositor
  focus, used to disambiguate `FocusedActiveMonitor` vs `FocusedInactiveMonitor` in `effectiveState()`),
  `occupied_workspace_ids_`, and `urgent_workspace_ids_` (a fallback urgent signal for compositors/situations
  where the protocol bit lags). Nothing in this design adds a new HyprlandWorkspaceService → rows_ write path.
- `effectiveState()` remains the single merge point combining a `WorkspaceEntry`'s raw protocol state with
  the two Hyprland-IPC overlays. The new per-monitor query (§2) reads `entry.state` (the *raw* protocol value,
  before `effectiveState()` folds in global focus) — this is intentional and explained in §2.

---

## 2. Per-Monitor Active-Workspace Resolution (REQ-C-001)

### 2.1 Key insight: the protocol already carries per-monitor activeness

`ext_workspace_handle_v1`'s `state` bit `Active` (parsed today in
`ExtWorkspaceHandle::ext_workspace_handle_v1_state`, `ExtWorkspaceManager.cpp:27-37`) is **not** globally
exclusive — Hyprland's `ext-workspace-v1` implementation sets it independently per output's
`ext_workspace_group_handle_v1`, since each output can have its own foreground workspace. The codebase
already establishes group↔workspace membership via `workspace_enter`/`workspace_leave`
(`ExtWorkspaceGroup::workspaces_`, `ExtWorkspaceManager.cpp:64-70`). What's missing is **group↔output-name**
membership. Today `ExtWorkspaceGroup` only counts outputs (`output_count_`, used by `hasOutput()`); it
discards the actual `wl_output*` pointer.

So "the active workspace for monitor M" = "the `id` of the workspace whose raw `entry.state == Active` among
the workspaces in the group that owns output M" — no new tracking primitive is needed, only resolving group
→ monitor name and exposing the per-entry result through `WorkspaceModel`.

### 2.2 `ExtWorkspaceGroup` changes (`ExtWorkspaceManager.h`/`.cpp`)

Replace the output refcount with an actual pointer list, and add a lazy name-resolution method using the
exact reverse of the pattern already established in
`libs/holonight-surfaces/src/PerMonitorLayerManager.cpp:72-75` (`screen->nativeInterface<QNativeInterface::QWaylandScreen>()->output()`):

```cpp
// ExtWorkspaceManager.h — ExtWorkspaceGroup
 public:
  [[nodiscard]] bool hasOutput() const { return !outputs_.isEmpty(); }
  [[nodiscard]] bool hasWorkspace(struct ::ext_workspace_handle_v1* proto) const { return workspaces_.contains(proto); }
  // Resolved lazily (not cached) — see §2.3 for why.
  [[nodiscard]] QStringList monitorNames() const;

 protected:
  void ext_workspace_group_handle_v1_output_enter(struct ::wl_output* output) override;
  void ext_workspace_group_handle_v1_output_leave(struct ::wl_output* output) override;
  ...
 private:
  ExtWorkspaceManager* manager_;
  QList<struct ::wl_output*> outputs_;   // was: int output_count_
  QSet<struct ::ext_workspace_handle_v1*> workspaces_;
```

```cpp
// ExtWorkspaceManager.cpp
void ExtWorkspaceGroup::ext_workspace_group_handle_v1_output_enter(struct ::wl_output* output) {
  if (!outputs_.contains(output)) {
    outputs_.append(output);
  }
}

void ExtWorkspaceGroup::ext_workspace_group_handle_v1_output_leave(struct ::wl_output* output) {
  outputs_.removeOne(output);
}

QStringList ExtWorkspaceGroup::monitorNames() const {
  QStringList names;
  for (auto* output : outputs_) {
    for (QScreen* screen : QGuiApplication::screens()) {
      auto* wayland_screen = screen->nativeInterface<QNativeInterface::QWaylandScreen>();
      if (wayland_screen != nullptr && wayland_screen->output() == output) {
        names.append(screen->name());
        break;
      }
    }
  }
  return names;
}
```

New includes in `ExtWorkspaceManager.cpp`: `<QGuiApplication>`, `<QScreen>`, `<QStringList>`. No new CMake
link is required — `holonight_core` already gets `Qt6::Gui` and the Wayland client extension transitively
through `holonight_platform` (`libs/holonight-platform/CMakeLists.txt:21-27`, linked `PUBLIC` from
`libs/holonight-core/CMakeLists.txt:22-28`). `QNativeInterface::QWaylandScreen` is the public native-interface
API (unlike `QNativeInterface::Private::QWaylandWindow` used in `PerMonitorLayerManager.cpp:60`, which needs
the private QPA headers) — no `GuiPrivate` dependency needed for this half of the round trip.

### 2.3 Timing / race handling — resolve lazily, on query, every time

`ext_workspace_group_handle_v1_output_enter` can fire (compositor → client, at manager bind time) before
Qt has finished creating/mapping the corresponding `QScreen` for that output (Qt's Wayland QPA plugin maps
screens asynchronously off the same Wayland event queue, the ordering relative to the ext-workspace
extension's own bind is not guaranteed). If `monitorNames()` were computed *once*, eagerly, inside
`output_enter`, it could legitimately return an empty list on first connection.

The design resolves this exactly the way `PerMonitorLayerManager` resolves the forward direction — by never
caching the cross-reference. `monitorNames()` re-walks `QGuiApplication::screens()` **every time it's
called**, and it is only ever called from `ExtWorkspaceManager::ext_workspace_manager_v1_done()` (§2.4),
which fires on every protocol round trip (every workspace creation, removal, focus change, etc. across the
whole session) — not just once at startup. So even if the very first `done()` event after connecting
produces empty `monitor_names` for some entries (screen not mapped yet), the next `done()` event — which
will arrive promptly, since Hyprland emits one for essentially any workspace state change — re-resolves
correctly. `activeWorkspaceForMonitor()` itself does no caching either (§2.4); it is a pure scan over
`rows_` at query time, so there is no stale-cache invalidation problem to solve.

### 2.4 `WorkspaceModel` additions

```cpp
// WorkspaceModel.h
struct WorkspaceEntry {
  int id{0};
  QString name;
  WorkspaceState state{WorkspaceState::Empty};
  bool on_monitor{false};
  bool is_special{false};        // NEW — see §4
  QStringList monitor_names;     // NEW — output(s) whose group owns this workspace
};

[[nodiscard]] Q_INVOKABLE int activeWorkspaceForMonitor(const QString& monitorName) const;
```

```cpp
// WorkspaceModel.cpp
int WorkspaceModel::activeWorkspaceForMonitor(const QString& monitorName) const {
  if (monitorName.isEmpty()) {
    return 0;
  }
  for (const auto& entry : rows_) {
    if (entry.is_special) {
      continue;
    }
    if (entry.state == WorkspaceState::Active && entry.monitor_names.contains(monitorName)) {
      return entry.id;
    }
  }
  return 0;
}
```

Note this reads `entry.state` (the **raw** value `ExtWorkspaceHandle::ext_workspace_handle_v1_state` wrote —
`Active`, `Urgent`, or `Empty`), not `effectiveState(entry)`. `effectiveState()` is a *display* transform
that depends on the single global `focused_workspace_id_`; per-monitor activeness must not depend on which
monitor currently has compositor input focus, otherwise every monitor would report the same answer,
defeating the entire feature. This is also why `entry.state` cannot simply be `WorkspaceState::Active` *and*
`WorkspaceState::Urgent` simultaneously today (the parser in `ext_workspace_handle_v1_state` prioritizes the
urgent bit, `ExtWorkspaceManager.cpp:30-36`) — flagged as a known limitation in §10.

`ExtWorkspaceManager::ext_workspace_manager_v1_done()` (`ExtWorkspaceManager.cpp:108-125`) gains one more
per-entry computation alongside the existing `on_monitor` loop:

```cpp
void ExtWorkspaceManager::ext_workspace_manager_v1_done() {
  QList<WorkspaceModel::WorkspaceEntry> entries;
  QList<WorkspaceModel::SpecialWorkspaceEntry> specials;
  entries.reserve(handle_map_.size());

  for (auto* handle : std::as_const(handle_map_)) {
    WorkspaceModel::WorkspaceEntry entry = handle->entry_;
    for (const auto* group : std::as_const(groups_)) {
      if (group->hasOutput() && group->hasWorkspace(handle->raw_)) {
        entry.on_monitor = true;
        entry.monitor_names += group->monitorNames();
      }
    }
    if (entry.is_special) {
      specials.append(WorkspaceModel::SpecialWorkspaceEntry{
          .name = entry.name, .active = entry.state == WorkspaceModel::WorkspaceState::Active,
          .urgent = entry.state == WorkspaceModel::WorkspaceState::Urgent});
    } else {
      entries.append(entry);
    }
  }

  std::ranges::sort(entries, {}, &WorkspaceModel::WorkspaceEntry::id);
  model_->applyBatchUpdate(entries);
  model_->applySpecialWorkspaces(specials);
}
```

(The `break` after the first matching group is dropped in favor of `+=` so a workspace that — in an unusual
mirrored-output configuration — sits in a group bound to two outputs reports both monitor names. Single-output
groups, the normal case, just get a one-element list, same cost as before.)

### 2.5 Single-monitor correctness (REQ-C-001 AC5)

With one monitor, exactly one `ExtWorkspaceGroup` exists with exactly one entry in `outputs_`, so
`monitorNames()` returns a single-element list equal to that screen's `QScreen::name()`. `WorkspaceSection.qml`'s
`barMonitorName` (set via `setInitialProperties` before `view->setSource()`, same mechanism documented in
project conventions for `ActiveWindowService`) matches it exactly, so `activeWorkspaceForMonitor(barMonitorName)`
resolves on the first call with no special-casing needed in the C++ or QML.

---

## 3. Sliding Viewport Mechanism (REQ-F-003)

### 3.1 Why a real strip, not index relabeling

The alternative ("relabel") would keep exactly N `WorkspacePill` instances and rewrite each one's `wsId`
when `window_start` changes, animating only `opacity`/text-crossfade. That satisfies REQ-F-002's literal AC1
("exactly N pills instantiated") but cannot produce the *sliding* motion REQ-F-003 explicitly specifies
("pills shall slide to their new positions") — there is nothing to slide; the same N items would just
relabel in place. The task brief is explicit that the sliding-viewport approach was already decided during
grilling, so this design implements a continuous absolute-ID strip, clipped to a fixed-width window, with an
animated `x` offset. See §9 for the resulting (and accepted) tension with REQ-F-002 AC1's literal wording.

### 3.2 `WorkspacePillStrip.qml` (NEW, `apps/shell/qml/Topbar/`)

```qml
pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell

Item {
    id: root

    required property string barMonitorName
    required property int windowStart        // first visible absolute workspace id (>= 1)

    readonly property int pillSize: 32
    readonly property int pillSpacing: 16
    readonly property int pillStep: root.pillSize + root.pillSpacing
    readonly property int stripPad: 1         // ensures a real pill exists just past the right edge to slide in

    implicitWidth: WorkspaceModel.displayCount * root.pillStep - root.pillSpacing
    implicitHeight: root.pillSize
    clip: true

    readonly property int stripCount: Math.max(
        root.windowStart + WorkspaceModel.displayCount - 1 + root.stripPad,
        WorkspaceModel.revision >= 0 ? WorkspaceModel.maxWorkspaceId() : 0)

    Item {
        id: strip
        width: root.stripCount * root.pillStep
        height: root.pillSize
        x: -(root.windowStart - 1) * root.pillStep

        Behavior on x {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        Repeater {
            model: root.stripCount
            delegate: WorkspacePill {
                required property int index
                readonly property int absoluteId: index + 1
                x: index * root.pillStep
                y: 0
                wsId: absoluteId
                barMonitorName: root.barMonitorName
                wsState: WorkspaceModel.revision >= 0
                         ? WorkspaceModel.stateForId(absoluteId)
                         : WorkspaceModel.Empty
            }
        }
    }
}
```

**Strip width.** `stripCount` is bounded below by `windowStart + N - 1 + stripPad` — the right edge of the
visible window plus one extra real pill, so a single-step right-pan or a one-step recenter (the common case
per the worked examples in the spec, e.g. active 3→4) always slides in an already-instantiated neighbor
instead of revealing a gap. It's also bounded below by `WorkspaceModel.maxWorkspaceId()` (new helper, §3.4) so
that a large jump (e.g. clicking pill 5 while window is `[1..5]`, or a compositor keybind jumping to workspace
20) still has every intermediate pill already present in the strip to slide through, rather than animating
across instantiated-on-demand placeholders. There is no left pad: the strip always starts at absolute id 1,
and `windowStart` is left-clamped at 1, so nothing can ever need to exist left of id 1.

**Binding to absolute IDs, not window-relative index.** Each delegate's `wsId`/`absoluteId` is `index + 1`
against the *whole* strip (`Repeater { model: root.stripCount }`), not against the N-wide visible window.
This is what makes the `x` offset on the outer `strip` Item alone sufficient to implement scrolling — pills
never need their model index renumbered, only the container's offset changes.

**Clipping.** The outer `root` `Item` is a fixed `implicitWidth` of exactly `N * pillStep - spacing` (the
footprint of N pills) with `clip: true`. The inner `strip` Item is the full `stripCount`-wide content; only
the `[windowStart-1, windowStart-1+N) * pillStep`-wide slice is visible at any time, identical in effect to
`Flickable`/`ListView` viewport clipping but without the input-handling overhead those bring (clicks need to
land precisely on individual `WorkspacePill` `MouseArea`s, not be eaten by a flickable's drag gesture).

**Trigger.** `windowStart` is a `required property` set from `WorkspaceSection.qml` (§3.3); whenever it
changes, the `strip.x` binding re-evaluates and the `Behavior on x` (200 ms, `Easing.OutCubic` — within the
150–250 ms / consistency-with-existing-`Behavior on x`-patterns range required by REQ-F-003 AC4 and
REQ-NF-002 AC1) animates the transition.

### 3.3 `WorkspaceSection.qml` — window-centering + manual pan state

```qml
readonly property int activeWorkspaceId: WorkspaceModel.revision >= 0
    ? WorkspaceModel.activeWorkspaceForMonitor(root.barMonitorName)
    : 0

property int manualPanOffset: 0

// QML only fires onXChanged when the *value* actually changes, even though this binding
// re-evaluates on every WorkspaceModel.revision tick (incidental occupancy/urgency churn on
// other monitors bumps revision constantly). That built-in value-change semantics is exactly
// the "ignore incidental revision bumps, only reset on a real per-monitor focus change" rule
// REQ-F-004 asks for — no manual previous-value bookkeeping needed.
onActiveWorkspaceIdChanged: root.manualPanOffset = 0

readonly property int targetWindowStart: Math.max(
    1, root.activeWorkspaceId - Math.floor((WorkspaceModel.displayCount - 1) / 2))
readonly property int windowStart: Math.max(1, root.targetWindowStart + root.manualPanOffset)
```

### 3.4 New `WorkspaceModel` helper backing the strip's width bound

```cpp
[[nodiscard]] Q_INVOKABLE int maxWorkspaceId() const;
```
```cpp
int WorkspaceModel::maxWorkspaceId() const {
  int max_id = 0;
  for (const auto& entry : rows_) {
    max_id = std::max(max_id, entry.id);
  }
  return max_id;
}
```

### 3.5 Manual pan-offset survives "incidental" revision bumps — by construction

`manualPanOffset` is a plain (non-readonly) property mutated only by the edge-arrow click handlers (§5).
Nothing else writes it. `windowStart` is *derived* from `targetWindowStart + manualPanOffset`, and
`targetWindowStart` itself only changes when `activeWorkspaceId` changes (§3.3's `onActiveWorkspaceIdChanged`
handler is the only thing that zeroes `manualPanOffset`, and it only fires on an actual value change of
this monitor's active workspace). A `revisionChanged()` caused by, say, occupancy churn on a different
monitor re-evaluates `activeWorkspaceId`'s binding, computes the *same* value as before, so
`onActiveWorkspaceIdChanged` does not fire, and `manualPanOffset` is untouched — satisfying the Pan-Reset
Rule's negative cases (occupancy/urgency changes elsewhere, special-workspace churn) for free, with no
special-casing required.

---

## 4. Special Workspace Data Model (REQ-F-005, REQ-F-006)

### 4.1 Positive `is_special` flag, not `id == 0` inference

Today, `ExtWorkspaceHandle::ext_workspace_handle_v1_name` (`ExtWorkspaceManager.cpp:18-25`) only writes
`entry_.id` when `name.toInt(&parsed_ok)` succeeds; on failure `entry_.id` is left at its default-member-init
value of `0`. That is indistinguishable from a workspace literally named `"0"` (which *does* parse
successfully, `parsed_ok == true`, `parsed_id == 0`) — both end up with `id == 0`, so downstream code cannot
tell "this is special" from "this is a buggy/edge-case numbered workspace 0" by inspecting `id` alone. Fix:

```cpp
// ExtWorkspaceManager.cpp
void ExtWorkspaceHandle::ext_workspace_handle_v1_name(const QString& name) {
  entry_.name = name;
  bool parsed_ok = false;
  const int parsed_id = name.toInt(&parsed_ok);
  entry_.is_special = !parsed_ok;
  if (parsed_ok) {
    entry_.id = parsed_id;
  }
}
```

`is_special` is set unconditionally on every name update (handles renames mid-session, however unlikely).
`ext_workspace_manager_v1_done()` (§2.4) now routes on this explicit flag rather than on `id`.

### 4.2 Storage and QML-facing API: Q_INVOKABLE/property, not a second `QAbstractListModel`

```cpp
// WorkspaceModel.h
struct SpecialWorkspaceEntry {
  QString name;       // unique identifier — e.g. "special:scratch"
  bool active{false};
  bool urgent{false};
};

[[nodiscard]] Q_INVOKABLE QVariantList specialWorkspaceList() const;
Q_INVOKABLE void activateSpecialWorkspace(const QString& name);
void applySpecialWorkspaces(const QList<SpecialWorkspaceEntry>& entries);

 Q_SIGNALS:
  void activateSpecialWorkspaceRequested(const QString& name);

 private:
  QList<SpecialWorkspaceEntry> special_rows_;
```

```cpp
// WorkspaceModel.cpp
QVariantList WorkspaceModel::specialWorkspaceList() const {
  QVariantList list;
  list.reserve(special_rows_.size());
  for (const auto& entry : special_rows_) {
    list.append(QVariantMap{
        {QStringLiteral("name"), entry.name},
        {QStringLiteral("active"), entry.active},
        {QStringLiteral("urgent"), entry.urgent},
    });
  }
  return list;
}

void WorkspaceModel::applySpecialWorkspaces(const QList<SpecialWorkspaceEntry>& entries) {
  if (special_rows_ == entries) {  // requires SpecialWorkspaceEntry operator==, see below
    return;
  }
  special_rows_ = entries;
  ++revision_;
  emit revisionChanged();
}

void WorkspaceModel::activateSpecialWorkspace(const QString& name) {
  if (name.isEmpty()) {
    return;
  }
  emit activateSpecialWorkspaceRequested(name);
}
```

`SpecialWorkspaceEntry` needs `bool operator==(const SpecialWorkspaceEntry&) const = default;` (struct is a
plain aggregate of comparable members, same pattern `BarWorkspacesConfig` uses in
`libs/holonight-config/include/holonight_config/config_structs.h:29`) so `applySpecialWorkspaces` can no-op
on a `done()` event that didn't actually change special-workspace state, avoiding a spurious `revisionChanged()`
storm — not strictly required for correctness (everything downstream is idempotent against extra revision
bumps, same as every other setter in this class) but cheap and consistent.

**Why Q_INVOKABLE + plain struct, not a second `QAbstractListModel`:** `WorkspaceModel` already mixes both
idioms — the numbered-pill data is exposed as model rows (`rowCount`/`data`/`roleNames`) **but nothing in
the current QML actually consumes it that way**: `WorkspaceSection.qml`'s `Repeater` is driven by
`model: WorkspaceModel.displayCount` (a plain int) and per-pill state comes from the `Q_INVOKABLE
stateForId(int)` call, not from binding to `WorkspaceModel` as a `ListView`/`Repeater` model. The overflow
pills use the same `Q_INVOKABLE`-returns-a-value-tuple pattern (`overflowWorkspaceId()` +
`overflowWorkspaceState()` + ...). A second `QAbstractListModel` would be the only consumer of the
`QAbstractListModel` machinery in this class that's actually exercised by `Repeater.model`, introducing a
second, structurally different way of doing the same thing (`roleNames()`, `beginInsertRows`/`beginRemoveRows`
bookkeeping for a list that's wholesale-replaced on every `done()` event anyway) for no behavioral gain.
`Repeater { model: WorkspaceModel.revision >= 0 ? WorkspaceModel.specialWorkspaceList() : [] }` is a drop-in
match for the QML idiom every other dynamic value in this file already uses, and `specialWorkspaceList()`
being a plain `QVariantList` of `QVariantMap` is directly usable from `Repeater`'s `modelData` without any
role-name plumbing.

### 4.3 Live add/remove (REQ-F-005 AC9/AC10)

`ExtWorkspaceHandle::ext_workspace_handle_v1_removed()` already removes the handle from `handle_map_` and
self-deletes (`ExtWorkspaceManager.cpp:39-43`) — unchanged. The next `ext_workspace_manager_v1_done()` (which
the protocol guarantees follows a batch of adds/removes) rebuilds `specials` from scratch and calls
`applySpecialWorkspaces()`, which replaces `special_rows_` wholesale. Since `specialWorkspaceList()` is a pure
function of `special_rows_` re-evaluated on every QML call gated by `WorkspaceModel.revision`, a
newly-appeared or newly-destroyed special workspace shows up in the `Repeater`'s model on the very next
`revisionChanged()` — no diffing, no incremental row management, same "wholesale replace + bump revision"
pattern `applyBatchUpdate` already uses for numbered rows.

---

## 5. New/Changed QML Components

| File | Change |
|---|---|
| `apps/shell/qml/Topbar/WorkspaceSection.qml` | Rewritten body: owns `activeWorkspaceId`/`manualPanOffset`/`windowStart` (§3.3); lays out left `WorkspaceEdgeArrow` (`Loader`), `WorkspacePillStrip`, right `WorkspaceEdgeArrow` (`Loader`), separator (`Loader`), `Repeater` of `SpecialWorkspaceDot` over `specialWorkspaceList()`. All six `_overflow*`/`_urgentOverflow*` properties and the two extra `WorkspacePill` instances deleted (§6). |
| `apps/shell/qml/Topbar/WorkspacePill.qml` | Two single-line color changes (§7) + new top-center dot for `FocusedInactiveMonitor` (§8). No signature/property changes — `wsId`/`wsState`/`barMonitorName`/`label`/`active` are unchanged, so `WorkspacePillStrip.qml` instantiates it exactly as `WorkspaceSection.qml` does today. |
| `apps/shell/qml/Topbar/WorkspacePillStrip.qml` | **NEW.** Clipped sliding-viewport container, §3.2. |
| `apps/shell/qml/Topbar/WorkspaceEdgeArrow.qml` | **NEW.** Chevron glyph (`Shape`/`ShapePath`, same primitive `WorkspacePill` already uses), `urgent` pulsing-glow mode, `activated()` signal. §5.1. |
| `apps/shell/qml/Topbar/SpecialWorkspaceDot.qml` | **NEW.** Circular dot, radius/color/glow per state, click → `WorkspaceModel.activateSpecialWorkspace(name)`. §5.2. |

No `CMakeLists.txt` change is needed for the three new files: `apps/shell/CMakeLists.txt:44-49` globs
`qml/*.qml` recursively (`file(GLOB_RECURSE ... CONFIGURE_DEPENDS qml/*.qml)`) and derives each file's
`QT_RESOURCE_ALIAS`/QRC path from its location under `apps/shell/qml/` automatically — dropping a new
`.qml` file into `Topbar/` is sufficient (CMake reconfigures automatically via `CONFIGURE_DEPENDS` on the
next build). They land at `qrc:/HolonightShell/Topbar/WorkspacePillStrip.qml` etc. and are usable from any
file with `import HolonightShell`, same as `WorkspacePill` is today (no per-file import needed beyond what
`WorkspacePill.qml` already shows working for `BarTooltipArea`, which lives in the same directory).

### 5.1 `WorkspaceEdgeArrow.qml`

```qml
import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import Holonight

Item {
    id: root
    required property bool pointRight   // chevron direction
    required property bool urgent       // true if an urgent workspace lies beyond this edge (either direction)

    signal activated()

    width: 20
    height: 32

    property real urgentPulseOpacity: 0.45
    SequentialAnimation on urgentPulseOpacity {
        running: root.urgent
        loops: Animation.Infinite
        NumberAnimation { to: 0.95; duration: 760; easing.type: Easing.InOutSine }
        NumberAnimation { to: 0.40; duration: 760; easing.type: Easing.InOutSine }
    }

    Shape {
        id: glyph
        anchors.centerIn: parent
        width: 10
        height: 16
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.urgent ? HoloniightPalette.accentViolet : HoloniightPalette.workspaceOccupied
            strokeColor: "transparent"
            startX: root.pointRight ? 0 : glyph.width
            startY: 0
            PathLine { x: root.pointRight ? glyph.width : 0; y: glyph.height / 2 }
            PathLine { x: root.pointRight ? 0 : glyph.width; y: glyph.height }
            PathLine { x: root.pointRight ? 0 : glyph.width; y: 0 }
        }
    }

    MultiEffect {
        source: glyph
        anchors.fill: glyph
        visible: root.urgent
        shadowEnabled: true
        shadowColor: HoloniightPalette.accentViolet
        shadowBlur: 0.35
        shadowOpacity: root.urgentPulseOpacity
        shadowScale: 1.1
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.activated()
    }
}
```

`MultiEffect` is declared immediately after its source `glyph` and before nothing else stacks on top of it
(no text/overlay sits above the arrow glyph), so the project's "`MultiEffect` must precede elements that
should render above it" rule is trivially satisfied — there's nothing left to declare after it.

### 5.2 `SpecialWorkspaceDot.qml`

```qml
import QtQuick
import QtQuick.Effects
import HolonightShell
import Holonight

Item {
    id: root
    required property string wsName
    required property bool active
    required property bool urgent
    required property string barMonitorName

    readonly property bool hidden: !root.active && !root.urgent
    readonly property real targetDiameter: root.hidden ? 8 : 16   // radius 4px / 8px per spec

    width: 16
    height: 16

    property real urgentPulseOpacity: 0.5
    SequentialAnimation on urgentPulseOpacity {
        running: root.urgent
        loops: Animation.Infinite
        NumberAnimation { to: 0.95; duration: 760; easing.type: Easing.InOutSine }
        NumberAnimation { to: 0.40; duration: 760; easing.type: Easing.InOutSine }
    }

    Rectangle {
        id: dot
        anchors.centerIn: parent
        width: root.targetDiameter
        height: root.targetDiameter
        radius: width / 2
        color: root.urgent ? HoloniightPalette.accentViolet
             : root.active ? HoloniightPalette.accentCyan
             : HoloniightPalette.textDisabled

        Behavior on width { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
        Behavior on height { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: 130 } }
    }

    MultiEffect {
        source: dot
        anchors.fill: dot
        visible: root.active || root.urgent
        shadowEnabled: true
        shadowColor: dot.color
        shadowBlur: 0.3
        shadowOpacity: root.urgent ? root.urgentPulseOpacity : 0.5
    }

    MouseArea {
        id: pointer
        anchors.fill: parent
        hoverEnabled: true
        onClicked: WorkspaceModel.activateSpecialWorkspace(root.wsName)
    }

    BarTooltipArea {
        barMonitorName: root.barMonitorName
        title: root.wsName
        description: root.urgent ? "Needs attention." : (root.active ? "Currently visible." : "Hidden special workspace.")
        iconName: "workspace"
    }
}
```

### 5.3 `WorkspaceSection.qml` final body

```qml
pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell
import Holonight
import "../Controls"

BarSection {
    id: root

    required property string barMonitorName
    readonly property int slantCut: 12
    readonly property int contentLeftMargin: 24 + root.slantCut
    readonly property int contentRightMargin: 24 + root.slantCut
    readonly property int inheritedSectionPadding: 8

    readonly property int activeWorkspaceId: WorkspaceModel.revision >= 0
        ? WorkspaceModel.activeWorkspaceForMonitor(root.barMonitorName)
        : 0
    property int manualPanOffset: 0
    onActiveWorkspaceIdChanged: root.manualPanOffset = 0

    readonly property int targetWindowStart: Math.max(
        1, root.activeWorkspaceId - Math.floor((WorkspaceModel.displayCount - 1) / 2))
    readonly property int windowStart: Math.max(1, root.targetWindowStart + root.manualPanOffset)
    readonly property int windowEndExclusive: root.windowStart + WorkspaceModel.displayCount

    readonly property bool rightUrgentBeyond: WorkspaceModel.revision >= 0
        ? WorkspaceModel.hasUrgentBeyond(root.windowEndExclusive) : false
    readonly property bool rightOccupiedBeyond: WorkspaceModel.revision >= 0
        ? WorkspaceModel.hasOccupiedOrUrgentBeyond(root.windowEndExclusive) : false
    readonly property bool leftUrgentBefore: WorkspaceModel.revision >= 0
        ? WorkspaceModel.hasUrgentBefore(root.windowStart) : false
    readonly property var specialWorkspaces: WorkspaceModel.revision >= 0
        ? WorkspaceModel.specialWorkspaceList() : []

    implicitWidth: Math.max(392, root.contentLeftMargin + pillRow.implicitWidth + root.contentRightMargin)

    HudFrame {
        id: frameCanvas
        anchors { fill: parent; leftMargin: -root.inheritedSectionPadding; rightMargin: -root.inheritedSectionPadding }
        variant: HudFrame.Section
        leftTopOffset: root.slantCut
        rightBottomOffset: root.slantCut
    }

    Row {
        id: pillRow
        anchors {
            left: parent.left
            leftMargin: root.contentLeftMargin - root.inheritedSectionPadding
            right: parent.right
            rightMargin: root.contentRightMargin - root.inheritedSectionPadding
            verticalCenter: parent.verticalCenter
        }
        spacing: 12
        move: Transition { NumberAnimation { properties: "x"; duration: 150; easing.type: Easing.OutCubic } }

        Loader {
            anchors.verticalCenter: parent.verticalCenter
            active: root.windowStart > 1
            sourceComponent: leftArrowComponent
        }

        WorkspacePillStrip {
            anchors.verticalCenter: parent.verticalCenter
            barMonitorName: root.barMonitorName
            windowStart: root.windowStart
        }

        Loader {
            anchors.verticalCenter: parent.verticalCenter
            active: root.rightOccupiedBeyond
            sourceComponent: rightArrowComponent
        }

        Loader {
            anchors.verticalCenter: parent.verticalCenter
            active: root.specialWorkspaces.length > 0
            sourceComponent: separatorComponent
        }

        Repeater {
            model: root.specialWorkspaces
            delegate: SpecialWorkspaceDot {
                required property var modelData
                anchors.verticalCenter: parent.verticalCenter
                wsName: modelData.name
                active: modelData.active
                urgent: modelData.urgent
                barMonitorName: root.barMonitorName
            }
        }
    }

    Component {
        id: leftArrowComponent
        WorkspaceEdgeArrow {
            pointRight: false
            urgent: root.leftUrgentBefore
            onActivated: {
                if (root.leftUrgentBefore) {
                    WorkspaceModel.activateWorkspace(WorkspaceModel.lastUrgentIdBefore(root.windowStart))
                } else {
                    root.manualPanOffset -= 1
                }
            }
        }
    }

    Component {
        id: rightArrowComponent
        WorkspaceEdgeArrow {
            pointRight: true
            urgent: root.rightUrgentBeyond
            onActivated: {
                if (root.rightUrgentBeyond) {
                    WorkspaceModel.activateWorkspace(WorkspaceModel.firstUrgentIdBeyond(root.windowEndExclusive))
                } else {
                    root.manualPanOffset += 1
                }
            }
        }
    }

    Component {
        id: separatorComponent
        Rectangle {
            width: 1
            height: 24
            color: HoloniightPalette.borderPassive
            opacity: 0
            Component.onCompleted: opacity = 1
            Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        }
    }
}
```

`Component { ... }` blocks used as `Loader.sourceComponent` are not subject to the
`pragma ComponentBehavior: Bound` restriction that forces `required property` capture inside `Repeater`
delegates — that restriction targets the implicit per-row context object (`index`/`modelData`) a `Repeater`
or `Instantiator` injects, not ordinary id-based lookups across a `Component` boundary within the same file.
`root.manualPanOffset`/`root.rightUrgentBeyond`/etc. are resolved through normal QML object-tree id scoping,
exactly like `root.barMonitorName` is already referenced directly inside the existing `Repeater` delegate in
the current file (`WorkspaceSection.qml:47`, unguarded by `required property`).

`Row`'s `move: Transition` smooths the reflow when the separator/dots are added or removed (Loader/Repeater
membership changes are otherwise instantaneous — see §9 for why this is an accepted simplification rather
than a true item-level fade-out).

---

## 6. Removal of the Old Overflow Mechanism

| Removed | Replaced by |
|---|---|
| `WorkspaceSection.qml`: `_overflowId`, `_overflowState`, `_urgentOverflowId`, `_urgentOverflowState`, `_urgentOverflowLabel` properties; the two extra `WorkspacePill { wsId: root._overflowId ... }` / `{ wsId: root._urgentOverflowId ... }` instances | `WorkspacePillStrip` (continuous absolute-ID strip, §3) makes any workspace beyond the fixed N reachable by sliding the window instead of a single bolt-on pill; `WorkspaceEdgeArrow` (right) replaces the urgent-overflow pill's "jump to the urgent one" behavior |
| `WorkspaceModel::overflowWorkspaceId()` | No longer meaningful — there is no single "the" overflow pill; replaced conceptually by `windowStart`/`windowEndExclusive`, computed entirely in QML (§3.3) |
| `WorkspaceModel::overflowWorkspaceState()` | Subsumed into `stateForId(int)`, already called per-pill by the strip for every absolute id in range |
| `WorkspaceModel::overflowUrgentWorkspaceId()` | `WorkspaceModel::firstUrgentIdBeyond(int minId)` (NEW, §6.1) — generalized to "first urgent id at/after a given id" rather than hardcoded to `display_count_` |
| `WorkspaceModel::overflowUrgentWorkspaceState()` | `WorkspaceModel::hasUrgentBeyond(int minId)` (NEW, §6.1) — drives `WorkspaceEdgeArrow.urgent`, which derives its own color/glow internally |
| `WorkspaceModel::overflowUrgentWorkspaceLabel()` | No longer needed — the old urgent-overflow pill rendered a `"+N"`/numeric label text; `WorkspaceEdgeArrow` is a plain chevron glyph with no label, since the spec's arrow-styling table (REQ-F-004) specifies color/glow only, not text |
| `WorkspaceModel::hiddenUrgentWorkspaceCount()` | No longer needed — it only existed to compute the old `"+N"` label, which no longer exists |

### 6.1 New `WorkspaceModel` predicate helpers backing the edge arrows

```cpp
[[nodiscard]] Q_INVOKABLE bool hasOccupiedOrUrgentBeyond(int minId) const;
[[nodiscard]] Q_INVOKABLE bool hasUrgentBeyond(int minId) const;
[[nodiscard]] Q_INVOKABLE int firstUrgentIdBeyond(int minId) const;
[[nodiscard]] Q_INVOKABLE bool hasUrgentBefore(int maxIdExclusive) const;
[[nodiscard]] Q_INVOKABLE int lastUrgentIdBefore(int maxIdExclusive) const;
```

```cpp
bool WorkspaceModel::hasOccupiedOrUrgentBeyond(int minId) const {
  return std::ranges::any_of(rows_, [this, minId](const WorkspaceEntry& entry) {
    if (entry.id < minId) {
      return false;
    }
    const WorkspaceState state = effectiveState(entry);
    return state == WorkspaceState::Occupied || state == WorkspaceState::Urgent;
  });
}

bool WorkspaceModel::hasUrgentBeyond(int minId) const {
  return std::ranges::any_of(rows_, [this, minId](const WorkspaceEntry& entry) {
    return entry.id >= minId && effectiveState(entry) == WorkspaceState::Urgent;
  });
}

int WorkspaceModel::firstUrgentIdBeyond(int minId) const {
  int lowest = 0;
  for (const auto& entry : rows_) {
    if (entry.id >= minId && effectiveState(entry) == WorkspaceState::Urgent) {
      if (lowest == 0 || entry.id < lowest) {
        lowest = entry.id;
      }
    }
  }
  return lowest;
}

bool WorkspaceModel::hasUrgentBefore(int maxIdExclusive) const {
  return std::ranges::any_of(rows_, [this, maxIdExclusive](const WorkspaceEntry& entry) {
    return entry.id < maxIdExclusive && effectiveState(entry) == WorkspaceState::Urgent;
  });
}

int WorkspaceModel::lastUrgentIdBefore(int maxIdExclusive) const {
  int highest = 0;
  for (const auto& entry : rows_) {
    if (entry.id < maxIdExclusive && effectiveState(entry) == WorkspaceState::Urgent) {
      highest = std::max(highest, entry.id);
    }
  }
  return highest;
}
```

(`std::ranges::any_of` per this project's clang-tidy preference, `CLAUDE.md` "clang-tidy Gotchas".) These
use `effectiveState()`, not raw `entry.state` — unlike `activeWorkspaceForMonitor` (§2.4), the edge-arrow
visibility/urgency questions are legitimately global ("is there anything occupied/urgent beyond this
monitor's visible window", matching the old `overflowWorkspaceId()`'s behavior which also scanned all
`rows_` unconditionally) and should reflect the same Hyprland-IPC-overlaid urgent/occupied state every other
pill displays, including the `urgent_workspace_ids_` fallback. `firstUrgentIdBeyond`/`lastUrgentIdBefore`
both resolve to the **nearest** urgent id to the visible window on their respective side (lowest-above for
the right edge, highest-below for the left edge) per SPEC REQ-F-004 AC6/AC7 ("nearest").

---

## 7. Urgent Color Unification (REQ-F-007)

Two literal line changes in `WorkspacePill.qml`'s `_style` `QtObject` (current lines 140 and 151):

```diff
- if (urgent)          return HoloniightPalette.borderUrgent
+ if (urgent)          return HoloniightPalette.accentViolet
```
```diff
- if (urgent)                           return HoloniightPalette.error
+ if (urgent)                           return HoloniightPalette.accentViolet
```

`WorkspaceEdgeArrow.qml` and `SpecialWorkspaceDot.qml` (§5.1, §5.2) are written from scratch using
`HoloniightPalette.accentViolet` directly — they never reference `borderUrgent`/`error` to begin with, so
REQ-F-007 AC2/AC3 are satisfied by construction, not by a later cleanup pass.

---

## 8. `FocusedInactiveMonitor` Restyle (REQ-F-008)

`WorkspacePill.qml` `_style` block changes (current lines 126, 139, 144, 150):

```diff
  readonly property color fill: {
      if (focusedActive)   return HoloniightPalette.workspaceActive
-     if (focusedInactive) return HoloniightPalette.surface
+     if (focusedInactive) return HoloniightPalette.workspaceOccupied
      if (urgent)          return HoloniightPalette.workspaceOccupied
      if (occupied)        return HoloniightPalette.workspaceOccupied
      return HoloniightPalette.surface
  }
  readonly property color borderColor: {
      if (focusedActive)   return HoloniightPalette.accentCyan
-     if (focusedInactive) return HoloniightPalette.borderActive
+     if (focusedInactive) return HoloniightPalette.borderPassive   // irrelevant — width is 0 below
      if (urgent)          return HoloniightPalette.accentViolet
      if (occupied)        return HoloniightPalette.borderPassive
      return HoloniightPalette.borderPassive
  }
  readonly property real borderWidth: {
      if (focusedActive) return 1.8
-     if (focusedInactive || urgent) return 1.4
+     if (focusedInactive) return 0
+     if (urgent) return 1.4
      return 1.0
  }
  readonly property color textColor: {
-     if (focusedActive || focusedInactive) return HoloniightPalette.textPrimary
+     if (focusedActive) return HoloniightPalette.textPrimary
+     if (focusedInactive) return HoloniightPalette.textSecondary
      if (urgent)                           return HoloniightPalette.accentViolet
      if (occupied)                         return HoloniightPalette.textSecondary
      return HoloniightPalette.textMuted
  }
```

`fillOpacity` (current line 132) needs **no change** — `focusedInactive` is already bucketed with
`occupied || urgent` at the `0.72` base opacity, matching `Occupied`'s visual weight as required.

New top-center dot, inserted after the existing `Text` element (i.e. after the `MultiEffect`/glow
declaration, so it always paints on top per the project's MultiEffect z-order rule) and before `MouseArea`:

```qml
Rectangle {
    id: inactiveMonitorDot
    width: 7
    height: 7
    radius: width / 2
    color: HoloniightPalette.accentCyan
    anchors.horizontalCenter: pill.horizontalCenter
    anchors.bottom: pill.top
    anchors.bottomMargin: 3
    opacity: _style.focusedInactive ? 1 : 0
    visible: opacity > 0.01

    Behavior on opacity {
        NumberAnimation { duration: 100; easing.type: Easing.OutCubic }
    }
}
```

3–4 px radius via a 6–8 px diameter `Rectangle` (chosen 7 px, mid-range); `anchors.bottom: pill.top` with a
3 px margin positions it "2–4 px above the top edge" of the pill shape; `visible: opacity > 0.01` plus the
`Behavior on opacity` gives the ~100 ms fade-in/out REQ-F-008 AC7 asks for while still fully removing it from
painting once transparent.

---

## 9. Key Decisions

**Sliding viewport vs. relabeling N fixed pills (already decided).** Implemented as a genuine wider strip
(§3) because REQ-F-003's "pills shall slide to their new positions via smooth animation" cannot be expressed
by relabeling the same N items in place — there's nothing to slide. Accepted consequence: this puts REQ-F-003
in direct tension with REQ-F-002 AC1 ("exactly N `WorkspacePill` elements are instantiated"), since the strip
must instantiate `stripCount ≥ N` pills so off-window neighbors exist to slide in/through. Resolution: treat
REQ-F-002 AC2 ("every numbered pill 1..N is rendered **visible**... at all times") as the binding,
user-observable contract, and AC1's literal instantiation count as superseded by the later, more specific
REQ-F-003 sliding-viewport mechanism the task explicitly directed ("a wider internal pill strip ... clipped,
animated x offset ... decided during grilling"). The extra pad pills are never visible to the user (clipped)
and cost nothing perceptible (`stripCount` is bounded by real workspace counts, typically single digits).
This is flagged again in §10 as a spec-internal inconsistency worth resolving explicitly with stakeholders
before sign-off, not silently.

**`ext-workspace-v1` vs. Hyprland IPC for special workspaces (already decided).** Special workspaces are
sourced from the same `ext_workspace_handle_v1` stream numbered workspaces already come from (§4), not a new
Hyprland-IPC query. Rationale: `HyprlandWorkspaceService` exists specifically to backfill data the Wayland
protocol doesn't carry (occupancy bitmaps, urgent-window-to-workspace resolution) — but name/active/urgent
*are* already carried per-handle by `ext-workspace-v1` for every workspace, numbered or not, today; the only
bug is that the special ones get silently dropped by the `id`-based routing (§4.1). Reusing the existing pipe
is strictly less code than adding a second `j/...`-style Hyprland IPC query and keeps the
protocol/IPC split (§1) clean — `HyprlandWorkspaceService` still only ever touches the three orthogonal
overlay setters, never `rows_`/`special_rows_` directly.

**`Q_INVOKABLE`/`QVariantList` vs. a second `QAbstractListModel` for specials (this design's call).** See
§4.2's full rationale — the existing `QAbstractListModel` surface on `WorkspaceModel` is currently dead code
from the QML side (nothing binds `Repeater.model` to `WorkspaceModel` directly), while the `Q_INVOKABLE`
function pattern is the one actually exercised throughout `WorkspaceSection.qml` today (`overflowWorkspaceId()`
et al.). Matching the established, exercised pattern was preferred over introducing a second list-model
machinery that would only be the second consumer of `roleNames()`/`rowCount()`/`data()` in the entire file.

**Edge-arrow urgent-jump is symmetric (confirmed with spec owner).** An earlier draft of this design followed
REQ-F-004's original ACs literally, which were asymmetric (only the right arrow urgent-jumped). The spec
owner confirmed the intent is actually symmetric — both arrows urgent-jump to the nearest urgent workspace
beyond their respective edge when one exists, otherwise pan by one step. SPEC.md REQ-F-004 ACs 3, 5, 6 were
revised accordingly (AC6 added for left-arrow urgent-jump). `WorkspaceEdgeArrow.urgent` is now driven by
`root.leftUrgentBefore`/`root.rightUrgentBeyond` symmetrically (§5.3), backed by the new
`hasUrgentBefore`/`lastUrgentIdBefore` model helpers (§6.1) mirroring `hasUrgentBeyond`/`firstUrgentIdBeyond`.
Note the left arrow's *visibility* rule stays asymmetric on purpose (per the earlier grilling decision,
SPEC.md "Left Arrow Visibility"): it shows whenever `window_start > 1`, regardless of occupancy/urgency,
since workspace 1 is always a meaningful hard floor — only the right arrow's *visibility* is gated on
occupied/urgent existing beyond the edge. Only *click behavior* (pan vs. urgent-jump) is now symmetric.

**No text label on edge arrows.** The deleted `overflowUrgentWorkspaceLabel()` rendered `"N"`/`"+N"` text on
the old urgent-overflow pill. REQ-F-004's styling table specifies only color and glow for arrows, no text
requirement; `WorkspaceEdgeArrow` is a bare chevron. Clicking it (urgent case) still activates and focuses
the actual urgent workspace, so the destination is discoverable by the resulting pill highlight, not by a
pre-click label.

**Loader-driven instant removal vs. true exit-transition fade for the separator/dots (this design's call).**
REQ-F-001/REQ-F-006 require non-existence in the DOM when absent (not `opacity: 0`), which `Loader{active:...}`
and `Repeater` model-shrinkage both genuinely satisfy. But REQ-F-006 AC5 also asks for a "smooth" disappearance
("fade-out... or layout animation"). A true per-item fade-out on removal needs `Repeater`/`ListView` `remove:`
transitions wired to a `DelayRemove`-style component, which is materially more complex for a Repeater (as
opposed to `ListView`, which supports it natively) and not justified by the spec's own "or layout animation"
escape hatch. This design accepts instant disappearance plus a `Row { move: Transition {...} }` (§5.3) that
smoothly slides the remaining siblings into their reclaimed space — satisfying the "or layout animation"
alternative explicitly offered by the AC wording, not the fade-out branch.

---

## 10. Known Risks

**REQ-C-002 (empirical) — special workspace protocol support.** This entire §4 design assumes Hyprland's
`ext-workspace-v1` implementation actually emits non-numeric-named `ext_workspace_handle_v1` objects for
`special:*` workspaces with usable active/urgent bits. This has not been checked against a live compositor.
If Hyprland omits special workspaces from `ext-workspace-v1` entirely (some compositors only expose "real"
output-bound workspaces through this protocol and treat scratch/special workspaces as an implementation
detail), `specialWorkspaceList()` will simply always be empty and §4/§5.2/§6 (REQ-F-005/006) silently never
activate — no crash, but a fully dark feature. Must be verified per REQ-C-002/REQ-C-004 before sign-off;
if false, this whole sub-feature needs a Hyprland-IPC-based fallback (`hyprctl activeworkspace`/`workspaces`
already report special workspaces by name in their JSON — `HyprlandWorkspaceService`'s existing parsers in
`libs/holonight-core/src/HyprlandIpc.{h,cpp}` would be the natural extension point, deliberately *not*
designed here since REQ-C-002 explicitly defers that decision pending the empirical result).

**REQ-C-003 (empirical) — per-monitor active-workspace distinction.** §2's entire design rests on the
assumption that Hyprland's `ext-workspace-v1` implementation sets the `Active` bit independently per output
group rather than globally (i.e., that two workspaces on two different monitors can simultaneously both
report `Active` in their `ext_workspace_handle_v1_state`). If Hyprland instead only ever marks a single
workspace `Active` compositor-wide (mirroring the already-existing global `focused_workspace_id_` semantics),
`activeWorkspaceForMonitor()` would return `0` for every monitor except whichever one currently has input
focus — silently collapsing REQ-F-003's per-monitor centering back to global-focus-only behavior on all
*other* monitors. Must be verified per REQ-C-003/REQ-C-004 with a live ≥2-monitor session before sign-off.

**`wl_output*`/`QScreen` mapping race (new risk identified in this design, §2.3).** Mitigated by resolving
`monitorNames()` lazily on every `done()` event rather than caching at `output_enter` time — but if a
monitor is hot-plugged and its `QScreen` genuinely never maps (broken EDID, compositor bug), `monitorNames()`
permanently returns an empty list for that group and `activeWorkspaceForMonitor()` permanently returns 0 for
that monitor's bar, with no retry/backoff distinct from the next incidental `done()` event. Low severity
(the existing `PerMonitorLayerManager` already depends on `QScreen` existing to create a bar surface at all,
so a monitor with no `QScreen` has no bar to display this in anyway), noted for completeness.

**Urgent-bit priority hides per-monitor "active" status for an urgent active workspace (new risk identified
in this design, §2.4).** `ExtWorkspaceHandle::ext_workspace_handle_v1_state` already collapses simultaneous
Active+Urgent protocol bits to `WorkspaceState::Urgent` only (`ExtWorkspaceManager.cpp:30-36`, pre-existing
behavior, unchanged by this design). Consequence: if the workspace currently displayed on a monitor somehow
also carries the urgent bit (e.g., a background process flags urgency on the workspace the user is already
looking at), `activeWorkspaceForMonitor()` for that monitor returns `0` instead of that workspace's id,
because `entry.state` reads `Urgent`, not `Active`. Likely rare in practice (urgent is normally raised on a
workspace the user is *not* currently viewing) but not impossible. Out of this design's scope to fix (would
require splitting the single `WorkspaceState state` field into independent `active`/`urgent` booleans on
`WorkspaceEntry`, a larger refactor touching `effectiveState()` and every consumer) — flagged for a future
iteration rather than silently accepted.

**Animation jank if `stripCount` is miscalculated (new risk identified in this design, §3.2).** If
`maxWorkspaceId()` undercounts (e.g., a workspace exists in `ext-workspace-v1` state but momentarily isn't
yet in `rows_` due to batching timing) at the exact moment a large `windowStart` jump is triggered, the strip
could be too narrow and either clip mid-pill or show a one-frame pop before the next `done()` event widens it.
Low practical impact since `ext_workspace_manager_v1_done()` fires once per logical batch (the protocol's
`done` event itself exists to mark "a consistent batch is now applied"), so `rows_` and any subsequent
`windowStart` recompute (driven by `activeWorkspaceForMonitor`, itself only changes after the same `done()`)
are inherently consistent at the moment QML re-evaluates — flagged for completeness, not expected to manifest.

**Spec-internal tension, REQ-F-002 AC1 vs. REQ-F-003 (already discussed in §9).** Worth a one-line
confirmation from spec owner that the "exactly N instantiated" AC is superseded by the sliding-strip design,
rather than silently resolving it unilaterally in code.

---

## 11. Test / Verification Strategy

### 11.1 C++ unit tests (`tests/test_workspace_model.cpp`, GTest, `ctest -R test_holonight_*`)

Fully unit-testable, no Wayland/Qt-GUI dependency required (the `WorkspaceModel` class itself has none):

- **Window-centering math** is implemented in QML (§3.3), not C++ — it's pure arithmetic on
  `activeWorkspaceId`/`displayCount`/`manualPanOffset`, no model state. It is testable via the QML harness
  (§11.2), not GTest.
- `activeWorkspaceForMonitor(QString)` — seed `rows_` via `applyBatchUpdate` with `WorkspaceEntry` literals
  that set `.state = WorkspaceState::Active` and `.monitor_names = {"DP-3"}` / `{"HDMI-1"}` on different
  entries; assert each monitor name resolves to its own id, an unknown monitor name resolves to 0, and (per
  §10's flagged risk) an entry with `.state = WorkspaceState::Urgent` and a matching `monitor_names` is
  **not** returned by `activeWorkspaceForMonitor` (documents the known limitation as a passing, intentional
  test rather than a silent gap).
- `maxWorkspaceId()` — empty model returns 0; mixed special/numbered rows ignore specials' lack of a
  meaningful numeric id appropriately (specials never enter `rows_` at all post-§4.1, so no special-case
  logic needed — the test should simply confirm specials routed via `applySpecialWorkspaces` don't affect
  `maxWorkspaceId()`).
- `hasOccupiedOrUrgentBeyond(int)` / `hasUrgentBeyond(int)` / `firstUrgentIdBeyond(int)` — directly port the
  existing `OverflowZeroWhenOffMonitor`/`OverflowReturnedWhenOnMonitor`/`OverflowWorkspaceStateTracks...`-style
  tests in `tests/test_workspace_model.cpp` to the new predicate signatures (boundary at exactly `minId`,
  `minId - 1` excluded, multiple urgents beyond `minId` returns the lowest id, no match returns
  `false`/`0` as documented).
- `hasUrgentBefore(int)` / `lastUrgentIdBefore(int)` — mirror image of the above for the left edge (boundary
  at exactly `maxIdExclusive - 1` included, `maxIdExclusive` itself excluded, multiple urgents below
  `maxIdExclusive` returns the highest/nearest id, no match returns `false`/`0`).
- `specialWorkspaceList()` / `applySpecialWorkspaces()` — verify a `SpecialWorkspaceEntry` round-trips through
  to the exact `QVariantMap` shape (`name`/`active`/`urgent` keys) consumers will read, and that
  `applySpecialWorkspaces` is a revision-bump no-op when called twice with identical content (the
  `operator==` short-circuit, §4.2).
- `activateSpecialWorkspace(QString)` — `QSignalSpy` on `activateSpecialWorkspaceRequested`, same pattern
  `WorkspaceModelTest` already uses for `activateWorkspaceRequested` elsewhere in this file.
- **Update/replace, don't leave dangling:** `tests/test_workspace_model.cpp`'s existing
  `OverflowZeroWhenOffMonitor`/`OverflowReturnedWhenOnMonitor`/`OverflowWorkspaceStateTracksPresentAndAbsentOverflow`
  and any sibling overflow-urgent tests call the six deleted methods directly (§6) — these must be deleted or
  rewritten alongside the production code change, not left to bit-rot as compile failures.
  `tests/test_integration_workspace_config.cpp:87-106` (`overflowWorkspaceId()` against `displayCount`
  changes) needs the same treatment, re-pointed at `hasOccupiedOrUrgentBeyond(displayCount + 1)`.
- `ExtWorkspaceHandle`/`ExtWorkspaceGroup` are not directly unit-testable in isolation (they wrap
  `QtWayland::ext_workspace_*_v1` generated protocol classes requiring a live `wl_display` connection) —
  the `is_special` flag-setting logic in `ext_workspace_handle_v1_name` (§4.1) is simple enough
  (`name.toInt(&parsed_ok)`) that it's reasonable to leave covered only by the live verification (§11.3)
  plus a close code-review read, consistent with how this file is tested today (no existing unit tests
  target `ExtWorkspaceManager.cpp` directly — it's all integration-tested via `test_integration_workspace_config.cpp`
  driving the real `ConfigService`+`WorkspaceModel` pair, never a live Wayland socket).

### 11.2 QML test harness (`tests/qml/tst_*.qml`, `FakeQmlServices`, `ctest -R test_holonight_qml_harness -V`)

`FakeQmlServices` (`tests/FakeQmlServices.h:644-704`) registers the **real** `WorkspaceModel` C++ class as the
`HolonightShell`/`WorkspaceModel` singleton (not a fake/mock) — so all the new `Q_INVOKABLE`s above are
directly callable from QML tests with no extra mock wiring. Add `tests/qml/tst_WorkspaceSection.qml` and/or
`tst_WorkspacePillStrip.qml` (none exist today for this directory) covering:

- Window-centering arithmetic (§3.3) end-to-end: seed `WorkspaceModel` (via the singleton instance the
  harness exposes) with `monitor_names` for a fake `"TEST-1"`, set active to various ids, assert
  `windowStart` lands where the worked examples in REQ-F-003 specify (active=1 → `windowStart==1`; active=4
  with displayCount=5 → `windowStart==2`; etc.) by reading the property off the instantiated
  `WorkspaceSection` item.
- Manual pan-offset survives incidental revision bumps (§3.5): set `manualPanOffset` via a simulated arrow
  click, then trigger an unrelated `WorkspaceModel.applySpecialWorkspaces([...])` or
  `setOccupiedWorkspaceIds(...)` call (bumps `revisionChanged` without changing this monitor's active id),
  assert `manualPanOffset` is unchanged; then change the active workspace and assert it resets to 0.
- Edge-arrow visibility predicates (§6.1) wired to actual `Loader.active` state for both directions, and the
  urgent-vs-pan click dispatch branch for **both** arrows symmetrically (assert `activateWorkspaceRequested`
  fires with the correct nearest-urgent id for the urgent case on each side, `manualPanOffset` changes and
  nothing fires for the non-urgent case on each side).
- Special-workspace dot count/state mirrors `specialWorkspaceList()` exactly, including the
  zero-specials → zero-dots-and-no-separator-instantiated case (REQ-F-001/REQ-F-006) — assertable via
  `findChild`/`children.length` returning nothing for the `Loader`'s `item`, not just `opacity`/`visible`
  checks, to actually validate the "not in DOM" requirement rather than a weaker visual proxy.
- `WorkspacePill.qml`'s `FocusedInactiveMonitor` styling (§8) — `border.width === 0`,
  `fill === HoloniightPalette.workspaceOccupied`, dot child present/visible only in that state.
- Palette-only grep check (REQ-NF-003) is better done as a `task qml-lint`/CI grep step than a QML
  `TestCase`, but `borderUrgent`/`error` absence in `WorkspacePill.qml` (REQ-F-007 AC1) can additionally be
  asserted by reading the live `_style.borderColor`/`textColor` values in the `Urgent` state and comparing
  against `HoloniightPalette.accentViolet` directly, which is a stronger behavioral check than a text grep.

### 11.3 Live-only verification (cannot be simulated in `FakeQmlServices`)

`FakeQmlServices` registers a real `WorkspaceModel` but there is no fake `ExtWorkspaceManager` — the Wayland
protocol layer itself, and therefore both REQ-C-002 and REQ-C-003, can only be exercised against a real
compositor. Per this project's documented live-testing playbook (`CLAUDE.md`):

- Drive workspace switches via `hyprctl dispatch workspace N` / `hyprctl dispatch movetoworkspace
  special:scratch` directly (not by clicking through the shell UI — see the project's "never drive shell UI
  programmatically" manual-testing-protocol convention) and observe the bar via `grim -g "<x>,<y> <w>x<h>"`
  screenshots (logical coordinates, remember per-output scale) on the **focused** monitor.
- `task compositor-smoke-check` is the documented entry point for compositor-facing changes affecting popup
  positioning/bar behavior; run it as part of this feature's smoke pass even though it doesn't have
  workspace-indicator-specific checks baked in today — it at minimum validates the harness/prerequisites
  haven't regressed.
- REQ-C-002 verification: create a special workspace (`hyprctl dispatch movetoworkspace special:scratch`),
  confirm a dot appears with zero code changes needed to *see* it appear (if it doesn't, that's the
  REQ-C-002 risk materializing, §10) — then toggle it (`hyprctl dispatch togglespecialworkspace scratch`)
  and confirm Hidden ↔ Active dot-radius/color transitions.
- REQ-C-003 verification: a genuine ≥2-monitor session (or `wlr-randr`-simulated outputs if hardware isn't
  available), switch workspaces independently per monitor via `hyprctl dispatch workspace N` while focused
  on each output in turn (`hyprctl dispatch focusmonitor <name>` first), confirm each bar's `windowStart`
  centers independently and a pill click/edge-arrow pan on one monitor's bar never perturbs the other's.
- Both findings get written to `docs/sdd/workspace-indicator-redesign/VERIFICATION.md` per REQ-C-004 — this
  design does not produce that file; it's an implementation/verification-phase deliverable, listed here only
  to confirm the design doesn't block it (no missing API/hook needed for the verifier to observe the
  required state).

---

## 12. Summary of File-Level Changes

- `libs/holonight-core/src/WorkspaceModel.h` / `.cpp` — add `SpecialWorkspaceEntry`, `is_special`/`monitor_names`
  on `WorkspaceEntry`, `activeWorkspaceForMonitor`, `maxWorkspaceId`, `hasOccupiedOrUrgentBeyond`,
  `hasUrgentBeyond`, `firstUrgentIdBeyond`, `hasUrgentBefore`, `lastUrgentIdBefore`, `specialWorkspaceList`,
  `activateSpecialWorkspace`, `applySpecialWorkspaces`, `activateSpecialWorkspaceRequested` signal; remove the
  six overflow* methods.
- `libs/holonight-core/src/ExtWorkspaceManager.h` / `.cpp` — `ExtWorkspaceGroup::outputs_`/`monitorNames()`
  replacing `output_count_`; `ExtWorkspaceHandle::ext_workspace_handle_v1_name` sets `is_special` explicitly;
  `ext_workspace_manager_v1_done()` builds and dispatches both `entries` and `specials`; new `<QGuiApplication>`/
  `<QScreen>` includes.
- `apps/shell/qml/Topbar/WorkspaceSection.qml` — rewritten (§5.3).
- `apps/shell/qml/Topbar/WorkspacePill.qml` — `_style` color/border-width/text-color changes (§7, §8) + new
  top-center dot.
- `apps/shell/qml/Topbar/WorkspacePillStrip.qml`, `WorkspaceEdgeArrow.qml`, `SpecialWorkspaceDot.qml` — new.
- `tests/test_workspace_model.cpp` — remove overflow-method tests, add tests per §11.1.
- `tests/test_integration_workspace_config.cpp` — re-point the one `overflowWorkspaceId()` reference (line 98/106)
  at `hasOccupiedOrUrgentBeyond`.
- `tests/qml/tst_WorkspaceSection.qml` (new) — per §11.2.
- `docs/sdd/workspace-indicator-redesign/VERIFICATION.md` — produced during implementation/verification phase
  per REQ-C-004, not part of this design.

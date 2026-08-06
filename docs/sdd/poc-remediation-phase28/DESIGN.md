# Phase 28 — Per-Monitor View Lookup: Design

**Input**: `poc-remediation-phase28/SPEC.md`
**Baseline**: Phase 27 accepted in `471209e`.
**Status**: Complete — implementation, automated validation, and user verification passed.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `libs/holonight-surfaces/src/PerMonitorLayerManager.{h,cpp}`, `libs/holonight-surfaces/src/WidgetManager.{h,cpp}` | Existing widget and surfaces regression suite |

## 2. Design Decisions

### 2.1 Index the shared lifecycle owner

Every per-monitor manager already owns the authoritative `QScreen*` to surface
map. A `QHash<QString, QScreen*>` beside that map allows name lookup without a
linear scan, then retrieves the owned view by its existing screen key.

### 2.2 Synchronize at successful creation and teardown

The index is written only after QML has loaded and the `MonitorSurface` is in
the authoritative map. On removal, the name is erased before the surface and
screen teardown, preventing a retained stale pointer.

### 2.3 Preserve the nullable lookup contract

The base helper returns `nullptr` for a missing name, exactly matching the
former `WidgetManager` behavior. No occupancy, visibility, or timer policy is
altered.

## 3. Risks and Boundaries

- `QScreen::name()` is the existing monitor identity used by widget occupancy
  events and configuration.
- The change does not alter ownership or the documented LayerSurface-before-view
  destruction order.

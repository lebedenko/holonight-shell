# Phase 15 — Active Window State Hygiene: Design

**Input**: `poc-remediation-phase15/SPEC.md`
**Baseline**: Phase 14 accepted in `af590d6`.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `ActiveWindowService.{h,cpp}` snapshot reconciliation | active-window service tests |
| F-02 | `ActiveWindowService.{h,cpp}` API cleanup | build plus focused service tests |
| F-03 | `ActiveWindowService.cpp` internal desktop-entry helper | active-window service tests |

## 2. Design Decisions

### 2.1 Reconcile only after a complete monitor snapshot

`applyActiveWindowSnapshot()` already receives the authoritative monitor map.
After updating live monitors, it will identify state keys absent from that map,
remove their window/workspace entries, and add them to the existing change
lists. `focused_monitor_name` will clear only when the authoritative snapshot
does not contain it. Failed monitor responses never reach this function, so
they retain the current state.

### 2.2 Retire the duplicate accessor without renaming QML API

`focusedMonitorName()` is the explicit `Q_INVOKABLE` and will remain the
canonical interface. Repository usage is re-grepped before editing; if
`focusedMonitor()` has no callers, both its declaration and implementation are
removed. The `focusedMonitor` Q_PROPERTY keeps its current externally visible
name by reading `focusedMonitorName()` directly.

### 2.3 Extract a narrowly typed desktop-entry reader

Use a narrowly scoped parser over desktop-entry text, with a file reader as
its only adapter. It finds `[Desktop Entry]` and makes each first-`=` split
available to the category and matching paths; deterministic tests exercise the
same parser. The existing paths retain their category-selection decisions,
avoiding a general desktop-file framework or behavior-changing data model.

## 3. Risks and Boundaries

- A monitor can disappear between an event and a snapshot. Snapshot
  reconciliation is authoritative and must emit enough existing signals for
  QML consumers to clear stale rows.
- A desktop file can contain multiple sections or keys with `=` in the value;
  the helper must preserve the existing first-section and split behavior.
- Removing a C++ method can be source-incompatible for out-of-repository users;
  this personal shell exposes the service through its checked-in QML module,
  so repository usage is the compatibility boundary for this phase.

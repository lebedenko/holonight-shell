# Phase 29 — Tray Item Membership Index: Design

**Input**: `poc-remediation-phase29/SPEC.md`
**Baseline**: Phase 28 accepted in `e230597`.
**Status**: Complete — implementation, automated validation, and user verification passed.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `libs/holonight-surfaces/src/TrayWatcher.{h,cpp}` | `tests/test_tray_watcher.cpp` |

## 2. Design Decisions

### 2.1 Retain the ordered D-Bus representation

`QStringList` continues to hold registered keys in arrival order because it is
returned through `RegisteredStatusNotifierItems` and supports ordered
service-removal reporting.

### 2.2 Add a synchronized set for membership

`QSet<QString>` records the same normalized keys. Registration checks and
inserts the set alongside the list; removal removes the set entry before
updating the list and per-service count. This gives direct lookup to the
high-frequency asynchronous completion guards without changing external data.

### 2.3 Test the synchronization contract

Existing registration, individual removal, and service-removal tests now assert
both representations. They cover duplicate suppression, visible list order,
and that no stale membership remains after removal.

## 3. Risks and Boundaries

- The two containers must remain synchronized; their mutation is centralized in
  the existing registration helpers.
- Service-wide removal still walks the ordered list to preserve its existing
  result order; it is not on the asynchronous property-fetch completion path.

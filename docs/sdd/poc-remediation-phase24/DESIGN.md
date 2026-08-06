# Phase 24 — Precise Workspace Model Role Notifications: Design

**Input**: `poc-remediation-phase24/SPEC.md`
**Baseline**: Phase 23 accepted in `c12f9dc`.
**Status**: Complete — implementation, automated validation, and user verification passed.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `libs/holonight-core/src/WorkspaceModel.cpp` | `tests/test_workspace_model.cpp` |

## 2. Design Decisions

### 2.1 Pass roles through the existing notification helper

`emitRowsChanged()` will accept a role list and forward it to Qt's existing
three-argument `dataChanged` signal. This keeps the row-range guard in one
place and avoids duplicating model-notification code across callers.

### 2.2 Separate state-only and snapshot updates

The model's focused-workspace, occupied-ID, and urgent-ID paths change only
the computed `wsState` value, so they emit `WorkspaceStateRole`. A batch
replacement may alter every stored field while retaining the same row count;
it therefore explicitly lists all four roles.

### 2.3 Assert the public signal contract

Tests inspect the role-list argument of the public Qt model signal rather
than the private helper. This protects the QML-observable invalidation
contract while permitting internal refactoring.

## 3. Risks and Boundaries

- An incomplete role list can leave QML bindings stale; the batch path must
  retain all four roles.
- The phase deliberately does not attempt per-row comparison, which would add
  complexity beyond this low-severity notification refinement.

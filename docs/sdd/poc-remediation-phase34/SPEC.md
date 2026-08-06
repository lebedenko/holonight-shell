# Phase 34 — Launcher Results Accessor Constness

**Status**: Complete — automated checks and user verification passed.

## Objective

Remediate U-06 I-01: make `LauncherService::results()` a const QML property
reader while preserving the stable `QAbstractItemModel*` exposed to launcher
views.

| Source | Phase 34 item | Impact |
|---|---|---|
| U-06 I-01 | Make the `results` property reader const | The QML-facing model accessor follows the service's const-reader convention without changing the model instance or launcher behavior. |

## Functional Requirements

### REQ-F-01 — Expose results through a const property reader

`LauncherService::results()` shall be declared `const` while continuing to
return the same `QAbstractItemModel*` that backs `LauncherService.results`.

- The `Q_PROPERTY` declaration and QML property name remain unchanged.
- The returned pointer continues to refer to the service-owned `LauncherModel`.
- Querying the property must not mutate launcher state.
- Existing QML bindings and C++ callers retain source and runtime behavior.

**Acceptance**: `LauncherService.results` remains usable as the model for both
launcher lists, and the accessor matches the const qualification of its sibling
property readers.

## Constraints and Verification

- Keep the change local to `LauncherService`; do not change `LauncherModel`,
  QML bindings, property notifications, or model ownership.
- Use the existing `NetworkService::wifiNetworks() const` reader as the
  in-repository precedent for exposing a Qt model pointer from a const QML
  reader.
- Retain existing focused launcher-service coverage; add a test only if the
  compile or behavior review exposes an observable coverage gap.
- Run focused launcher tests, `task test`, `task architecture-check`,
  `task format-check`, and `git diff --check`.

## Out of Scope

- Changing launcher result ordering, selection, scanning, caching, or QML UI.
- Altering the `LauncherModel` API or model mutability contract.
- Other queued Phase 7 Low-severity candidates.

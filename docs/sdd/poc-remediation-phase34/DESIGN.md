# Phase 34 — Launcher Results Accessor Constness: Design

**Input**: `poc-remediation-phase34/SPEC.md`
**Baseline**: Phase 33 accepted in `c5942cc`.
**Status**: Complete — automated checks and user verification passed.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | Declare `LauncherService::results()` const and return its owned `LauncherModel` through the existing `QAbstractItemModel*` API | Existing launcher-service tests and QML bindings continue to access the same model pointer |

## 2. Design Decisions

### 2.1 Preserve the established mutable-model pointer contract

The property intentionally exposes a `QAbstractItemModel*` for QML to observe;
const qualification applies to the `LauncherService` reader, not to the model
object it owns. Because `model_` is value-owned, the inline reader can use a
local `const_cast` to preserve the existing mutable-pointer API. The related
`NetworkService::wifiNetworks() const` reader establishes the in-repository
precedent for a const QML reader that exposes a model pointer.

### 2.2 Keep the cleanup header-local

`results()` is an inline reader and has no state mutation. Qualifying it const
does not require a `LauncherModel` change, a notification change, or QML edits.
This keeps the correction narrow and avoids conflating API consistency with
launcher behavior changes.

### 2.3 Rely on observable launcher coverage

Existing launcher-service tests read data through `service.results()`, and the
two launcher QML lists bind to `LauncherService.results`. A focused rebuild and
test run establish that the const reader preserves both C++ and QML-facing model
access; an implementation-detail test is unnecessary unless a gap is found.

## 3. Risks and Boundaries

- Returning a pointer to a temporary or changing ownership would break QML
  model bindings; the phase returns the existing service-owned `model_` only.
- Altering the property declaration or pointer type could change Qt's
  meta-object API; neither is in scope.
- This is a source-constness cleanup with no intended live-compositor behavior
  change, so focused automated verification is sufficient.

# Phase 14 — Surface and Lifecycle Reliability: Design

**Input**: `poc-remediation-phase14/SPEC.md`
**Baseline**: Phase 13 accepted in `c7be4e6`.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `TrayModel.{h,cpp}` | `tests/test_tray_model.cpp` |
| F-02 | surface classes that create `QQuickView` | focused surfaces tests and existing valid-load coverage |
| F-03 | `network/NetworkManagerBackend.{h,cpp}` | network backend/service tests with controllable futures |
| F-04 | `launcher/LauncherService.{h,cpp}`, `Launcher.qml` only if needed | launcher service and QML launcher tests |

## 2. Design Decisions

### 2.1 Keep tray image access as a small read/write critical section

Add a private read/write lock guarding the `rows_` and `index_by_key_` state
that participates in `imageForKey()`. Model mutations update these containers
under a write lock, while image lookup copies the selected `QImage` under a
read lock and returns that value after unlocking. Qt model begin/end signals,
menu calls, and logging stay outside the locked section.

This satisfies the `QQuickImageProvider` reentrancy contract without moving
the model itself off the GUI thread or changing image URLs. A snapshot is
preferable to holding a lock through image-provider work because `QImage` is
implicitly shared and callers need no live model data after lookup.

### 2.2 Use one internal QML-load validation helper

The affected surface classes need the same post-`setSource()` rule. Add a
small internal surfaces helper that receives a `QQuickView` and source URL,
checks for `QQuickView::Error`, logs the URL plus `errors()`, and returns a
boolean. Each creation path invokes it immediately after `setSource()` and
uses its established cleanup/failure branch before `wl_surface_commit` or
open-state assignment.

QRC components are synchronously ready in the supported path, so this phase
does not add `statusChanged` lifecycle machinery. `Loading` is treated as a
programming/configuration error for these local resources and logged with the
same source context rather than committing a half-initialized surface.

### 2.3 Make NetworkManager shutdown non-reentrant

Introduce a private shutdown guard set at the start of the destructor. Paths
that would queue `refresh()` after an operation completion check this guard and
return. Stop the poll timer, disconnect watcher callbacks that target the
backend, cancel each live watcher, and use its completion wait primitive
without `QCoreApplication::processEvents()`.

Operate on the live watcher collection only while no event loop is pumped; do
not retain a raw-pointer snapshot. Completion bookkeeping is disconnected or
made shutdown-aware before object teardown, and queued backend events are
removed only after all futures have finished. This avoids both the stale-copy
use-after-free and a late refresh re-arming `watcher_`.

### 2.4 Restore launcher selection by semantic identity

Before a model reset, record whether the selection is an application row or an
action row, along with the owning desktop-file identity and action identity.
After the reset, search the rebuilt results for that identity and restore its
index. If the exact row is absent, clamp the prior index to the new range; an
empty result set remains `-1`.

The restore path is used only for model rebuilds that preserve the active
query/category. Explicit query/category updates continue to select the first
result as they do today. `Launcher.qml` relies on its existing binding from
`selectedIndex` to `ListView.currentIndex`, so the restored logical row is
kept visible instead of a reset selecting row zero. It deliberately does not
restore a stale `contentY` pixel value across changed result heights.

## 3. Test Strategy

| Area | Deterministic coverage |
|---|---|
| Tray | request images while scripted model updates run; assert no invalid access and returned image dimensions/colors are from a complete row snapshot |
| QML load | valid local QRC source remains Ready; invalid injectable source logs/returns failure before surface commit |
| Network teardown | complete two operation futures in teardown order; assert no event-loop pumping/re-arm and no callbacks reach destroyed state |
| Launcher service | preserve a selected app/action across a rebuild; clamp predictably when removed; preserve existing query-reset behavior |
| Launcher QML | selected non-first row remains current and visible after the service rebuild |
| Live shell | tray icons update while applications appear/disappear; launcher selection survives a rescan; injected bad source is diagnostic-only and leaves no ghost surface |

## 4. Risks and Boundaries

- Locks must not span model signals: Qt item-model observers can synchronously
  re-enter model APIs on the GUI thread.
- Surface construction has different ownership shapes. The helper centralizes
  diagnosis only; each caller retains its existing cleanup order, particularly
  layer-surface-before-view ownership.
- Blocking for a future during destruction can lengthen shutdown. It is still
  safer than nested event processing; cancellation remains the first action.
- Stable launcher identity must distinguish an app row from an action row to
  avoid restoring a selection to the parent application's default action.

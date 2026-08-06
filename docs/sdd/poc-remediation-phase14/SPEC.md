# Phase 14 — Surface and Lifecycle Reliability

**Status**: Complete — implementation and live acceptance passed.

## Objective

Remediate the four medium-severity Confirmed items still queued from the Phase
7 triage. This is a focused reliability tranche: it makes the tray image
provider reentrant, makes QML load failures observable, removes reentrant
NetworkManager teardown, and preserves launcher navigation across a background
result rebuild.

| Source | Phase 14 item | User or system impact |
|---|---|---|
| U-02 I-01 | Tray image/model synchronization | `QQuickImageProvider` must be safe when Qt invokes it off the GUI thread. |
| U-02 I-04 | QQuickView source-load validation | Broken QML resources must not create a committed but unusable layer surface. |
| U-04 I-01 | NetworkManager shutdown reentrancy | Backend destruction must not iterate freed operation watchers. |
| U-06 I-02 | Launcher rescan state preservation | A background rebuild must not throw navigation back to the first result. |

The Phase 7 triage remains the source of finding rationale and priority:
`docs/sdd/poc-remediation-phase7/REPORT.md` §3.

## Functional Requirements

### REQ-F-01 — Tray image reads are reentrant

`TrayImageProvider::requestImage()` shall be safe when called concurrently
with tray model updates.

- Access to the model state used to resolve an image shall be synchronized.
- `requestImage()` shall return a value snapshot; it shall not expose a row or
  container reference after releasing synchronization.
- GUI-thread model notifications and D-Bus menu interaction remain on their
  existing threads and shall not be emitted while the image-state lock is held.

**Acceptance**: deterministic unit coverage overlaps image requests with
add/update/remove activity and verifies returned images are valid snapshots,
with ThreadSanitizer-friendly synchronization boundaries where available.

### REQ-F-02 — Layer-surface QML loading fails closed and visibly

Every layer-surface path that calls `QQuickView::setSource()` shall check the
resulting `QQuickView` status before committing or marking its surface usable.

- On `QQuickView::Error`, the implementation shall log the source URL and
  every QML error, clean up the just-created view/surface, and return its
  existing failure result (or otherwise leave no usable-surface state).
- Successful QRC loading preserves existing layer-shell creation, geometry,
  visibility, and initial-property behavior.
- This applies to the paths enumerated by Phase 7: per-monitor surfaces,
  status popup, tooltip, launcher, tray menu/dismiss overlay, notifications,
  and sidebar.

**Acceptance**: focused surfaces tests inject an invalid QML URL into the
shared loading seam and prove no protocol commit/active-surface state follows;
existing valid-source tests continue to pass.

### REQ-F-03 — NetworkManager teardown cannot re-enter itself

`QtNetworkManagerBackend` destruction shall cancel and finish outstanding
query and operation futures without pumping the application event loop.

- Teardown shall first prevent queued refresh work from starting new work.
- It shall not retain or dereference a stale copy of operation-watcher raw
  pointers while completion handlers may remove and defer-delete them.
- Outstanding work shall be cancelled and awaited before QObject-owned
  watchers are destroyed; no operation completion may schedule a refresh once
  shutdown begins.

**Acceptance**: a deterministic backend test exercises multiple controllable
operation futures during destruction and proves that completion cannot create a
new query or access a deleted watcher.

### REQ-F-04 — Launcher rebuilds preserve navigation context

When a `LauncherModel` rebuilds results for an unchanged query/category, the
launcher shall retain the selected logical entry when it still exists.

- Capture selection before model reset using a stable entry/action identity.
- Restore that identity after the reset; if it disappeared, choose the nearest
  valid result, or `-1` only when no results remain.
- The visible list shall follow the restored selection instead of being forced
  to the first row; no raw pixel scroll position is retained when its content
  changed.
- A deliberate query/category change retains its existing first-result
  selection semantics.

**Acceptance**: service and QML coverage prove a background rebuild preserves
a selected non-first entry and its visible navigation context, handles a
removed selected entry predictably, and keeps query-change behavior unchanged.

## Constraints and Verification

- Do not introduce a new threading framework, persistent schema, or public
  D-Bus/QML API solely for this phase.
- Keep layer-shell teardown ordering and deferred deletion conventions from
  `CLAUDE.md` intact.
- Add deterministic C++ and QML coverage before any live shell verification.
- Run `task test`, `task qml-lint`, `task qmltypes-check`,
  `task architecture-check`, `task format-check`, and `git diff --check`.
- Run `task compositor-smoke-check` and perform focused live checks for tray
  icons, failed QML-source diagnostics, and launcher rescan behavior.

## Out of Scope

- The 61 remaining Low-severity Phase 7 Confirmed findings.
- A generic asynchronous QML-loading framework or runtime hot-reload support.
- Broader NetworkManager polling, D-Bus error-message, and monitor-pruning
  changes outside teardown safety.
- The separately deferred logging redesign, including Phase 11 launcher-hide
  logging noise.

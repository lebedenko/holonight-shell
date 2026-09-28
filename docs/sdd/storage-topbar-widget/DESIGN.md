# Storage Topbar Widget + Popup Redesign — Design

Implements the requirements in `SPEC.md` (REQ-F-001..021, REQ-NF-001..003, REQ-C-001..004). This
document describes HOW, not WHAT — every requirement in SPEC.md is treated as fixed.

---

## 1. Components

### 1.1 C++ — `libs/holonight-services/src/storage/`

| File | Change | Responsibility |
|---|---|---|
| `StorageService.h` | **modify** | New properties (`deviceCount`, `hasMounted`), new roles (`usedBytes`, `totalBytes`, `freeBytes`, `operationText`, `errorText`), new invokables (drive-level lookups, `retry`, `openVolume`, `openInFiles`, `showAllDevices`), new private state (`in_flight_ops_`, `last_errors_`, `pending_open_after_mount_`, `removal_labels_`). |
| `StorageService.cpp` | **modify** | Implements all of the above; extends `refresh()` to populate the new per-row byte/operation/error data; extends the `operationFinished` handler to drive the operation-tracking and error-tracking state machines and the safe-to-remove and device-connected D-Bus notifications; adds a small `driveIconNameFor()` classifier reusing the existing anonymous-namespace free-function pattern (`driveName()`, `volumeName()`, `volumeState()`). |
| `StorageFilter.h` | **unchanged** | REQ-C-004. Not read or modified beyond the existing `#include`. |

No files change under `holonight-system-services` (REQ-C-001).

The shell notification receiver also changes: `NotificationTypes.h` carries the shared storage
category and internal filter-bypass flag; `NotificationServer.cpp` authenticates storage calls
against the shell's unique bus sender; `NotificationService.cpp` applies that bypass and excludes
standard transient notifications from history. This does not change the storage backend API.

### 1.2 QML — Topbar

| File | Change | Responsibility |
|---|---|---|
| `apps/shell/qml/Topbar/StorageWidget.qml` | **modify** | Three-state rendering (hidden / subdued / active+badge) driven by `StorageService.deviceCount` / `hasMounted`. Keeps the existing `BarSection`, `BarTooltipArea`, `StatusPopupTriggerArea` structure — only the icon/badge visuals and width/visibility bindings change. |

No new topbar files — the badge is small enough (a numeral in a pill, following the `NotificationsWidget.qml` badge-dot precedent scaled up to a numeric badge) to stay inline in `StorageWidget.qml`.

### 1.3 QML — Popup (`apps/shell/qml/Popups/Storage/`)

Following this directory's existing flat-per-feature-directory convention (matching `Popups/Audio/*.qml`, `Popups/Network/*.qml` — several sibling `.qml` files directly inside the feature directory, not further nested subdirectories):

| File | Status | Responsibility |
|---|---|---|
| `StoragePopupContent.qml` | **modify** | Root `ColumnLayout`. Header, the `ListView` (drive-grouped), the empty-state swap (REQ-F-020), the footer action row (REQ-F-016/017), and per-drive collapse state. Orchestration only — no business logic. |
| `StorageDriveSection.qml` | **new** | `ListView.section.delegate`. Drive header: icon (`StorageService.driveIconName(section)`), name (`StorageService.driveLabel(section)`), subtitle (`StorageService.driveSubtitle(section)`), total capacity (`StorageService.driveCapacityText(section)`), eject button, power-off button, and the drive-level transient/error text (`StorageService.driveOperationText/driveErrorText(section)`). REQ-F-007. |
| `StorageVolumeCard.qml` | **new** | Per-volume row delegate. Mounted-state indicator, name, capacity bar (mounted-only), mount path, Mount/Unmount buttons, transient operation text, error text + Try Again, and the body-click-to-open handler (REQ-F-018/019). REQ-F-012/013/014. |
| `StorageCapacityBar.qml` | **new** | Reusable bar: takes `usedBytes`/`totalBytes`/`freeBytes`, renders the fill + "X used of Y" label, hard-codes the three threshold colors (REQ-F-010, REQ-C-003). Used only by `StorageVolumeCard.qml`, but split out because the fill-color/threshold logic is a distinct, independently testable unit and keeps `StorageVolumeCard.qml` focused on layout/actions. |

No new empty-state file: `Holonight.Controls`' existing `HnEmptyState` (icon + title + description + optional action, already used by `AudioDeviceList.qml`) is reused directly in `StoragePopupContent.qml` for REQ-F-020 — it already provides exactly the icon/title/subtitle shape the requirement asks for.

No new surface/C++ sizing files. See §4.6 — the existing `StatusPopupGeometry.cpp` policy for `"storage"` already allocates a larger, scrollable (`InternalList`) envelope than any other popup and needs no change.

---

## 2. Data Flow

### 2.1 Model refresh (existing cycle, extended)

`StorageService::refresh()` is unchanged in *what* it filters (REQ-C-004) but each row gains five
new keys, computed in the same pass that builds `rows_`:

- `usedBytes` / `totalBytes` / `freeBytes` (`qint64`, mounted volumes only): `QStorageInfo(volume.mountPoints.first())`, `bytesTotal()`/`bytesAvailable()`/(`bytesTotal()-bytesAvailable()`). `0` for unmounted volumes and for the optical/no-filesystem drive rows appended by `appendOpticalDrives()`.
- `operationText` (`QString`, empty when idle): derived from `in_flight_ops_.value(targetId)` via a `operationTextFor(targetId)` helper (`"Mounting…"`/`"Unmounting…"`/`"Ejecting…"`/`"Powering off…"`).
- `errorText` (`QString`, empty when no error): derived from `last_errors_.value(targetId).message` via an `errorTextFor(targetId)` helper.

`refresh()` is still triggered by the same signals as today (`drives()`/`volumes()` model changes,
`operationStateChanged`, `operationFinished`) plus is now also called synchronously from the new
invokables (§2.2) so REQ-F-012's "within 10ms" text update doesn't wait for a controller round trip.

### 2.2 Mount/Unmount/Eject/PowerOff round trip

```
QML click → StorageService::mount/unmount/eject/powerOff(targetId)
   1. if in_flight_ops_.contains(targetId): return (no-op; dedupes double-clicks, REQ-F-012/018)
   2. last_errors_.remove(targetId)                       // REQ-F-013 point 3
   3. in_flight_ops_.insert(targetId, <Operation>)
   4. refresh()                                           // row's operationText flips immediately
   5. controller_->mount/unmount/eject/powerOff(targetId)  // async, D-Bus under the hood
        ↓ (later, on the controller's own event loop turn)
StorageController::operationFinished(StorageResult result)
   1. in_flight_ops_.remove(result.targetId)
   2. if result.succeeded():
        last_errors_.remove(result.targetId)
        if result.operation == Eject or PowerOff: send D-Bus Notify with the cached drive label (§2.4)
        if result.operation == Mount && pending_open_after_mount_.remove(result.targetId):
            QProcess::startDetached("holonight-files", {result.mountPath})   // REQ-F-018 step 2
      else:
        last_errors_.insert(result.targetId, {result.operation, operationError(result), ++next_error_generation_})
        pending_open_after_mount_.remove(result.targetId)                   // REQ-F-018 step 3: no launch
        QTimer::singleShot(30000, ...) → expire this entry iff its generation is still current (REQ-NF-003)
   3. refresh()
```

Both `in_flight_ops_` and `last_errors_` are keyed by **targetId exactly as passed to the
operation** — a volume id for mount/unmount, a drive id for eject/power-off. Power-off dispatches immediately with the current removal scope;
there is no confirmation state. Removal labels are captured at dispatch and released on completion,
because the backend may remove the drive before reporting success. This is why volume rows expose the two new text roles directly, while
the drive header (which has no row of its own once a drive has volumes — see §4.3) reads the same
underlying maps via invokables keyed by the section's `driveId`.

`StorageController::operationFinished`'s `StorageResult` already carries `targetId`, `operation`,
and (on success) `mountPath` — this is the entire correlation mechanism; no `requestId` bookkeeping
is needed (see §5.2).

### 2.3 Click-to-mount-then-open (REQ-F-018/019)

A single new invokable, `StorageService::openVolume(targetId)`, is called identically by the QML
body-click handler regardless of the volume's mount state:

```
openVolume(targetId):
  volume = controller_->volumes()->find(targetId)
  if volume && !volume->mountPoints.isEmpty():
      QProcess::startDetached("holonight-files", {volume->mountPoints.first()})   // REQ-F-019
      return
  if !visibleTarget(targetId, "canMount") || in_flight_ops_.contains(targetId):
      return                                                                       // dedupe, REQ-F-018 ac#3
  pending_open_after_mount_.insert(targetId)
  mount(targetId)                                                                  // reuses §2.2's guard+tracking
```

`mount()`'s own duplicate-call guard (§2.2 step 1) is what makes "click again while in flight" a
no-op for free — `openVolume()` doesn't need its own separate in-flight check for the mount path,
only the early return above before it *re-enters* `pending_open_after_mount_`.

### 2.4 Transient storage notifications (REQ-F-015/021)

StorageService submits an asynchronous `QDBusMessage` to `org.freedesktop.Notifications.Notify`
with app name `holonight-shell`, the summary, an empty body, a 5000ms timeout, and hints
`transient: true` / `category: "x-holonight.storage"`. No synchronous interface introspection or
Notify call runs on the GUI thread.

The shell owns this D-Bus endpoint during normal operation. Direct transport alone does not
bypass its pipeline. `NotificationServer` grants a filter bypass only when the caller's unique
bus name matches the shell's own session-bus connection, the app/category match, and the
notification is transient. Other clients cannot gain the bypass by copying those hints.
`NotificationService` skips DND and app rules for that trusted call and excludes transient
notifications from history. Normal notifications retain their existing filtering and history.

Eject and power-off success use the label captured before dispatch, even if the drive has
already disappeared. Failures, mount, and unmount remain silent.

Connected notifications compare eligible drive IDs after startup. The initial controller
snapshot fills both models before `availableChanged`; refreshes while unavailable do not seed
the known-ID set. The first available snapshot is seeded silently, and later new IDs notify once.

### 2.5 `deviceCount` / `hasMounted`

Both are pure derived reads over `rows_` (already the fully-filtered, eligible set — REQ-C-004):

```cpp
int StorageService::deviceCount() const {
  QSet<QString> ids;
  for (const auto& row : rows_) ids.insert(row.value("driveId").toString());
  return static_cast<int>(ids.size());
}
bool StorageService::hasMounted() const {
  return std::ranges::any_of(rows_, [](const auto& row){ return row.value("mounted").toBool(); });
}
```

Both reuse the existing `changed()` NOTIFY signal, already emitted at the end of every `refresh()`
call and every other state mutation — no new signal is needed, and the 500ms budget (REQ-F-001/002)
is trivially met since `refresh()` runs synchronously on the signals that already drive it today.

---

## 3. Interfaces / APIs

### 3.1 `StorageService` — new `Q_PROPERTY`

```cpp
Q_PROPERTY(int deviceCount READ deviceCount NOTIFY changed)
Q_PROPERTY(bool hasMounted READ hasMounted NOTIFY changed)
```

### 3.2 `StorageService` — new roles (appended to `enum class Role`, `roleNames()`)

| Role | QML name | Type | Populated for |
|---|---|---|---|
| `UsedBytes` | `usedBytes` | `qint64` | mounted volumes; `0` otherwise |
| `TotalBytes` | `totalBytes` | `qint64` | mounted volumes; `0` otherwise |
| `FreeBytes` | `freeBytes` | `qint64` | mounted volumes; `0` otherwise |
| `OperationText` | `operationText` | `QString` | any row with an in-flight op on its `targetId`; `""` otherwise |
| `ErrorText` | `errorText` | `QString` | any row with a stored error on its `targetId`; `""` otherwise |

### 3.3 `StorageService` — new `Q_INVOKABLE`s

```cpp
// Drive-header lookups (section delegate only has the driveId string, not a row — see §4.3)
Q_INVOKABLE [[nodiscard]] QString driveIconName(const QString& driveId) const;      // REQ-F-006
Q_INVOKABLE [[nodiscard]] QString driveSubtitle(const QString& driveId) const;      // "USB · Solid state" etc.
Q_INVOKABLE [[nodiscard]] QString driveCapacityText(const QString& driveId) const;  // "1 TB total"
Q_INVOKABLE [[nodiscard]] QString driveOperationText(const QString& driveId) const; // REQ-F-012 for eject/power-off
Q_INVOKABLE [[nodiscard]] QString driveErrorText(const QString& driveId) const;     // REQ-F-013/014 for eject/power-off
Q_INVOKABLE [[nodiscard]] bool driveCanEject(const QString& driveId) const;
Q_INVOKABLE [[nodiscard]] bool driveCanPowerOff(const QString& driveId) const;

// Retry dispatch (REQ-F-014) — C++ owns the "which operation failed" decision
Q_INVOKABLE void retry(const QString& targetId);

// Click-to-mount-then-open (REQ-F-018/019)
Q_INVOKABLE void openVolume(const QString& targetId);

// Footer actions (REQ-F-016/017)
Q_INVOKABLE void openInFiles() const;
Q_INVOKABLE void showAllDevices() const;
```

`mount()`/`unmount()`/`eject()`/`driveLabel()` retain their signatures.
`powerOff(targetId)` replaces the former request/confirm/cancel flow and dispatches immediately.

### 3.4 New QML components

**`StorageCapacityBar.qml`**
```qml
required property real usedBytes
required property real totalBytes
required property real freeBytes
```
No signals — pure presentation.

**`StorageVolumeCard.qml`**
```qml
required property string targetId
required property string driveId
required property string name
required property string stateText
required property bool mounted
required property bool canMount
required property bool canUnmount
required property real usedBytes
required property real totalBytes
required property real freeBytes
required property string operationText
required property string errorText
```
No signals — every interactive element calls `StorageService` directly (`mount`, `unmount`,
`retry`, `openVolume`), matching how `WifiNetworkDelegate.qml`/`ProfileButton.qml` already call
their services directly rather than bubbling signals up through the delegate.

**`StorageDriveSection.qml`**
```qml
required property string section   // the driveId, per ListView.section.delegate contract
```
(See §4.3 for why this is the *only* property it can declare.)

---

## 4. Key Decisions

### 4.1 Used/total/free bytes: computed server-side in `StorageService::refresh()`

REQ-F-008 offers both options explicitly. Server-side wins:

- QML has no native access to `QStorageInfo` — the "client-side" option still requires a new C++
  invokable (e.g. `storageInfoFor(mountPath)`), so it doesn't actually avoid C++; it just moves the
  same `QStorageInfo` call behind a QML-triggered call, which is easy to invoke redundantly from a
  property binding that re-evaluates on unrelated dependency changes.
- Every other derived display field on a row (`name`, `driveName`, `stateText`, `canMount`, …) is
  already computed once per `refresh()` pass and cached in the row. Adding three more keys to that
  same pass is the path of least surprise and keeps one row-construction call site.
- The spec's client-side variant explicitly samples "at the time `StorageService` emits a mount
  notification," meaning it would go stale on later free-space changes without inventing a new
  notification concept. Server-side values are refreshed by every signal that already drives
  `refresh()` (drive/volume model changes, operation completions) — free correctness, no new event.

### 4.2 Operation-in-flight tracking: a StorageService-owned `QHash<targetId, StorageOperation>`, not reuse of `StorageController::busy()`

`StorageController::busy(targetId)` already exists and is already used for the pre-existing `busy`
role, but it is a plain `bool` — it cannot say *which* operation is in flight, and REQ-F-012 needs
per-operation text ("Mounting…" vs "Ejecting…"). The existing `busy` role is left untouched (it
still drives the existing `"Working…"` `stateText` fallback and general disablement); the new
`operationText`/`in_flight_ops_` is an independent, StorageService-owned data source populated only
from the four invokables that dispatch operations. Two data sources for "is this target busy" is
slightly redundant, but re-deriving operation *kind* from the backend is not possible without
touching `StorageController` (REQ-C-001 forbids that), so this is the only compliant option.

### 4.3 Drive-header data via invokables, not extra roles, because of `ComponentBehavior: Bound`

`StoragePopupContent.qml` already opens with `pragma ComponentBehavior: Bound`, and per this
project's documented gotcha, a `ListView.section.delegate` under Bound mode only receives the
implicit `section` string — it has no access to any row's other role values (there may not even be
a row for a pure drive header once volumes exist under it). The existing code already works this
way (`StorageService.driveLabel(section)`); this design extends the exact same pattern —
`driveIconName`, `driveSubtitle`, `driveCapacityText`, `driveOperationText`, `driveErrorText`,
`driveCanEject`, `driveCanPowerOff` are all `Q_INVOKABLE`s taking the `driveId` string, called from
`StorageDriveSection.qml`'s single `required property string section`. The alternative — synthesizing
a synthetic "drive row" per drive and inserting it into the model above its volumes — was rejected:
it would require sentinel rows threaded through `rowCount()`/`data()`/sorting/section grouping, and
would fight the `ListView.section` mechanism instead of using it as designed.

Drive-state bindings read `StorageService.count` directly inside each expression so its
`changed()` NOTIFY signal re-evaluates the invokable even when the count value stays unchanged.
An intermediate integer property would suppress updates when its value did not change.

Drive capacity uses the whole-disk partition-container size when available; otherwise it sums
non-container records while excluding cleartext overlays (`cryptoBackingId`), preserving physical
capacity without counting disk/partition or encrypted/cleartext bytes twice.

Icon classification prioritizes optical, flash/card media (including USB readers), USB SSD/HDD,
and generic removable media. `ExternalIcon` has an explicit generic removable-media fallback.
All five primary names render through Qt with both installed Breeze and Adwaita selected; four
are absent as standalone Breeze files, so filename inventory alone is not a resolution check.

### 4.4 Body-click vs. button-click isolation: geometric exclusion, not event-propagation reliance

REQ-NF-002 forbids a Mount/Unmount/Try-Again button click from also firing the body-click handler.
Two candidate mechanisms:

- **Z-order + accepted-event propagation**: rely on `Controls.Button`'s own internal pointer
  handling to grab the press before a same-area body `TapHandler`/`MouseArea` underneath it sees it.
  This is the standard QtQuick nested-clickable pattern and usually works, but this codebase has a
  documented history of pointer-handler swaps that pass `qmllint`/build/unit tests and only fail
  live (the `WheelHandler` regression, the `TapHandler` `gesturePolicy` regression) — betting
  REQ-NF-002 purely on implicit grab semantics is exactly the class of thing that has bitten this
  project before.
- **Geometric exclusion (chosen)**: `StorageVolumeCard.qml` puts the action buttons (Mount/Unmount,
  Try Again) in a `RowLayout` anchored to the card's bottom edge, and attaches the body
  `TapHandler` only to a sibling `Item` that spans the card's top region (name/subtitle/capacity
  bar/path) — a region that never geometrically overlaps the button row. Isolation is then true *by
  construction*, independent of hit-test/grab ordering, and is trivially verifiable by inspecting
  the two items' `anchors`/geometry rather than reasoning about handler precedence.

`TapHandler` is used (not `MouseArea`) for the body click, with `gesturePolicy:
TapHandler.ReleaseWithinBounds` — this exact combination is already the established pattern in
`StatusPopupTriggerArea.qml` for the equivalent "forgiving click, not drag-sensitive" requirement,
and CLAUDE.md documents `ReleaseWithinBounds` as the correct override for `MouseArea.onClicked`
parity. The body `TapHandler` is additionally disabled (`enabled: !(operationText.length > 0)`) as
defense-in-depth against double-triggering during an in-flight mount, on top of the C++-side guard
in `openVolume()`/`mount()` (§2.2/2.3).

### 4.5 Retry dispatch lives in C++ (`StorageService::retry(targetId)`), not QML

REQ-F-014 requires replaying the *same* operation that failed. `last_errors_` already stores the
`StorageOperation` enum value per target; exposing that enum to QML (as an int role) would force
`StoragePopupContent`/`StorageVolumeCard` to carry a switch statement mapping it back to
`mount()`/`unmount()`/`eject()`/`powerOff()` calls. Instead, `retry(targetId)` does
that dispatch internally and QML's "Try Again" button becomes a single unconditional call —
`StorageService.retry(root.targetId)` — for both volume-row and drive-header error states.

### 4.6 Popup surface sizing: reused as-is, no C++ geometry change

`StatusPopupGeometry.cpp`'s `statusPopupSizePolicy()` already special-cases `"storage"` with
`minimum 360×280 / preferred 480×560 / maximum 600×820` and `StorageOverflowMode::InternalList` —
already the largest allowance of any popup except `audio`/`network`, and already configured for
*internal* scrolling rather than surface-size-tracks-content. The card-based, taller layout fits
entirely inside this pre-existing envelope: `StoragePopupContent.qml`'s `ListView` (unchanged
`Layout.fillWidth`/`Layout.fillHeight` + `Controls.ScrollBar.vertical`) simply grows its per-item
delegate height from ~48px (old flat 2-line row) to ~110–140px (new card), and the existing
`ScrollBar` absorbs any overflow beyond the fixed surface height. No change to
`StatusPopupGeometry.cpp`/`StatusPopupSurface` is needed, and — because the popup content area's
size is dictated top-down by the fixed surface (not bottom-up by children's implicit size) — the
`afterAnimating`-vs-`implicitWidthChanged` sizing gotcha (which applies to surfaces that size
*themselves* from content, e.g. sidebars/widgets) does not apply here at all.

---

## 5. Alternatives Considered

### 5.1 Used/total/free bytes — client-side (QML) computation

Rejected per §4.1: doesn't eliminate the C++ dependency (QML still needs an invokable to reach
`QStorageInfo`), and the spec's own client-side variant is defined to be sampled only on mount
events, which is strictly staler than refreshing on every signal `refresh()` already listens to.

### 5.2 Operation-state tracking — correlate via `requestId` instead of `targetId`

`StorageController::mount()`/`unmount()`/`eject()`/`powerOff()` each return a `QString requestId`,
and it's tempting to store `QHash<requestId, targetId>` for precise correlation. Rejected: the
`operationFinished(StorageResult)` signal payload already carries `targetId` and `operation`
directly — no `requestId`→target lookup is needed to know what finished. `requestId`-based tracking
would only earn its keep if the *same* `targetId` could have two operations in flight
simultaneously, but REQ-F-012's "conflicting actions disabled while operation in flight" already
requires enforcing at most one in-flight operation per `targetId` — which this design's guard
(§2.2 step 1) provides as a side effect. Given that invariant, `requestId` bookkeeping would be
dead weight.

### 5.3 Body-click isolation — `MouseArea` instead of `TapHandler`

Considered reverting to a plain `MouseArea` (whose `onClicked` fires on any press+release
regardless of drift, `NF-002`'s "forgiving" click semantics without needing an explicit
`gesturePolicy`). Rejected in favor of `TapHandler` + `ReleaseWithinBounds`: this project's own
`StatusPopupTriggerArea.qml` already established `TapHandler.ReleaseWithinBounds` as the idiomatic
way to get `MouseArea`-equivalent forgiveness in this codebase, and pointer-handler consistency
across sibling components is worth more than a marginal risk difference between the two primitives
— especially since §4.4's geometric-exclusion approach makes the isolation guarantee independent of
which primitive is chosen.

---

## 6. Known Risks (require live compositor testing, not just build/lint/tests)

1. **Symbolic icon tinting for device-type icons.** `StorageWidget.qml`'s current placeholder
   already sets `normalColor` on an `HnIcon` with `rendering: HnIcon.Original` loading an
   `image://icon/...` source — if `HnIcon` does *not* internally apply the documented
   MultiEffect-colorization workaround for this combination, both the topbar's subdued/active icon
   recoloring (REQ-F-004/005) and every drive-header device-type icon in the popup
   (`StorageDriveSection.qml`, REQ-F-006) will render near-black on the dark theme instead of the
   intended color. Must be confirmed by reading `HnIcon`'s implementation (outside this repo, in
   the shared component library) or by live-launching the shell — this cannot be verified from
   `holonight-shell` source alone.

2. **SSD/NVMe classification heuristic accuracy.** `StorageDrive` (from `StorageTypes.h`) exposes
   `media`/`mediaCompatibility` string lists but no rotation-rate or explicit is-SSD field. REQ-F-006's
   SSD detection is implemented as a substring heuristic (`"ssd"`, `"nvme"`, `"solid_state"`) against
   those fields; whether real UDisks2 `Drive.Media`/`Drive.MediaCompatibility` values for an actual
   USB SSD (e.g. a Samsung T7) contain any such substring cannot be confirmed by reading code —
   REQ-NF-001 explicitly requires confirming against live hardware/`busctl introspect`. The
   classifier degrades to the `UsbHdd` bucket (never blank/undefined) if the heuristic misses, so
   worst case is a wrong-but-present icon, not a missing one — but "wrong" still needs a live check
   against the acceptance criterion's named devices.

3. **Body-click vs. button-click isolation (REQ-NF-002).** Even with the geometric-exclusion design
   in §4.4, this project has three prior instances (documented in project memory) of a
   `MouseArea`↔pointer-handler swap passing `qmllint`, build, and the automated QML smoke test while
   failing to route input correctly live. The `TapHandler.ReleaseWithinBounds` body handler and the
   sibling button `RowLayout` must be clicked in a live Hyprland session (both edge cases: clicking
   near the card/button boundary, and clicking a button while an operation is already in flight and
   the body handler is `enabled: false`).

4. **Full model reset on every operation-state transition.** `StorageService::refresh()` keeps its
   existing `if (rows != rows_) { beginResetModel(); …; endResetModel(); }` pattern (unchanged, per
   minimizing risk to already-working model-sync code). Because `operationText`/`errorText` are now
   *row content*, every mount/unmount/eject click and every completion still triggers a full
   `QAbstractItemModel` reset (all `StorageVolumeCard` delegates destroyed and recreated), not a
   targeted `dataChanged()`. This is pre-existing architecture, not a regression introduced here, but
   it means the in-flight `TapHandler`'s press/grab state and the `ListView`'s scroll position are
   candidates for visible hiccups during rapid operations — worth watching for during REQ-F-018's
   "click again before it finishes" acceptance check live, even though the C++ dedupe guard should
   make it functionally correct either way.

5. **Popup delegate height growth inside a fixed-size, `InternalList` surface.** §4.6 establishes
   that no C++ sizing change is needed, but this is a read of `StatusPopupGeometry.cpp`, not a live
   measurement — worth confirming live that a drive with 3+ volumes plus an active error/retry row
   still scrolls cleanly (not clipped) inside the 600×820 maximum, and that the `ScrollBar` remains
   usable when the popup is near its 360×280 minimum on a small/scaled output.

6. **External notification daemon policy.** Notification dispatch is asynchronous. The shell's
   own receiver honors trusted storage bypass and transient history exclusion; a replacement
   notification daemon controls its own DND and transient-notification policies.

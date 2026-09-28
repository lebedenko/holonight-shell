# Storage Topbar Widget + Popup Redesign Specification

## Overview

This specification defines the redesign of the Storage widget in the HoloNight Shell topbar and its companion Storage popup interface. The feature leverages the existing `StorageService` backend (UDisks2-backed mount/unmount/eject/powerOff operations via `holonight-system-services`) and improves visibility, interactivity, and user feedback for removable storage device management.

**Scope**: Updates the storage service wrapper, topbar widget, popup components, and the shell notification receiver's trusted transient handling. No backend changes to `holonight-system-services`, `StorageController`, or `StorageDrive`/`StorageVolume` contracts.

---

## Functional Requirements

### Device Count & Visibility

**REQ-F-001** — Device Count Aggregation

The `StorageService` shall expose a new `deviceCount` property (read-only int, NOTIFY-connected) that counts the number of distinct physical drives (`driveId` values) currently visible and eligible for display, computed from the intersection of eligible rows (per existing `StorageFilter::eligible()` logic) and drive-level removal/media-presence checks used in `StorageService::refresh()`.

- **Acceptance Criterion**: Given a USB stick with two NTFS partitions and one USB optical drive connected, `StorageService.deviceCount` is exactly 2, not 4 or 5. Given no removable devices attached, `StorageService.deviceCount` is exactly 0. The property value updates (emits NOTIFY signal) within 500ms of device hotplug or mount-state change.

**REQ-F-002** — Mounted-Volume Aggregation

The `StorageService` shall expose a new `hasMounted` property (read-only bool, NOTIFY-connected) that is true if and only if at least one volume in the current list has `mounted == true`, and false otherwise.

- **Acceptance Criterion**: With one USB stick mounted (one partition) and one unmounted USB card reader, `hasMounted == true`. Unmount that partition, then `hasMounted == false`. The property updates (emits NOTIFY signal) within 500ms of a mount/unmount operation completing.

### Topbar Widget Visual States

**REQ-F-003** — Topbar Hidden State

WHILE `deviceCount == 0`, the StorageWidget shall render as completely invisible and occupy zero width in the topbar layout.

- **Acceptance Criterion**: With no removable devices attached, the topbar Storage widget takes zero layout space (adjacent topbar items abut without gap). Attaching any eligible removable device transitions it to visible state.

**REQ-F-004** — Topbar Subdued State

WHILE `deviceCount > 0` AND `hasMounted == false`, the StorageWidget shall render the storage icon in a subdued (dimmed/desaturated) foreground color and shall NOT display a count badge.

- **Acceptance Criterion**: With one USB stick (unmounted) connected, the widget renders a dimmed icon with no numeric badge. Mounting any partition on that stick transitions the widget to active state.

**REQ-F-005** — Topbar Active State

WHILE `deviceCount > 0` AND `hasMounted == true`, the StorageWidget shall render the storage icon in the system accent color and SHALL display a count badge showing the `deviceCount` value (including when `deviceCount == 1`).

- **Acceptance Criterion**: With one USB stick (mounted) and one unmounted USB card reader, the widget renders an accent-colored icon with a badge reading "2" (not "1", and not omitted). Unmounting the stick transitions to subdued state; ejecting the card reader transitions to hidden state.

### Icon Selection by Device Type

**REQ-F-006** — Device-Type Icon Mapping (Popup Card Icons)

For each removable drive in the Storage popup, the system shall select a symbolic icon name based on the drive's properties as follows:

- IF `drive.optical == true` THEN icon name is `media-optical-symbolic`
- ELSE IF `drive.mediaCompatibility` indicates SD card, CompactFlash, or similar flash media (including USB readers) THEN icon name is `media-flash-symbolic`
- ELSE IF `drive.connectionBus` indicates USB AND `drive.media`/`mediaCompatibility` indicate SSD/NVMe-like storage THEN icon name is `drive-harddisk-solidstate-symbolic`
- ELSE IF `drive.connectionBus` indicates USB AND NOT SSD THEN icon name is `drive-harddisk-usb-symbolic`
- ELSE (fallback for any other removable drive) icon name is `drive-removable-media-symbolic`

- **Acceptance Criterion**: A USB optical disc drive displays `media-optical-symbolic`. A USB Samsung T7 (SSD) displays `drive-harddisk-solidstate-symbolic`. A USB WD Blue HDD displays `drive-harddisk-usb-symbolic`. A USB SD-card reader displays `media-flash-symbolic`. An unknown vendor removable drive (e.g. proprietary music player) displays `drive-removable-media-symbolic`. No device's icon is blank or undefined.

**REQ-NF-001** — Icon Resolution Consistency

Icon names selected by REQ-F-006 shall be confirmed to render through Qt with both Breeze and Adwaita selected (both installed on the target system), including Qt theme fallback lookup. A missing standalone SVG is not proof of resolution failure. The popup shall provide an explicit generic removable-media fallback; an icon that remains blank after fallback is a blocker.

- **Acceptance Criterion**: With each theme selected using `QIcon::setThemeName()`, `QIcon::fromTheme(name).pixmap(22, 22)` is non-null for every selected name or its explicit fallback. Record both the filename inventory and runtime resolution result. On the target system all five names resolve with Breeze and Adwaita; only the generic removable symbol is a standalone Breeze file.

### Popup Structure & Grouping

**REQ-F-007** — Drive-Grouped, Collapsible Popup Layout

The StoragePopupContent shall organize volumes by physical drive (grouping key: `driveId`) with each drive's header rendered once, followed by its subordinate volumes. The section delegate shall render:

- Drive header: drive icon (per REQ-F-006), friendly drive name (via `StorageService.driveLabel(targetId)` where targetId is a drive row), connection type/media hint text, total capacity, a collapse/expand toggle, and an Eject button and a Power Off button (both act on the whole drive; icon-only).
- A collapse/expand toggle on the drive header that hides/shows that drive's volume rows without affecting any other drive's collapsed state (tracked per-`driveId`, default expanded).
- Below each (expanded) drive header, one row per volume: mounted-state indicator, volume name, capacity bar (mounted-only, see REQ-F-008), mount path (subdued text, mounted-only), and Mount/Unmount/Open buttons.

- **Acceptance Criterion**: With one USB stick containing two partitions (both mountable) and one mounted, the popup renders: one drive header with drive icon, name, collapse toggle, Eject button, and Power Off button; two volume rows underneath (one with filled indicator, one with hollow indicator). No duplicate drive headers. Volumes are ordered by `driveId`, then by row order within `StorageService`. Clicking the collapse toggle on one drive hides only that drive's volume rows; a second, unrelated drive's rows remain visible and its own collapse state is unaffected.

**REQ-F-007a** — Popup Hero Header

The StoragePopupContent shall render a fixed hero header above the device list, per `docs/mockups/udisks2.png`, structured as three columns: a leading generic removable-storage icon, a two-line title/subtitle text column ("Removable Storage" / device-count summary), and a trailing settings icon-button.

- The subtitle text shall read "No devices connected" when `deviceCount == 0`, "1 device connected" when `deviceCount == 1`, and "N devices connected" for `deviceCount > 1` (never "0 devices connected").
- The trailing settings icon-button is present but inert this cycle — no Storage settings page exists yet (product decision made during this cycle; wiring it up is out of scope).

- **Acceptance Criterion**: With zero devices attached, the header subtitle reads exactly "No devices connected" (not "0 devices connected"). With exactly one device attached, it reads "1 device connected". With three attached, it reads "3 devices connected". The header renders regardless of `deviceCount` (unlike the ListView/empty-state below it, which are mutually exclusive on `deviceCount == 0`).

### Capacity Display & Bar

**REQ-F-008** — Used-Space Data Exposure

For each mounted volume in `StorageService`, the system shall expose or compute the following data accessible to the popup QML:

- Used bytes (computed from `QStorageInfo(mountPath).bytesTotal() - bytesAvailable()`)
- Total bytes (from `QStorageInfo(mountPath).bytesTotal()`)
- Free bytes (from `QStorageInfo(mountPath).bytesAvailable()`)

If computed client-side (in QML), these values shall be sampled at the time `StorageService` emits a mount notification. If computed server-side (in `StorageService::refresh()`), new roles `usedBytes`, `totalBytes`, `freeBytes` shall be added to the model and updated each refresh cycle.

- **Acceptance Criterion**: With a 1TB USB drive mounted with 463GB used and 537GB free, `StorageService` exposes or QML computes: `usedBytes == 463GB`, `totalBytes == 1TB`, `freeBytes == 537GB`. The popup renders the capacity bar and text "463 GB used of 1 TB" or "537 GB free of 1 TB" (primary convention per decision: "N used of M") without additional D-Bus calls or manual `QProcess` invocations.

**REQ-F-009** — Capacity Bar Rendering (Mounted Volumes Only)

For each mounted volume, the popup shall render a horizontal capacity bar:

- Bar width represents 100% of total capacity.
- Filled portion (left-to-right) represents used bytes; color determined by free-space threshold (see REQ-F-010).
- Bar is rendered ONLY for mounted volumes; unmounted volumes show no bar.
- Text label near or below the bar: "X GB used of Y GB" (using the primary convention) or "X GB free of Y GB" as appropriate to the design (mockup shows both variants; implementation shall pick one consistently).

- **Acceptance Criterion**: A mounted 1TB volume with 100GB free renders a 90%-filled colored bar. An unmounted 1TB volume shows no bar. Resizing the popup does not break the bar layout or cause text truncation beyond the available width.

**REQ-F-010** — Capacity Bar Coloring by Free-Space Threshold

The capacity bar fill color shall be determined by free-space percentage (free bytes / total bytes) as follows, using hardcoded constants (NOT sourced from `holonight-config` this cycle):

- IF `free% >= 10%` THEN use system accent color (normal state)
- ELSE IF `5% <= free% < 10%` THEN use system warning color (amber/yellow)
- ELSE IF `free% < 5%` THEN use system critical color (red)

- **Acceptance Criterion**: A 1TB volume with 150GB free (85% free) displays an accent-colored bar. The same drive with 95GB free (9.5% free) displays warning color. The same drive with 40GB free (4% free) displays critical color. These are hardcoded thresholds in the QML component or a QML constant file, not sourced from settings or configuration files.

### Transient Operation States

**REQ-F-011** — Per-Target Operation Tracking

The `StorageService` shall track, for each targetId, the currently in-flight `StorageOperation` (mount/unmount/eject/powerOff). When an invokable (`mount()`, `unmount()`, `eject()`, `powerOff()`) is called with a targetId, the system shall record that operation; when `StorageController::operationFinished(StorageResult)` fires and the result's `targetId` matches a tracked operation, the tracking entry shall be cleared. Power-off dispatches immediately on invocation, identically to mount/unmount/eject — this cycle deliberately removes the confirmation step (`confirmPowerOff()`/`cancelPowerOff()`/`confirmationText`) that existed prior to this redesign (product decision made during this cycle; see Non-Goals).

- **Acceptance Criterion**: Call `StorageService.mount(volumeTargetId)`. Within 10ms, a new operation-in-flight property is set (implementation detail; not exposed as a public property at this stage). When the mount completes (success or failure), that entry is cleared. Multiple simultaneous operations on different targetIds are tracked independently.

**REQ-F-012** — Transient Operation Text

WHILE an operation is in flight for a given target (REQ-F-011), the popup's card for that target shall display transient text in place of or adjacent to the action buttons:

- Mount in flight: "Mounting…"
- Unmount in flight: "Unmounting…"
- Eject in flight: "Ejecting…"
- Power off in flight: "Powering off…"

Conflicting actions (mount button disabled while mounting, unmount button disabled while unmounting, etc.) on that card shall be disabled during the operation.

- **Acceptance Criterion**: Call `StorageService.mount(volumeId)` on an unmounted volume. The volume's row immediately shows "Mounting…" text and the Mount button becomes disabled. When the operation completes (success or failure), the text clears and the button re-enables. Clicking the Mount button while "Mounting…" is displayed has no effect.

### Per-Target Error Tracking & Retry

**REQ-F-013** — Per-Target Error Tracking

The `StorageService` shall maintain a per-targetId map of last-error state. When an operation completes with `StorageResult::succeeded() == false`, the system shall:

1. Store the error message (already translated via the existing `operationError()` function in `StorageService.cpp`) keyed by targetId.
2. Store the `StorageOperation` that failed (mount/unmount/eject/powerOff).
3. Clear the error entry when a new operation is initiated for that targetId, or when an operation completes successfully for that targetId.

- **Acceptance Criterion**: An unmount operation fails with "Permission denied". The error message is stored and displayed inline on that volume's row. Initiating a retry (REQ-F-014) clears the error placeholder. A successful unmount on a different volume does not affect the first volume's error display.

**REQ-F-014** — Per-Target Error Display & Retry Button

The popup shall render, for each target with an active error (REQ-F-013):

- The error message text (subdued styling, red or warning color).
- A "Try Again" button adjacent to the error.

Clicking "Try Again" shall re-invoke the same operation that failed (`mount()` if mount failed, `unmount()` if unmount failed, etc.), using the stored `StorageOperation` enum to dispatch.

- **Acceptance Criterion**: An eject operation fails. The row displays red error text "Device is in use" and a "Try Again" button. Clicking "Try Again" calls `StorageService.eject(targetId)` again (not a different operation). On retry success, the error display is cleared.

### Device Connect & Safe-to-Remove Notifications

**REQ-F-015** — Safe-to-Remove D-Bus Notification (Eject or Power Off)

WHEN an eject OR power-off operation completes successfully (`StorageResult::succeeded() == true` AND `operation` is `Eject` or `PowerOff`), the system SHALL send a transient desktop notification via D-Bus. The notification SHALL be posted to `org.freedesktop.Notifications.Notify` directly, with transient metadata and a receiver-side bypass for calls originating from the shell's own session-bus connection (bypassing DND and per-app rules and excluding history), with:

- Summary: "{Device name} can be safely removed" (device name from `StorageService.driveLabel(targetId)` or equivalent)
- Body: empty string (notification is title-only)
- App name: "holonight-shell"
- Timeout: 5 seconds

- **Acceptance Criterion**: Eject a USB stick named "BACKUP". The desktop displays a notification "BACKUP can be safely removed" that fades after ~5 seconds, without appearing in the shell's notification history/archive. Power off the same device instead of ejecting it: the identical notification text is posted. The notification is posted via a direct D-Bus `Notify` call (observable via `busctl`, with trusted receiver-side transient handling).

**REQ-F-021** — Device-Connected D-Bus Notification

WHEN a drive newly appears in `StorageService` (a `driveId` not present in the previous refresh cycle) AFTER the first available controller snapshot has fully populated both models, the system SHALL send a transient desktop notification via the same direct-D-Bus mechanism as REQ-F-015, with summary "{Device name} connected", empty body, app name "holonight-shell", and a 5-second timeout. Drives already present in that initial available snapshot SHALL NOT trigger this notification. Mount and unmount operations SHALL NOT trigger any desktop notification (neither this one nor REQ-F-015).

- **Acceptance Criterion**: With the shell already running and no devices attached, plug in a USB stick. The desktop displays "{stick name} connected". Restart the shell with that same USB stick already attached: no "connected" notification fires on startup. Mount, then unmount, that stick: neither action produces any desktop notification.

### Popup Footer Actions

**REQ-F-016** — Open in Files Action

The popup footer shall include an "Open in Files" button that, when clicked, launches `holonight-files` via `QProcess::startDetached()` with NO positional arguments (opens `holonight-files` to its default/home view).

- **Acceptance Criterion**: Click "Open in Files". The `holonight-files` window appears showing the default/home view. No mount path argument is passed to `holonight-files`.

**REQ-F-017** — Show All Devices Action

The popup footer shall include a "Show All Devices" button that, when clicked, launches `holonight-files` via `QProcess::startDetached()` with the first currently-mounted removable volume's mount path as the positional folder argument. IF no volume is currently mounted, the button SHALL launch `holonight-files` with NO arguments (identical to REQ-F-016).

- **Acceptance Criterion**: With one mounted USB stick at `/run/media/user/BACKUP`, clicking "Show All Devices" launches `holonight-files /run/media/user/BACKUP`. With no mounted removable volumes, clicking "Show All Devices" launches `holonight-files` with no arguments (same as "Open in Files"). **Known Limitation**: `holonight-files` has no flag to pre-focus its Devices sidebar; "Show All Devices" is a best-effort approximation.

### Click-to-Mount-and-Open Interaction

**REQ-F-018** — Unmounted Volume Body Click (Mount + Open)

The popup volume card's content body (excluding the explicit Mount/Unmount buttons) shall support a click handler. WHEN a user clicks an unmounted volume's body:

1. Invoke `StorageService.mount(targetId)`.
2. When the mount operation completes successfully, invoke `QProcess::startDetached("holonight-files", [mountPath])`.
3. If the mount operation fails, display the per-target error (REQ-F-013/F-014) and do not launch Files.

- **Acceptance Criterion**: Click the body of an unmounted USB stick. It mounts and `holonight-files` opens showing the mount path. Click the body again (now mounted). See REQ-F-019. Click a different unmounted USB drive's body, but before it finishes mounting, click it again (in-flight). The mount operation is not double-triggered, and Files launches exactly once on successful completion.

**REQ-F-019** — Mounted Volume Body Click (Open Without Mount)

WHEN a user clicks a mounted volume's body, the system SHALL invoke `QProcess::startDetached("holonight-files", [mountPath])` directly, WITHOUT invoking mount again.

- **Acceptance Criterion**: Click the body of a mounted USB stick. `holonight-files` opens immediately, showing the mount path. The volume is not re-mounted. Clicking the explicit Mount button (if still present) is a separate interaction and remains available.

**REQ-NF-002** — Body vs. Button Click Separation

The click event handling for volume body and Mount/Unmount/Eject buttons shall not conflict. Clicking a button must not also trigger the body-click handler (REQ-F-018/F-019).

- **Acceptance Criterion**: Click the explicit Mount button on an unmounted volume. Only the mount action is triggered; the auto-open behavior (REQ-F-018) does not fire. Click the volume body background (away from buttons). Only the body-click handler fires.

### Empty-State & Device Removal

**REQ-F-020** — Transient Empty-State on Device Removal

IF the last eligible removable device is removed WHILE the Storage popup is open (i.e., `deviceCount` transitions from 1 to 0 while the popup has focus), the system SHALL:

1. Render an empty-state view in the popup (not close the popup immediately).
2. Empty-state display (rendered below the hero header of REQ-F-007a, which remains visible and shows "No devices connected"): icon + title "No removable storage" + subtitle "External drives and USB devices will appear here" (per mockup design).
3. Popup closes on next natural dismiss event (focus loss, user clicks outside, or user clicks a close/dismiss button) — NOT a forced auto-close.

- **Acceptance Criterion**: With one USB stick in the popup, unplug it. The popup remains open, displaying the empty-state message. Click outside the popup or press Escape. The popup closes normally. Re-attach the USB stick; the popup does not reopen automatically (awaits next manual trigger).

---

## Non-Functional Requirements

**REQ-NF-001** — Icon Resolution Consistency [see REQ-F-006 for details]

**REQ-NF-002** — Button Click Event Isolation [see REQ-F-018/F-019 for details]

**REQ-NF-003** — Operation Tracking Efficiency

Per-target operation tracking (REQ-F-011) shall not introduce measurable additional CPU load or memory overhead. Tracking entries for targets whose operations have completed (and are persisted in error state per REQ-F-013) shall be cleared automatically within 30 seconds of completion if no new operation is initiated.

- **Acceptance Criterion**: After 100 eject operations on different targets (each completing successfully or with an error), the in-memory operation-tracking map contains at most 2–3 entries (those with active errors awaiting retry). Memory usage remains <1MB above baseline for the StorageService singleton.

---

## Constraint Requirements

**REQ-C-001** — Backend API Stability

The system SHALL NOT modify `StorageController`, `StorageDriveModel`, `StorageVolumeModel`, `StorageDrive`, `StorageVolume`, `StorageOperation`, or `StorageResult` public APIs or contracts in `holonight-system-services`. Storage behavior is implemented via wrapper/derived state in `StorageService` (the QML-facing singleton in `holonight-shell/libs/holonight-services/src/storage/`) or client-side in QML. The shell notification receiver additionally implements trusted transient delivery for the direct D-Bus notifications.

- **Acceptance Criterion**: Post-implementation, `holonight-system-services/include/holonight_system/storage/` contains no new public types, no modified method signatures in existing types, and no removed methods. The service binary protocol (UDisks2 D-Bus integration) is unchanged.

**REQ-C-002** — No holonight-files Modifications

The system SHALL NOT modify `holonight-files` source code beyond using its existing CLI interface (`folder` positional argument).

- **Acceptance Criterion**: `apps/files/main.cpp` and all `holonight-files` QML/logic remain unmodified. The shell's interaction is limited to `QProcess::startDetached("holonight-files", […])`.

**REQ-C-003** — No Configuration Wiring This Cycle

The system SHALL NOT add `holonight-config` or `holonight-settings` configuration entries for capacity thresholds (REQ-F-010) this cycle. All thresholds are hardcoded constants in the QML component.

- **Acceptance Criterion**: No new keys are added to `holonight-config` schema. The capacity thresholds (10%, 5%) appear as QML numeric literals in `StoragePopupContent.qml`, not derived from `ConfigService` or settings bindings.

**REQ-C-004** — Existing Filter Logic Unchanged

The system SHALL NOT modify `StorageFilter::eligible()` or the drive-removal/media-presence logic in `StorageService::refresh()`. Device visibility and filtering remain exactly as defined before this redesign.

- **Acceptance Criterion**: Running the same hardware configuration before and after this redesign shows identical devices in the `StorageService` list (same targetIds, same mount/unmount/eject eligibility).

---

## Non-Goals

The following items are explicitly OUT OF SCOPE for this redesign and shall NOT be implemented this cycle:

1. **Backend Storage Controller Changes** — No modifications to UDisks2 integration, `StorageDrive`/`StorageVolume` model contracts, or D-Bus operations.
2. **Encrypted Volume Unlock/Lock UI** — Encrypted (locked) volumes retain their current "Locked — unlocking is not available" state text behavior. No unlock dialog or passphrase entry is added.
3. **Internal/System Drives** — Internal, non-removable, or system drives are excluded by the existing filter logic; this design does not change or extend that filtering. Only devices marked removable with media present are shown.
4. **Partition Management** — No partition creation, deletion, resizing, or reformatting UI.
5. **Advanced Storage Features** — No SMART monitoring, RAID/LVM management, fsck, or disk repair workflows.
6. **Configuration Thresholds** — Capacity warning/critical thresholds are hardcoded this cycle. Future work may wire them into `holonight-settings`, but it is not part of this redesign.
7. **Devices-Only View in holonight-files** — The "Show All Devices" action launches `holonight-files` with a mount path argument (or no argument), not a Devices-only filtered view. `holonight-files` lacks a `--show-devices` flag or equivalent; this is documented as a known limitation (REQ-F-017).
8. **Multiple Device Selection** — Bulk mount/unmount/eject operations are not supported. Each device/volume is acted upon independently.
9. **Device Aliases or Custom Naming** — Device display names come from UDisks2 `StorageDrive.name` / `StorageVolume.name` and `StorageService.driveLabel()`. Custom aliases are not added.
10. **Background Sync or Monitoring** — The storage list updates reactively when `StorageController` signals changes; no independent polling or background refresh thread is added.
11. **Power-Off Confirmation Step** — A prior, pre-existing two-step confirm/cancel flow for power-off was removed during this cycle (product decision); power-off is now immediate and symmetric with mount/unmount/eject. No new confirmation UI is added for any destructive storage action.

---

## Traceability

| Decision # | EARS Template | Requirement ID | Scope |
|---|---|---|---|
| 1 | Constraint | REQ-C-001 | Backend stability; no changes to StorageController |
| 2 | Ubiquitous + State | REQ-F-001, REQ-F-002 | New deviceCount and hasMounted properties |
| 3 | State-driven | REQ-F-003, REQ-F-004, REQ-F-005 | Three topbar visual states (Hidden, Subdued, Active) |
| 4 | Conditional | REQ-F-006, REQ-NF-001 | Icon selection by device type with fallback |
| 5 | Ubiquitous | REQ-F-007, REQ-F-007a | Drive-grouped, collapsible popup layout with hero header |
| 6 | Ubiquitous | REQ-F-008, REQ-F-009 | Capacity bar with used-space data |
| 7 | Ubiquitous | REQ-F-010 | Hardcoded capacity threshold coloring |
| 8 | Ubiquitous | REQ-F-011, REQ-F-012 | Per-target operation tracking and transient text (power off dispatches immediately, no confirmation) |
| 9 | Ubiquitous | REQ-F-013, REQ-F-014 | Per-target error tracking and retry |
| 10 | Event-driven | REQ-F-015, REQ-F-021 | D-Bus safe-to-remove (eject/power-off) and device-connected notifications; mount/unmount stay silent |
| 11 | Ubiquitous | REQ-F-016, REQ-F-017 | Footer actions: Open in Files and Show All Devices |
| 12 | Event-driven + When | REQ-F-018, REQ-F-019, REQ-NF-002 | Click-to-mount-then-open and mounted-open interactions |
| 13 | Event-driven | REQ-F-020 | Transient empty-state on device removal |
| 14 | Constraint | REQ-C-002 through REQ-C-004 | Non-goals and constraints |

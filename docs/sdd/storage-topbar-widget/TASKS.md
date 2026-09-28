# SDD Tasks — storage-topbar-widget

- [x] T-001: Add deviceCount and hasMounted properties to StorageService
  - REQs: REQ-F-001, REQ-F-002
  - Check: StorageService exposes int deviceCount and bool hasMounted properties that correctly count distinct drives and aggregate mounted status from eligible rows, emit NOTIFY signals within 500ms of changes, and compute accurately on each refresh(); StorageController/StorageFilter.h remain unmodified (REQ-C-001, REQ-C-004).

- [x] T-002: Implement per-target operation tracking and transient operation text in StorageService
  - REQs: REQ-F-011, REQ-F-012, REQ-NF-003
  - Check: Calling mount/unmount/eject/powerOff records the operation in in_flight_ops_, operationText role immediately displays "Mounting…"/"Unmounting…"/"Ejecting…"/"Powering off…" on affected rows within 10ms, entries clear on completion, multiple simultaneous operations on different targetIds are tracked independently, and the in-memory tracking map remains <1MB above baseline even after 100+ operations (REQ-NF-003).

- [x] T-003: Implement per-target error tracking and retry dispatch in StorageService
  - REQs: REQ-F-013, REQ-F-014
  - Check: Failed operations store error message and operation type per targetId in last_errors_, errorText role exposes the message, retry(targetId) re-invokes the stored operation (mount/unmount/eject/powerOff as appropriate), and errors clear on new operations or successful completion.

- [x] T-004: Expose used/total/free byte data for mounted volumes via QStorageInfo in StorageService
  - REQs: REQ-F-008
  - Check: Mounted volumes populate usedBytes, totalBytes, freeBytes roles from QStorageInfo(mountPath), unmounted volumes have these as 0, byte data updates within refresh() cycle on mount/unmount/operation-completion signals, and StorageFilter.eligible() logic remains unchanged (REQ-C-004).

- [x] T-005: Add device-type icon classification and drive-header invokables to StorageService
  - REQs: REQ-F-006, REQ-NF-001
  - Check: driveIconName(driveId) returns correct symbolic icon names (media-optical-symbolic for optical, drive-harddisk-solidstate-symbolic for USB SSD, drive-harddisk-usb-symbolic for USB HDD, media-flash-symbolic for flash/SD, drive-removable-media-symbolic as fallback), invokables driveSubtitle/driveCapacityText/driveOperationText/driveErrorText/driveCanEject/driveCanPowerOff are callable from QML, all icon names render through Qt with Breeze and Adwaita selected, including theme fallback lookup, and the popup has an explicit generic removable fallback (REQ-NF-001), and StorageController/StorageFilter.h remain unmodified (REQ-C-001).

- [x] T-006: Implement eject-success D-Bus notification directly from operationFinished handler
  - REQs: REQ-F-015
  - Check: On successful eject, a direct org.freedesktop.Notifications.Notify D-Bus call is issued with summary "{Device name} can be safely removed", empty body, app name "holonight-shell", and 5-second timeout, with trusted receiver-side DND/app-rule bypass and transient history exclusion.

- [x] T-007: Add click-to-mount-then-open and footer action invokables to StorageService
  - REQs: REQ-F-018, REQ-F-019, REQ-F-016, REQ-F-017
  - Check: openVolume(targetId) launches holonight-files with mount path if mounted, mounts and launches on success if unmounted, or displays error without launching if mount fails; openInFiles() launches holonight-files with no arguments; showAllDevices() launches with first mounted removable volume's path or no arguments if none mounted; holonight-files binary and source code remain unmodified (REQ-C-002).

- [x] T-008: Create StorageCapacityBar.qml base component with threshold-based coloring
  - REQs: REQ-F-009, REQ-F-010
  - Check: Component renders a horizontal bar showing used/free capacity ratio with fill color accent (free >= 10%), warning/amber (5% <= free < 10%), or critical/red (free < 5%), displays "X GB used of Y GB" text label, renders only for mounted volumes, and thresholds are hardcoded QML constants not derived from ConfigService (REQ-C-003).

- [x] T-009: Create StorageVolumeCard.qml volume row delegate with transient and error states
  - REQs: REQ-F-007, REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-018, REQ-F-019, REQ-NF-002
  - Check: Card displays mounted-state indicator, volume name, capacity bar (mounted volumes only), mount path; shows transient operation text while operation in-flight and disables conflicting action buttons; displays error message with "Try Again" button on operation failure; body-click (outside buttons) launches holonight-files if mounted or mounts-then-launches if unmounted; clicking Mount/Unmount/Try-Again buttons does not trigger body-click handler (REQ-NF-002 geometric isolation).

- [x] T-010: Create StorageDriveSection.qml section delegate for drive headers
  - REQs: REQ-F-006, REQ-F-007
  - Check: Section delegate renders drive header with icon (from driveIconName(section)), friendly name (from driveLabel(section)), connection/media subtitle (from driveSubtitle(section)), total capacity text (from driveCapacityText(section)), Eject and Power Off buttons with correct enabled state, transient operation text via driveOperationText(section), and error text with "Try Again" button via driveErrorText(section).

- [x] T-011: Redesign StorageWidget.qml with three-state visibility and badge rendering
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005
  - Check: Widget is invisible (zero layout width, no space occupied) when deviceCount == 0; renders dimmed/desaturated icon with no badge when deviceCount > 0 AND hasMounted == false; renders accent-colored icon with numeric count badge (e.g., "2") when deviceCount > 0 AND hasMounted == true; state transitions occur within 500ms of device hotplug or mount-state change.

- [x] T-012: Wire StoragePopupContent.qml with drive-grouped ListView, footer actions, and empty-state
  - REQs: REQ-F-007, REQ-F-016, REQ-F-017, REQ-F-020
  - Check: Popup ListView is grouped by driveId with StorageDriveSection.qml rendering section headers and StorageVolumeCard.qml rendering volume rows beneath each drive; footer contains "Open in Files" and "Show All Devices" buttons invoking correct invokables; empty-state view (icon, "No removable storage", subtitle) renders when last eligible device is removed while popup has focus; popup remains open after device removal until user dismisses via focus loss or Escape key.

- [x] T-014: Redesign popup with hero header and collapsible per-drive sections (post-implementation feedback round 2, per `docs/mockups/udisks2.png`)
  - REQs: REQ-F-007, REQ-F-007a
  - Check: `HnPanelHeader`-based 3-column hero header (icon / title+subtitle / settings icon) renders above the list; each drive section has a collapse/expand toggle independent of other drives; drive header carries icon, name, description, collapse toggle, and eject button (power off added in T-016).

- [x] T-015: Fix "0 devices connected" wording and remove power-off confirmation step (post-implementation feedback round 3)
  - REQs: REQ-F-007a, REQ-F-011
  - Check: Header subtitle reads "No devices connected" (never "0 devices connected") when `deviceCount == 0`; `StorageService::powerOff(targetId)` dispatches immediately with no `confirmationText`/`confirmPowerOff()`/`cancelPowerOff()` step; confirmation banner removed from `StoragePopupContent.qml`; `PowerOffActsImmediatelyAndScopeIncludesHiddenSiblingVolumes` test passes.

- [x] T-016: Add device-connected and widen safe-to-remove notifications (post-implementation feedback round 4)
  - REQs: REQ-F-015, REQ-F-021
  - Check: A newly-appearing drive after startup fires a "{name} connected" notification; a drive present in the initial available controller snapshot does not; eject AND power-off both fire "{name} can be safely removed"; mount/unmount fire no notification. `HotplugAfterStartupDoesNotCrashAndUpdatesDeviceCount` test passes.

- [~] T-013: Verify live compositor behavior for Known Risks and finalize (physical-device checks remain pending; see checklist below)
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-007a, REQ-F-008, REQ-F-009, REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-015, REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-019, REQ-F-020, REQ-F-021, REQ-NF-001, REQ-NF-002, REQ-NF-003, REQ-C-001, REQ-C-002, REQ-C-003, REQ-C-004
  - Check: Verify live in a running Hyprland session: (1) symbolic device-type icons tint correctly (accent/subdued/active) in both light and dark themes (REQ-F-004/005/006); (2) SSD/NVMe heuristic correctly classifies a real USB SSD (e.g., Samsung T7) against live UDisks2 introspection; (3) body-click vs button-click isolation works on edge cases (boundary clicks, clicking during in-flight operation); (4) rapid operations do not cause visible hiccups or scroll-position loss in ListView; (5) popup with 3+ volumes and active error rows scrolls cleanly within the 600×820 fixed surface envelope; (6) D-Bus notification calls (eject/power-off safe-to-remove, and connect) do not noticeably block the GUI thread; (7) per-drive collapse/expand toggle works independently across multiple simultaneously-attached drives; (8) plugging in a device after the shell is already running produces a "connected" toast, while a device already attached at shell startup produces none; (9) mount and unmount produce no toast in either direction.

- [x] T-017: Fix implementation-review regressions
  - REQs: REQ-F-006, REQ-F-007, REQ-F-012, REQ-F-014, REQ-F-015, REQ-F-021, REQ-NF-001
  - Check: Drive-header operation/error/capability bindings update at unchanged row count; startup discovery is silent; hotplug notifies once; storage notifications bypass DND/app rules only for the shell's own bus sender and never enter history; removal notifications retain the original name after the drive disappears; capacity counts physical bytes once for partitioned/encrypted disks; USB SD readers use the flash icon. Dedicated C++ and QML regression tests cover these cases.
  - Icon verification: Qt pixmap lookup resolves all five names under installed `breeze` and `Adwaita`; filename inventory confirms four names are absent as standalone Breeze files. Runtime resolution replaces the previous filename-only acceptance check, and the popup supplies the generic fallback explicitly.
  - Verification: `task test NPROC=2` passed all 1,184 tests; `task qmltypes-check NPROC=2` and `task architecture-check` passed; `task qml-lint NPROC=2` passed with existing AudioService unresolved-type warnings. C++ formatting and `git diff --check` passed. `task compositor-smoke-check` printed the live checklist; physical storage interaction checks in T-013 remain pending.

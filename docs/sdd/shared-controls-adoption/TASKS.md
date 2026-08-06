# Shared Controls Adoption Tasks

**Requirements:** `docs/sdd/shared-controls-adoption/SPEC.md`
**Design:** `docs/sdd/shared-controls-adoption/DESIGN.md`
**Review ledger:** `docs/sdd/shared-controls-adoption/REVIEW-CHECKPOINTS.md`
**Status:** Complete — pipeline closure approved

## Execution Rules

- Complete tasks strictly in listed order.
- Treat every `CP-*` item as an independent visual review stop.
- Do not begin the next item until the current item is explicitly `Approved` or
  `Skipped by decision`.
- Apply the full checkpoint protocol from REQ-F-005 to every `CP-*` task.
- If a checkpoint is `Needs correction`, correct and repeat that checkpoint.
- If a checkpoint is `Blocked upstream`, restore its component, update `UPSTREAM-NOTES.md`, and
  stop for user disposition.
- Update `REVIEW-CHECKPOINTS.md` as evidence is produced. Checkboxes alone never imply approval.

## Foundation

- [x] **CP-F-001 — Inventory controls, candidates, and contracts**
  - Re-read the upstream APIs at the recorded controls baseline and inventory every candidate
    call site, wrapper contract, model role, service action, test, and CMake/import path.
  - Confirm the exclusions and record any mismatch before editing application QML.

- [x] **CP-F-002 — Establish verification and screenshot procedures**
  - Select focused test commands and instantiation coverage for every surface.
  - Prove a repeatable temporary screenshot procedure and baseline naming convention.

- [x] **CP-F-003 — Revalidate canonical Controls discovery**
  - Verify all current `HnSurfaceFrame` consumers and the canonical Controls smoke test.
  - Confirm dynamic discovery from the installed dependency prefix.
  - Add `HolonightQt::Controls` linkage only if a failure proves it necessary.

## HoloNight Settings

- [x] **CP-S-001 — Migrate `NavPanel` to `HnNavigationDelegate`**
- [x] **CP-S-002 — Migrate Appearance headings/dividers to `HnSectionHeader`**
- [x] **CP-S-003 — Migrate theme variants to `HnChoiceCard`**
- [x] **CP-S-004 — Migrate accent choices to `HnChoiceCard`**
- [x] **CP-S-005 — Migrate global shape selection to `HnSegmentedControl`**
- [x] **CP-S-006 — Migrate shape scale and override rows to `HnSettingsRow`**
- [x] **CP-S-007 — Migrate font selectors to `HnIconComboBox`**
- [x] **CP-S-008 — Migrate font-size rows to `HnSettingsRow`**
- [x] **CP-S-009 — Migrate bar workspace/tray rows to `HnSettingsRow`**
- [x] **CP-S-010 — Migrate footer shell state to `HnStatusIndicator`**
- [x] **CP-S-011 — Migrate footer composition to `HnActionBar`**
  - Retain themed standard buttons and all load/save/error behavior.

- [x] **MS-S-001 — Approve the Settings milestone**
  - Require CP-S-001 through CP-S-011 to be approved or explicitly skipped.
  - Run the dark/light × 1.0/1.25 matrix, `task qmltypes-check`, `task test`, Settings workflow,
    and milestone screenshots. Stop for milestone approval.

## Launcher

- [x] **CP-L-001 — Migrate `LauncherSearchField` to `HnSearchField`**
  - Preserve query synchronization, Up/Down movement, Enter launch, Escape close/clear, and public
    focus/clear functions.
- [x] **CP-L-002 — Migrate `LauncherResultRow` to `HnListDelegate` with `HnKeyHint`**
- [x] **CP-L-003 — Migrate `LauncherActionRow` to the delegate matching launch semantics**
- [x] **CP-L-004 — Migrate recent application rows to `HnListDelegate`**
- [x] **CP-L-005 — Migrate category navigation to `HnNavigationDelegate`**
- [x] **CP-L-006 — Migrate search filters to `HnSegmentedControl`**
  - Use explicit values `all`, `applications`, and `actions`; adapt them to the existing filter
    contract without changing results.
- [x] **CP-L-007 — Migrate recent/no-result presentation to `HnEmptyState`**

- [x] **MS-L-001 — Approve the Launcher milestone**
  - Under the user-authorized Launcher review exception, require focused tests and QML lint,
    provide a manual theme/scale and interaction checklist without taking screenshots, and stop
    for milestone approval.

## Network and Audio Popups

- [x] **CP-N-001 — Migrate `NetworkActionRow` to `HnActionDelegate`**
- [x] **CP-N-002 — Migrate `WifiNetworkDelegate` to `HnListDelegate`**
  - Retain strength/security/connected content, connect routing, and password routing.
- [x] **CP-N-003 — Migrate `WifiPasswordDialog` field structure to `HnFormField`**
  - Preserve dialog surface, row/SSID properties, focus, validation, submit, cancel, and accepted
    signal.

- [x] **MS-N-001 — Approve the Network popup milestone**
  - Run the theme/scale matrix, `task qmltypes-check`, `task test`, network service states,
    password workflow, and live compositor screenshots. Stop for milestone approval.

- [x] **CP-A-001 — Migrate `AudioTabSidebar` to `HnNavigationDelegate`**
- [x] **CP-A-002 — Migrate `AudioDeviceDelegate` to `HnListDelegate`**
  - Keep mute and volume controls in application-owned slots.
- [x] **CP-A-003 — Migrate `AudioStreamDelegate` to `HnListDelegate`**
  - Keep mute and volume controls in application-owned slots.
- [x] **CP-A-004 — Migrate `AudioDeviceList` empty state to `HnEmptyState`**
- [x] **CP-A-005 — Migrate `AudioStreamList` empty state to `HnEmptyState`**

- [x] **MS-A-001 — Approve the Audio popup milestone**
  - Run the theme/scale matrix, `task qmltypes-check`, `task test`, device/stream/default/mute/
    volume/empty workflows, and live compositor screenshots. Stop for milestone approval.

## Right Sidebar

- [x] **CP-R-001 — Migrate `SidebarTabButton` to `HnNavigationDelegate`**
- [x] **CP-R-002 — Migrate `DefaultAppRow` to `HnSettingsRow` + `HnIconComboBox`**
- [x] **CP-R-003 — Migrate notification rule rows to `HnSettingsRow` + `HnIconComboBox`**
- [x] **CP-R-004 — Migrate notification empty presentation to `HnEmptyState`**
- [x] **CP-R-005 — Migrate overview notification rows to `HnListDelegate`**
- [x] **CP-R-006 — Migrate upcoming calendar loading presentation to `HnLoadingState`**
- [x] **CP-R-007 — Migrate diagnostic state/progress to `HnStatusIndicator`/`HnLoadingState`**
- [x] **CP-R-008 — Migrate `ChargeLimitRow` to `HnSettingsRow`**
- [x] **CP-R-009 — Migrate `BrightnessSlider` visual row to `HnSettingsRow`**

- [x] **MS-R-001 — Approve the Right Sidebar milestone**
  - Run the theme/scale matrix, `task qmltypes-check`, `task test`, all affected sidebar states,
    and live compositor screenshots. Stop for milestone approval.

## Final Acceptance

- [x] **CP-Z-001 — Reconcile decisions and upstream findings**
  - Confirm every candidate is approved, skipped by explicit decision, or explicitly deferred.
  - Resolve or disposition every upstream ledger entry.

- [x] **CP-Z-002 — Run final repository verification**
  - Run `task format-check`, `task qml-lint`, `task qmltypes-check`, `task test`,
    `git diff --check`, and a full diff review.
  - Confirm no excluded surface, public contract, generated Wayland file, secret, credential,
    dependency lockfile, or unrelated file changed.

- [x] **CP-Z-003 — Obtain explicit pipeline closure approval**
  - Summarize changed files, verification, milestone evidence, skipped/deferred items, and upstream
    dispositions. Do not mark the pipeline complete without user approval.

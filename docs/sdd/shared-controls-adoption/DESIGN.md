# Shared Controls Adoption Design

**Requirements:** `docs/sdd/shared-controls-adoption/SPEC.md`
**Status:** Complete
**Date:** 2026-07-28

## 1. Baseline and Dependency Model

Implementation starts from:

- `holonight-shell`: `a7f0c822c3121018620366f6354700f413d7428a`;
- sibling `holonight-qt`: `00b1b55d88a9d20f527559b8551ad664760c2a7b`.

The sibling revision provides every control named by this design. The shell already dynamically
loads `Holonight.Controls` for four application `HnSurfaceFrame` consumers and canonical control
tests. Existing QML/test import paths point at `/tmp/holonight-qt-prefix/lib/qt6/qml`, and
`QT_SKIP_AUTO_QML_PLUGIN_INCLUSION` supports runtime plugin discovery. Therefore the design
revalidates discovery and adds `HolonightQt::Controls` linkage only in response to a demonstrated
configure, load, or deployment failure.

## 2. Ownership Boundary

The application adapts domain data into stable public component contracts. A shared control
owns its documented visual state, accessibility, semantic geometry, and slot layout. Domain
actions flow back through the existing application signals and services.

For example, an audio delegate may place the existing mute button and `AudioVolumeSlider` in
`HnListDelegate.trailingContent`; `HnListDelegate` does not gain audio knowledge. Likewise,
launcher activation remains an application signal even when row selection is rendered by a
shared delegate.

## 3. Control-to-Component Mapping

| Surface | Existing region | Shared control | Preserved application contract |
| --- | --- | --- | --- |
| Settings | `NavPanel.qml` delegates | `HnNavigationDelegate` | `currentPage`, `pageRequested(pageKey)`, page model |
| Settings | Appearance headings/dividers | `HnSectionHeader` | section content and ordering |
| Settings | Theme variants | `HnChoiceCard` | `editModel.themeScheme` IDs |
| Settings | Accent choices | `HnChoiceCard` | `editModel.themeAccent` IDs |
| Settings | Global shape selector | `HnSegmentedControl` | corner-style IDs and synchronization |
| Settings | Shape scale/overrides | `HnSettingsRow` | sliders, switches, enablement, edit-model writes |
| Settings | Font selectors | `HnIconComboBox` | font models, current index, activated value |
| Settings | Font-size rows | `HnSettingsRow` | range, rounding, and edit-model writes |
| Settings | Bar workspace/tray rows | `HnSettingsRow` | ranges, rounding, and edit-model writes |
| Settings | Footer shell state | `HnStatusIndicator` | dirty/saving/error meaning |
| Settings | Footer composition | `HnActionBar` | load/save actions and themed standard buttons |
| Launcher | `LauncherSearchField.qml` | `HnSearchField` | query sync, focus/clear functions, move/launch/close signals |
| Launcher | `LauncherResultRow.qml` | `HnListDelegate` + `HnKeyHint` | required app roles, hover and activation signals |
| Launcher | `LauncherActionRow.qml` | `HnActionDelegate` or `HnListDelegate` | action launch semantics and `activated()` |
| Launcher | recent rows | `HnListDelegate` | recent-entry lookup and launch |
| Launcher | category navigation | `HnNavigationDelegate` | category value, count, active state |
| Launcher | search filters | `HnSegmentedControl` | explicit `all`, `applications`, `actions` values |
| Launcher | recent/no-result states | `HnEmptyState` | current visibility conditions |
| Network | `NetworkActionRow.qml` | `HnActionDelegate` | settings/info signals and disabled info state |
| Network | `WifiNetworkDelegate.qml` | `HnListDelegate` | roles, connect routing, password request, signal/security content |
| Network | password field structure | `HnFormField` | `ssid`, `row`, `accepted(row,password)`, focus and submit |
| Audio | `AudioTabSidebar.qml` | `HnNavigationDelegate` | `currentTab`, labels, `tabSelected(index)` |
| Audio | device delegate | `HnListDelegate` | model roles, default-device action, mute and volume slots |
| Audio | stream delegate | `HnListDelegate` | stream ID actions, mute and volume slots |
| Audio | device empty state | `HnEmptyState` | `emptyText` and empty-model condition |
| Audio | stream empty state | `HnEmptyState` | `emptyText` and empty-model condition |
| Sidebar | `SidebarTabButton.qml` | `HnNavigationDelegate` | icon, label, active state, badge, `clicked()` |
| Sidebar | `DefaultAppRow.qml` | `HnSettingsRow` + `HnIconComboBox` | filtering, unavailable state, `defaultChanged` |
| Sidebar | notification rule row | `HnSettingsRow` + `HnIconComboBox` | rule roles, switch, urgency write |
| Sidebar | notification empty state | `HnEmptyState` | zero-rule visibility |
| Sidebar | overview notification rows | `HnListDelegate` | grouping, time, overflow, tab routing |
| Sidebar | upcoming calendar loading | `HnLoadingState` | calendar load/error/data state machine |
| Sidebar | diagnostics/progress | `HnStatusIndicator` + `HnLoadingState` | status mapping and refresh/rebuild actions |
| Sidebar | charge-limit row | `HnSettingsRow` | displayed limit/service semantics |
| Sidebar | brightness row | `HnSettingsRow` | throttling, drag behavior, service synchronization |

`LauncherActionRow` is resolved during its checkpoint: use `HnActionDelegate` when the row is a
navigating command with chevron semantics; otherwise use `HnListDelegate` and preserve its direct
launch affordance. This is a bounded choice inside that checkpoint, not permission to redesign it.

## 4. State Preservation

- Delegate `checked`/`highlighted` state mirrors the existing selection source. Application code
  does not reproduce selection fills or rails.
- Existing keys and semantic values remain the source of truth. `HnSegmentedControl.currentIndex`
  is synchronized with those values, and `activated(index, value)` writes the existing value.
- Existing model roles stay explicit on delegates, especially under
  `pragma ComponentBehavior: Bound`.
- Component slot ownership is respected; slot objects are created by the shared control and
  inspected only through its read-only item aliases where focus coordination requires it.
- Focus functions and public signals remain on application wrapper components.
- Empty, loading, and error conditions remain mutually consistent with the current service/model
  state machine. A shared feedback control only renders the chosen state.
- Standard styled `Button`, `Switch`, `Slider`, dialog, and specialized application controls are
  retained when they express behavior not owned by the adopted shared control.

## 5. Sequential Checkpoint Mechanics

`TASKS.md` is the authoritative order and `REVIEW-CHECKPOINTS.md` is the state ledger. At most one
checkpoint may be `In progress` or `Needs correction`. Beginning a checkpoint means recording its
baseline evidence before editing.

Approval advances only one edge:

```text
Pending -> In progress -> Approved -> next Pending checkpoint
                     \-> Needs correction -> In progress
                     \-> Blocked upstream -> user disposition
Pending/In progress -> Skipped by decision (user only)
```

When upstream-blocked, restore only the active component to the last approved repository state,
retain tests or notes only when they remain valid and useful, record the upstream issue, and stop.
The user decides whether to wait, fix upstream in a separate change, or skip/defer.

## 6. Verification Design

### Per checkpoint

1. Focused QtQuickTest or GTest for observable behavior.
2. Direct component creation through the existing QML harness without new QML warnings.
3. `task qml-lint`.
4. `git diff --check`.
5. Live state exercise and temporary baseline/post-change screenshots.

The checkpoint record names exact commands and files; it does not merely state “tests passed.”

### Per surface milestone

Run the dark/light × 1.0/1.25 matrix, `task qmltypes-check`, `task test`, and the relevant live
workflow. Launcher review includes search, Up/Down, Enter, Escape, category/filter, action, recent,
and no-result paths. Popup/sidebar review uses `task compositor-smoke-check` plus the actual
surface interactions.

### Screenshot procedure

Use a consistent theme, scale, model fixture, and crop for baseline/post-change pairs:

1. Create one review directory with
   `mktemp -d /tmp/holonight-shared-controls-review.XXXXXX`.
2. Use filenames
   `<checkpoint>-<baseline|post>-<dark|light>-s<1.0|1.25>-<state>.png`. Keep the checkpoint,
   theme, scale, state, monitor, and capture geometry identical within a comparison pair.
3. In a live Hyprland session, obtain monitor geometry/scale with `hyprctl monitors -j`. Obtain
   Settings window geometry with `hyprctl clients -j` and compositor-surface geometry with
   `hyprctl layers -j`; record the chosen geometry in the checkpoint ledger.
4. Open shell surfaces through the existing control socket. Launch Settings directly with the
   requested `QT_SCALE_FACTOR`. Put the component in the named fixture/state before each capture.
5. Capture with
   `scripts/delayed-screenshot.sh -t 0 -g <X:Y:W:H> <temporary-file>`. Omit `-g` only for a
   diagnostic full-layout capture. Use the same crop for the post-change image.
6. Verify each output with `file <temporary-file>` and optionally record `sha256sum`. Use
   ImageMagick `compare -metric AE` as an aid, not as an approval oracle; present the paired
   images for review.
7. Record paths, state, geometry, and the visual conclusion in `REVIEW-CHECKPOINTS.md`. Remove the
   temporary directory after the checkpoint decision; never add its PNGs to Git.

Live capture requires access to the session Wayland socket. Offscreen tests do not replace this
visual gate.

### Focused verification matrix

The narrowest existing QML execution unit is `test_holonight_qml_harness`; new checkpoint tests
remain individual `TestCase` files/functions within that harness. Use
`ctest --test-dir build -R '^test_holonight_qml_harness$' --output-on-failure` for focused QML
execution, then `task test` only at milestones. C++ tests use anchored CTest regexes for the
affected suite or GTest case. The component-instantiation case remains a separate assertion in
`tests/qml/tst_component_instantiation.qml`, exercised by the same harness.

| Surface | Focused behavior/instantiation coverage |
| --- | --- |
| Settings | `test_holonight_settings` / `tests/test_settings_app.cpp`; add QML `TestCase` coverage for component contracts where interaction cannot be observed through the Settings loader test |
| Launcher | QML cases for search-field key/signals, result/action rows, browse/category/filter/empty states; existing `tst_LauncherActionRow.qml` and `tst_component_instantiation.qml` |
| Network | existing `tst_WifiNetworkDelegate.qml`, `tst_NetworkPopupPolish.qml`, and `tst_component_instantiation.qml`; add password-form signal/focus validation |
| Audio | QML delegate/list/sidebar cases plus `tst_component_instantiation.qml`; anchored audio model/service GTests only when a model/service contract is touched |
| Right sidebar | QML cases for tab/default/rules/feedback/rows and `tst_component_instantiation.qml`; existing `tst_power_extensions.qml`; anchored service tests only if nonvisual behavior changes |

Every visual checkpoint additionally runs `task qml-lint` and `git diff --check`. A QML/CMake
registration change also runs `task qmltypes-check`. Pointer, keyboard, focus, selection,
disabled, empty, loading, and error states are exercised only where applicable and are named in
the checkpoint evidence rather than reported generically.

## 7. Foundation Inventory

Current `HnSurfaceFrame` application consumers are:

- `apps/shell/qml/Controls/HudFrame.qml`;
- `apps/shell/qml/Popups/Network/WifiPasswordDialog.qml`;
- `apps/shell/qml/Popups/Tooltip/TooltipPopup.qml`;
- `apps/shell/qml/Popups/Tray/TrayMenuPopup.qml`.

They establish that canonical Controls imports work today and are not migration candidates except
for the separately scoped password-dialog field composition. The focused canonical import smoke
in `tests/test_qml_smoke.cpp` and popup QML tests provide the discovery baseline.

### 7.1 Upstream API snapshot

The controls baseline exposes the following adoption APIs under
`../holonight-qt/qml/controls/`:

| Control | Inputs used by this pipeline | Interaction/output contract |
| --- | --- | --- |
| `HnNavigationDelegate` | inherited `text`, `checked`, `highlighted`, `enabled`; `title`, `badgeText`, `leadingContent`, `trailingContent` | inherited `clicked()`; selection rendering is upstream-owned |
| `HnSectionHeader` | `titleText`, `descriptionText`, `dividerVisible`, `compact`, content slots | presentation only |
| `HnChoiceCard` | inherited `text`, `checked`, `enabled`; `title`, `description`, `thumbnailContent` | inherited `clicked()` |
| `HnSegmentedControl` | `model`, `textRole`, `valueRole`, `currentIndex` | `currentValue`; `activated(index, value)` |
| `HnSettingsRow` | `titleText`, `descriptionText`, `stacked`, `dividerVisible`, default control and named slots | read-only loaded-item aliases |
| `HnIconComboBox` | inherited ComboBox API; `sizeRole`, `iconRole` | inherited `activated(index)`; `currentIconSource` |
| `HnStatusIndicator` | `status`, `text`, `iconSource`, `dotVisible` | presentation only |
| `HnActionBar` | leading, center, and trailing content; `dividerVisible` | read-only loaded-item aliases |
| `HnSearchField` | inherited TextField API; `sizeRole`, leading/trailing content | inherited text, editing, accepted, and key handling; built-in clear action |
| `HnListDelegate` | inherited `text`, `checked`, `highlighted`, `enabled`; title/subtitle/metadata, content slots, divider | inherited `clicked()` |
| `HnKeyHint` | `text` | presentation only |
| `HnActionDelegate` | inherited delegate API; `description`, `iconSource`, `showChevron` | inherited `clicked()` |
| `HnEmptyState` | icon/title/description, graphic/action content, `sizeRole` | presentation and optional action slot |
| `HnFormField` | label/helper/error text, required/error state, default control | read-only `controlItem` |
| `HnLoadingState` | title/description, `running`, normalized `progress`, action content | presentation and optional action slot |

`HnSelectableDelegate` is the selection foundation for the shared delegates. Its
`checked`/`highlighted` inputs and `selectionStyle` determine the shared fill/indicator; downstream
components must not reproduce those colors.

### 7.2 Candidate call sites and retained contracts

The implementation-start scan found these exact call sites. No moved, renamed, or additional
exact-contract candidates were found.

| Checkpoints | Application paths | Contract details to retain |
| --- | --- | --- |
| CP-S-001 | `apps/settings/qml/NavPanel.qml` | 13-entry `pages` model; required `currentPage`; `pageRequested(pageKey)`; 40-pixel rows and selected-page synchronization |
| CP-S-002–006 | `apps/settings/qml/AppearancePage.qml` | local `SectionGroup`; theme/accent/corner IDs; shape-scale and base radius/chamfer enablement and writes |
| CP-S-007–008 | `apps/settings/qml/AppearancePage.qml` | `FontListModel` instances, current font synchronization, activated font values, integer font-size writes |
| CP-S-009 | `apps/settings/qml/BarPage.qml` | workspace/tray ranges, rounding, and `editModel` writes |
| CP-S-010–011 | `apps/settings/qml/FooterBar.qml` | dirty/saving/error display; reload/save actions and existing themed buttons |
| CP-L-001 | `apps/shell/qml/Launcher/LauncherSearchField.qml`, consumed by `Launcher.qml` | two-way query synchronization; `forceInputFocus()`, `clearInput()`; move/launch/close signals and key behavior |
| CP-L-002–003 | `LauncherResultRow.qml`, `LauncherActionRow.qml`, and both delegates in `Launcher.qml` | required result/action roles; selected state; hover selection; application/action launch routing |
| CP-L-004–005 | `LauncherRightPanelBrowse.qml` | recent-entry lookup/launch and refresh; category value/count/current state and `setActiveCategory()` |
| CP-L-006–007 | `Launcher.qml`, `LauncherRightPanelSearch.qml`, `LauncherRightPanelBrowse.qml` | current `""`/`apps`/`actions` filter adapter; browse/search/recent empty conditions |
| CP-N-001 | `apps/shell/qml/Popups/Network/NetworkActionRow.qml` | settings/info signals and disabled information action |
| CP-N-002 | `WifiNetworkDelegate.qml`, instantiated by `WifiNetworkList.qml` | index/SSID/strength/security/known/connected/count roles; row alias; password request and connect routing |
| CP-N-003 | `WifiPasswordDialog.qml`, opened by `NetworkPopupContent.qml` | `ssid`, `row`, `accepted(row,password)`, open focus, validation, submit, cancel, popup surface |
| CP-A-001 | `apps/shell/qml/Popups/Audio/AudioTabSidebar.qml` | `currentTab`, translated labels, `tabSelected(index)` |
| CP-A-002–005 | `AudioDeviceDelegate.qml`, `AudioStreamDelegate.qml`, `AudioDeviceList.qml`, `AudioStreamList.qml` | device/stream model roles and service calls; default/mute/volume behavior; list `emptyText` and empty condition |
| CP-R-001 | `apps/shell/qml/RightSidebar/SidebarTabButton.qml`, instantiated by `SidebarTabBar.qml` | tab index, icon, label, active state, badge count, `clicked()` |
| CP-R-002 | `Tabs/System/DefaultAppRow.qml`, instantiated by `SidebarSystem.qml` | MIME/category filtering, candidates, unavailable state, current-index synchronization, `defaultChanged` |
| CP-R-003–004 | `Tabs/Notifications/SidebarNotifications.qml` | notification-rule roles, enable/urgency writes, and zero-rule presentation |
| CP-R-005 | `Tabs/Overview/SidebarOverviewNotifications.qml` | grouped recent notifications, relative time, overflow, and `switchTab(index)` |
| CP-R-006 | `Tabs/Overview/SidebarOverviewUpcoming.qml` | calendar loading/error/data state machine and retry behavior |
| CP-R-007 | `Tabs/System/SidebarSystem.qml` | diagnostic status/detail/guidance mapping and mutually exclusive refresh/rebuild operations |
| CP-R-008–009 | `Tabs/QuickSettings/ChargeLimitRow.qml`, `BrightnessSlider.qml` | unavailable visibility, displayed limit, throttled brightness writes, drag/commit, and service synchronization |

### 7.3 Registration, models, services, and tests

- Settings QML is registered by `apps/settings/CMakeLists.txt`; shell QML is registered by
  `apps/shell/CMakeLists.txt`. The inventory found no candidate requiring a new component file or
  registration change.
- The affected application service boundary comprises `LauncherService`, `RecentAppsTracker`,
  `NetworkService`, `AudioService`, `NotificationService`, `NotificationRuleModel`,
  `CalendarService`, `SessionIntegrationService`, `BrightnessService`, and the charge-limit
  state exposed by `BatteryService`. These calls remain downstream-owned.
- Existing focused QML coverage includes `tests/qml/tst_LauncherActionRow.qml`,
  `tst_WifiNetworkDelegate.qml`, `tst_NetworkPopupPolish.qml`, `tst_power_extensions.qml`, and
  `tst_component_instantiation.qml`. Settings loading is covered in `tests/test_settings_app.cpp`;
  canonical Controls loading is covered in `tests/test_qml_smoke.cpp`.
- Audio model/service roles are independently covered by `tests/test_audio_device_model.cpp`,
  `tests/test_audio_stream_model.cpp`, `tests/test_audio_service.cpp`, and
  `tests/test_pulse_audio_backend.cpp`. Each visual checkpoint still adds or updates the narrowest
  observable QML behavior test before migration.

### 7.4 Inventory conclusions

- The component map in Section 3 remains complete at the recorded baselines.
- The Launcher filter has an intentional adapter requirement: the shared control will expose
  `all`, `applications`, and `actions`, while the current search panel consumes `""`, `apps`, and
  `actions`. This is already contained by CP-L-006 and is not a public behavior change.
- `HnApplicationWindow`, `HnTextArea`, and `HnCardDelegate` still have no exact matching downstream
  contract. The exclusions remain unchanged.
- No upstream API mismatch blocks the planned checkpoints at inventory time. Component-level
  geometry and interaction compatibility must still be proven at each visual checkpoint.

## 8. Rejected Alternatives

- A bulk surface migration is rejected because visual and behavioral regressions could not be
  attributed or reviewed independently.
- Downstream copies or visual patches around a deficient shared control are rejected because they
  split the design contract and hide reusable upstream defects.
- Automatic `HolonightQt::Controls` linkage is rejected without evidence because canonical module
  loading already works dynamically.
- `HnApplicationWindow`, `HnTextArea`, and `HnCardDelegate` adoption is rejected without a newly
  discovered exact contract.
- Changing the Settings window ownership or compositor surface policy is outside this visual
  adoption.

## 9. Requirement Traceability

| Requirement | Design coverage |
| --- | --- |
| REQ-F-001, REQ-F-002 | Sections 2–4 |
| REQ-F-003, REQ-F-004 | Sections 2, 3, 5 |
| REQ-F-005, REQ-F-006 | Sections 5, 6 |
| REQ-F-007, REQ-F-008 | Section 5 |
| REQ-F-009 | Sections 1, 7 |
| REQ-F-010, REQ-F-011 | Sections 5, 6 |
| Non-functional requirements | Sections 2, 4, 6, 8 |

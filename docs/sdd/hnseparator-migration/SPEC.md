# HnSeparator Migration Specification

## Overview

This specification formalize the migration of the holonight-shell codebase from the local, homegrown `ContentSeparator` QML component (defined in `qml/HoloNight/Components/ContentSeparator.qml` within the `Holonight.Components` module) to `HnSeparator`, a canonical shared-design-system control already installed system-wide at `/usr/lib/qt6/qml/Holonight/Controls/HnSeparator.qml` (QML module `Holonight.Controls`).

This migration follows the same pattern as prior successful adoptions of shared controls in this codebase (e.g., right sidebar, audio popup, network popup). At completion, `ContentSeparator` will be fully removed from the codebase and deleted from the source tree; all references will point to `HnSeparator`.

## Scope & Call-Site Inventory

The migration affects **16 ContentSeparator instances across 7 files**:

| File | Line(s) | Count | Orientation | Type/FadeMode | Notes |
|------|---------|-------|-------------|---------------|-------|
| `apps/shell/qml/Launcher/Launcher.qml` | ~326, ~357 | 2 | V, H | Center (implicit), Center (implicit) | Parent: plain Item; explicit `size` property; `type` not set — relies on default |
| `apps/shell/qml/Launcher/LauncherRightPanelBrowse.qml` | ~90 | 1 | H | End | Parent: plain Item; explicit `size` property |
| `apps/shell/qml/Popups/Network/NetworkPopupContent.qml` | ~26, ~134 | 2 | H | None | Parent: ColumnLayout; Layout.fillWidth+size; explicit `thickness` override |
| `apps/shell/qml/Popups/Weather/WeatherCurrentSection.qml` | ~96 | 1 | H | End | Parent: plain Item; explicit `size` property |
| `apps/shell/qml/Popups/Weather/WeatherDailyCards.qml` | ~61 | 1 | V | Center (implicit) | Parent: Row/Item; explicit `size` property; `visible` binding; `type` not set — relies on default |
| `apps/shell/qml/Popups/Weather/WeatherPopupContent.qml` | 86, 100, 117, 145, 232, 283, 331, 358 | 8 | Mixed | Center (implicit) at 86,100,117,145,331,358; End (explicit) at 232,283 | Parent: plain Items; explicit `size` properties; mixed `type`/`opacity` values |
| `apps/shell/qml/Topbar/WorkspaceSection.qml` | ~121 | 1 | V | Center | Parent: Component; explicit `height: 48`; animated `opacity`; `objectName` set |

**Total: 16 instances, 7 files.**

## Property API Mapping

### REQ-F-001: Direction → Orientation Mapping
**EARS Template:** Ubiquitous

All `ContentSeparator` instances must have their `direction` property replaced with `orientation` using the following mapping:
- `ContentSeparator.Direction.Horizontal` → `Qt.Horizontal`
- `ContentSeparator.Direction.Vertical` → `Qt.Vertical`

**Acceptance Criterion:**
- All 16 instances use `orientation: Qt.Horizontal` or `Qt.Vertical` in the migrated code.
- No `direction` property appears in any migrated QML file.

### REQ-F-002: Type → FadeMode Mapping
**EARS Template:** Ubiquitous

All `ContentSeparator` instances with a `type` property must be replaced with `fadeMode` using the following exact mapping:
- `ContentSeparator.Type.Center` → `HnSeparator.FadeBoth`
- `ContentSeparator.Type.Start` → `HnSeparator.FadeStart`
- `ContentSeparator.Type.End` → `HnSeparator.FadeEnd`
- `ContentSeparator.Type.None` → `HnSeparator.Solid`

**Correction (verified against source):** `ContentSeparator.qml` line 20 declares `property int type: ContentSeparator.Type.Center` — the implicit default is **Center**, not None. Instances with no explicit `type` property currently render with `Center` behavior and must map to `HnSeparator.FadeBoth`, not `Solid`. This affects 9 instances that omit `type:` entirely:
- `Launcher.qml`: both instances (~326, ~357)
- `WeatherDailyCards.qml`: 1 instance (~61)
- `WeatherPopupContent.qml`: 6 of 8 instances (lines 86, 100, 117, 145, 331, 358 — only 232 and 283 explicitly set `type: End`)

**Acceptance Criterion:**
- All 16 instances have explicit `fadeMode` property set, derived either from an existing explicit `type` or from the implicit `Center` default (→ `FadeBoth`) for the 9 instances listed above.
- No `type` property appears in any migrated QML file.

### REQ-F-003: Color Override Removal
**EARS Template:** Ubiquitous

All explicit `color: HoloniightPalette.borderPassive` overrides must be removed from `ContentSeparator` instances. The migrated `HnSeparator` will use its own built-in default (`HoloniightPalette.borderSubtle`). This is a deliberate, approved visual change.

**Instances affected:** All 16 instances currently set `color: HoloniightPalette.borderPassive` (either explicitly in code or inherited from a parent wrapper).

**Acceptance Criterion:**
- No explicit `color:` property assignment appears on any migrated `HnSeparator` instance (except if a different custom color was set — verify none are in scope; none are).
- Visual inspection confirms separators render with the new `borderSubtle` token (slightly lighter than `borderPassive`).

### REQ-F-004: Thickness Handling (Selective Override)
**EARS Template:** Conditional

The `thickness` property must be handled differently based on context:

**Case A (NetworkPopupContent.qml only):** Two instances explicitly set `thickness: root.sectionSeparatorThickness` (a device-pixel-ratio hairline value). This override must be preserved exactly on the migrated `HnSeparator` instances.

**Case B (all other 14 instances):** Drop the `thickness` property entirely. The migrated `HnSeparator` will use its own default (`HoloniightPalette.separatorWidth`). This is an approved visual change.

**Acceptance Criterion:**
- The two `HnSeparator` instances in `NetworkPopupContent.qml` have explicit `thickness: root.sectionSeparatorThickness` properties set.
- All other 14 instances do not have a `thickness` property defined.

### REQ-F-005: Size → Explicit Dimension Conversion (Conditional)
**EARS Template:** Conditional

The `size` property (a horizontal or vertical extent) must be converted based on the parent's container type:

**Rule 5A: Layout parents (ColumnLayout/RowLayout with existing Layout attached properties):**
- Applies to: Both instances in `NetworkPopupContent.qml` (already have `Layout.fillWidth: true`, `Layout.minimumHeight`, `Layout.preferredHeight`).
- Action: Remove the `size` property entirely. The existing Layout attached properties handle sizing.

**Rule 5B: Non-Layout parents (plain Item, Row, Column):**
- Applies to: All 14 remaining instances.
- Action: Convert `size: X` into an explicit dimension property on the `HnSeparator`:
  - If orientation was Vertical: convert `size: X` → `height: X`
  - If orientation was Horizontal: convert `size: X` → `width: X`

**Acceptance Criterion:**
- The two `NetworkPopupContent.qml` instances do not have a `size` property; their Layout attached properties are intact.
- All 14 other instances have an explicit `height: X` or `width: X` property matching the original `size: X` value.
- No instance has a `size` property in the migrated code.

### REQ-F-006: Edge/Center Opacity Preservation
**EARS Template:** Ubiquitous

The `centerOpacity` and `edgeOpacity` properties are identical on both `ContentSeparator` and `HnSeparator`. These must be carried over unchanged wherever they are present in the source.

**Instances affected:** At least the two instances in `NetworkPopupContent.qml` set `centerOpacity: root.sectionSeparatorOpacity`.

**Acceptance Criterion:**
- All instances with explicit `centerOpacity` or `edgeOpacity` properties retain those properties exactly as currently written.
- No loss of opacity properties in migration.

### REQ-F-007: Outer Item Opacity Preservation
**EARS Template:** Ubiquitous

The outer QtQuick `opacity` property (e.g., `opacity: 0.5`, `opacity: 0.32`, or `opacity: 0` with animated behavior) is a builtin Item property present on both components. This property must be preserved exactly as-is on all instances, including any bound behaviors or event handlers (e.g., `Component.onCompleted: opacity = 1`, `Behavior on opacity { ... }`).

**Instances affected:** Nearly all 16 instances set an explicit outer `opacity` value; WorkspaceSection.qml additionally animates it via `Behavior`.

**Acceptance Criterion:**
- All instances with explicit `opacity` properties retain them unchanged.
- WorkspaceSection.qml's `Component.onCompleted: opacity = 1` and `Behavior on opacity { NumberAnimation { ... } }` are preserved verbatim.
- No regression in fade-in/animation behavior for any affected component.

## Import Fixups

### REQ-F-008: Add/Remove Holonight.Controls and Holonight.Components Imports
**EARS Template:** Ubiquitous

The following import fixups must be applied to each of the 7 affected files:

| File | Remove Import | Add Import | Keep Reason |
|------|---------------|-----------|------------|
| `apps/shell/qml/Launcher/Launcher.qml` | `import Holonight.Components` | (None; already has `import Holonight.Controls`) | ContentSeparator only use in file; file already uses other Controls |
| `apps/shell/qml/Launcher/LauncherRightPanelBrowse.qml` | (None; keep it) | (None; already has `import Holonight.Controls`) | File also uses `ExternalIcon` from Holonight.Components |
| `apps/shell/qml/Popups/Network/NetworkPopupContent.qml` | `import Holonight.Components` | `import Holonight.Controls` | Only ContentSeparator was imported from Components |
| `apps/shell/qml/Popups/Weather/WeatherCurrentSection.qml` | `import Holonight.Components` | `import Holonight.Controls` | Only ContentSeparator was imported from Components |
| `apps/shell/qml/Popups/Weather/WeatherDailyCards.qml` | `import Holonight.Components` | `import Holonight.Controls` | Only ContentSeparator was imported from Components |
| `apps/shell/qml/Popups/Weather/WeatherPopupContent.qml` | `import Holonight.Components` | `import Holonight.Controls` | Only ContentSeparator was imported from Components |
| `apps/shell/qml/Topbar/WorkspaceSection.qml` | `import Holonight.Components` | `import Holonight.Controls` | Only ContentSeparator was imported from Components |

**Acceptance Criterion:**
- The 6 files listed above that require removal of `import Holonight.Components` no longer have that import statement.
- All 7 files have `import Holonight.Controls` present (6 newly added, 1 already present).
- LauncherRightPanelBrowse.qml retains its `import Holonight.Components` statement (for ExternalIcon).
- Launcher.qml does not break due to missing ContentSeparator import (it is removed cleanly).
- No other imports in any file are accidentally removed or altered.

## Removal & Build-Script Requirements

### REQ-F-009: Delete ContentSeparator.qml Source File
**EARS Template:** Ubiquitous

After all 16 call sites have been migrated to `HnSeparator`, the source file defining `ContentSeparator` must be permanently deleted.

**Target file:** `qml/HoloNight/Components/ContentSeparator.qml`

**Acceptance Criterion:**
- The file `qml/HoloNight/Components/ContentSeparator.qml` does not exist in the repository after the migration is complete.
- Deletion is verified via `git status` or `ls -la qml/HoloNight/Components/` showing the file is absent.

### REQ-F-010: Update CMakeLists.txt to Remove ContentSeparator Registration
**EARS Template:** Ubiquitous

The build-script registration of `ContentSeparator.qml` must be removed from the CMake configuration. The `Holonight.Components` module itself remains (it still exports `ExternalIcon` and other components).

**Target file:** `qml/HoloNight/CMakeLists.txt`

**Action:** Remove the line containing `Components/ContentSeparator.qml` from the `HOLONIGHT_COMPONENT_QML_FILES` list. Do not remove the module definition or other files in the Components directory.

**Acceptance Criterion:**
- `qml/HoloNight/CMakeLists.txt` does not contain any reference to `ContentSeparator.qml` in `HOLONIGHT_COMPONENT_QML_FILES`.
- The `Holonight.Components` module definition remains intact.
- `Components/ExternalIcon.qml` and other non-separator components remain registered.
- `task build` completes without CMake errors or warnings about missing ContentSeparator.qml.

### REQ-F-011: Delete Unit Test File tst_ContentSeparator.qml
**EARS Template:** Ubiquitous

The dedicated unit test for the `ContentSeparator` component must be deleted. No replacement test is being written (HnSeparator is an external shared control, already tested upstream; the same treatment is applied to HnIcon and other adopted controls).

**Target file:** `tests/qml/tst_ContentSeparator.qml`

**Acceptance Criterion:**
- The file `tests/qml/tst_ContentSeparator.qml` does not exist after migration.
- `task test` runs without errors related to missing tst_ContentSeparator.qml.

### REQ-F-012: Update test_qml_smoke.cpp to Remove ContentSeparator qmldir Entry
**EARS Template:** Ubiquitous

The `componentsQmldir()` helper function in the QML smoke test generates a dynamic qmldir string. The line registering `ContentSeparator` must be removed.

**Target file:** `tests/test_qml_smoke.cpp`

**Action:** Locate the `componentsQmldir()` function. Find and remove the line:
```
"ContentSeparator 1.0 %1ContentSeparator.qml\n"
```

Keep all other lines intact, including the `ExternalIcon` registration and `module Holonight.Components` declaration.

**Acceptance Criterion:**
- `tests/test_qml_smoke.cpp` no longer contains the string `ContentSeparator 1.0` in the qmldir generator.
- The `module Holonight.Components` line remains in the generated qmldir.
- `ExternalIcon` registration remains intact.
- `task test` (QML smoke test) passes without errors.

### REQ-F-013: Verify Complete Removal from Live Codebase
**EARS Template:** Ubiquitous (Verification)

After all removals are complete, a comprehensive grep search must confirm zero references to `ContentSeparator` remain in the live codebase.

**Acceptance Criterion:**
```bash
grep -r "ContentSeparator" \
  apps/ qml/ tests/ \
  --include="*.qml" --include="*.cpp" --include="*.h" --include="CMakeLists.txt" \
  --exclude-dir="docs"
```
This command must return **zero matches** (exit code 0, no output).

Historical references in `docs/sdd/` directories (documenting past features) are explicitly out of scope and may be left untouched.

## Testing & Verification

### REQ-NF-001: Regression Testing via Existing Consumer Tests
**EARS Template:** Ubiquitous

No new dedicated test file will be written for `HnSeparator` (it is an external shared control, already tested upstream). Existing consumer tests that exercise the affected QML files must continue to pass unchanged after migration.

**Acceptance Criterion:**
- All existing tests that indirectly exercise the 7 affected files pass without modification:
  - QML unit tests for WorkspaceSection, Launcher, Network popup, Weather popup, etc.
  - `task test` (GTest + QtQuickTest harness) completes with zero failures.
  - No test updates or rewrites are required due to the migration.

### REQ-NF-002: No Visual Regression Beyond Approved Token/Thickness Changes
**EARS Template:** Unwanted-Behaviour Prevention

The migration must not introduce visual regressions except for the two approved, deliberate changes:
1. Separator color: `borderPassive` → `borderSubtle` (user-approved, slightly lighter shade).
2. Thickness (where not explicitly overridden): defaults to `separatorWidth` instead of no explicit override (user-approved).

**Acceptance Criterion:**
- Live manual testing of the affected UI sections (Launcher, Weather popup, Network popup, Topbar workspace section) confirms separators render correctly with expected position, orientation, fade modes, and opacity animations.
- No visual glitches (misalignment, clipping, disappearance, unintended color shifts beyond the two approved changes).
- Fade animations (FadeBoth, FadeStart, FadeEnd) render smoothly on live display.

### REQ-NF-003: qml-lint Must Pass Without Errors
**EARS Template:** Ubiquitous

The QML linter must produce no errors on any of the 7 migrated files.

**Acceptance Criterion:**
- `task qml-lint` completes with zero errors reported on any of the 7 files listed in the call-site inventory.
- `HnSeparator` type is resolved correctly by qml-lint (qml-lint CMake target includes `-I "/tmp/holonight-qt-prefix/lib/qt6/qml"`, which syncs the system-installed module tree).

### REQ-NF-004: Build Must Complete Successfully
**EARS Template:** Ubiquitous

The full project build must complete without errors or CMake configuration warnings.

**Acceptance Criterion:**
- `task build` completes successfully.
- No compiler errors or warnings related to missing ContentSeparator or HnSeparator resolution issues.
- `build/holonight-shell` binary is generated.

### REQ-NF-005: All Tests Must Pass
**EARS Template:** Ubiquitous

The full test suite (C++ and QML tests) must pass without regressions.

**Acceptance Criterion:**
- `task test` (including `task configure-tests` prerequisite) completes with all tests passing (100% pass rate).
- No test failures in consumer tests that exercise the migrated files.
- No new test warnings or errors introduced.

## Constraints

### REQ-C-001: Do Not Modify Upstream HnSeparator
**EARS Template:** Ubiquitous

The `HnSeparator.qml` implementation in the system-wide shared module (`/usr/lib/qt6/qml/Holonight/Controls/HnSeparator.qml`) must not be modified. Migration is a consumer-side adopt operation only.

**Acceptance Criterion:**
- No edits to `/usr/lib/qt6/qml/Holonight/Controls/HnSeparator.qml`.
- No files in `holonight-qt` (the upstream library) are touched.
- holonight-shell acts as a pure consumer of the already-installed shared control.

### REQ-C-002: Do Not Touch Unrelated Hand-Rolled Separators
**EARS Template:** Ubiquitous

The `currentConnectionSeparator` rectangle in `NetworkPopupContent.qml` is a hand-rolled separator (a plain `Rectangle` element, not a `ContentSeparator` or `HnSeparator` instance) and is out of scope. It must not be modified or migrated.

**Acceptance Criterion:**
- The `currentConnectionSeparator` Rectangle in `NetworkPopupContent.qml` remains unchanged.
- No attempt to convert or replace this custom separator.
- Grep for `currentConnectionSeparator` confirms the element is untouched in the final code.

### REQ-C-003: Do Not Modify Other Hn* Control Adoption Efforts
**EARS Template:** Ubiquitous

This migration stands alone. Other past or ongoing adoptions of shared controls (HnIcon, HnButton, etc.) must not be modified or revisited as part of this effort.

**Acceptance Criterion:**
- No files other than the 7 listed in the call-site inventory are edited.
- No changes to other Hn* adoptions in Launcher, RightSidebar, AudioPopup, or other components.
- Scope is strictly bounded to the 16 ContentSeparator → HnSeparator conversions.

### REQ-C-004: Do Not Modify Historical docs/sdd Records
**EARS Template:** Ubiquitous

Historical SDD documentation records for past features (e.g., `docs/sdd/weather-icon-compositor/`, `docs/sdd/poc-readiness-review/`) may mention `ContentSeparator` or other past design decisions. These files are out of scope and must not be modified.

**Acceptance Criterion:**
- No files in `docs/sdd/` directories (other than this spec itself and any Stage 2+ artifacts created by this SDD cycle) are edited.
- Historical references remain as-is for future reference.

## Non-Goals

1. **Pixel-perfect fade-gradient fidelity for Start/End modes:** HnSeparator's middle gradient stop for FadeStart/FadeEnd modes may average center+edge opacity slightly differently than ContentSeparator's pure-center middle stop. This cosmetic difference is accepted and approved. No compensation or workaround is required.

2. **New dedicated HnSeparator test file:** HnSeparator is an external shared control, already tested upstream. The same "no new test" approach is applied to HnIcon and other adopted controls. Existing consumer tests provide the regression gate.

3. **Modify the holonight-qt upstream library:** HnSeparator.qml and all supporting infrastructure in `/usr/lib/qt6/qml/Holonight/Controls/` must not be touched. This is a consumer migration only.

4. **Migrate or modify currentConnectionSeparator:** The hand-rolled Rectangle separator in NetworkPopupContent.qml is deliberately left as-is. This spec does not attempt to unify or convert custom separators.

## Acceptance Criteria Summary

### Functional Completeness
- [ ] All 16 ContentSeparator instances replaced with HnSeparator
- [ ] Direction → Orientation mapping applied to all instances
- [ ] Type → FadeMode mapping applied to all instances
- [ ] Color overrides removed (approved visual change)
- [ ] Thickness selective override preserved (NetworkPopupContent only)
- [ ] Size → height/width conversion applied (14 instances) or removed (2 Layout instances)
- [ ] Opacity and opacity animations preserved
- [ ] Import statements updated in all 7 files
- [ ] ContentSeparator.qml deleted
- [ ] CMakeLists.txt updated (ContentSeparator.qml line removed)
- [ ] tst_ContentSeparator.qml deleted
- [ ] test_qml_smoke.cpp updated (qmldir entry removed)
- [ ] Zero grep matches for "ContentSeparator" in live codebase (excluding docs/)

### Quality Gates
- [ ] task qml-lint passes
- [ ] task build succeeds
- [ ] task test passes (100% pass rate)
- [ ] No visual regressions beyond approved token/thickness changes
- [ ] No compiler errors or CMake warnings
- [ ] No unrelated files modified

### Documentation & Verification
- [ ] Manual testing confirms fade animations and opacity behavior on live Wayland session
- [ ] Grep verification confirms complete removal from live codebase
- [ ] All 7 affected files compile and function without errors

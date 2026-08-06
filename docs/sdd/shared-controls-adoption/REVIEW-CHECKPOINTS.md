# Shared Controls Adoption Review Checkpoints

**Status:** Complete — pipeline closure approved
**Allowed states:** `Pending`, `In progress`, `Needs correction`, `Blocked upstream`,
`Skipped by decision`, `Approved`

This ledger is the durable review record. A task checkbox is not approval. For each active
checkpoint, replace the placeholder in its evidence record with baseline behavior/geometry,
relevant states, focused tests, instantiation result, lint/diff checks, temporary screenshot
identifiers, corrections, and the user's dated decision.

## Checkpoint Register

| ID | Scope | State | Decision |
| --- | --- | --- | --- |
| CP-F-001 | Control/candidate/contract inventory | Approved | Approved by user, 2026-07-28 |
| CP-F-002 | Verification and screenshot procedure | Approved | Approved by user, 2026-07-28 |
| CP-F-003 | Dynamic Controls discovery and `HnSurfaceFrame` consumers | Approved | Approved by user, 2026-07-28 |
| CP-S-001 | Settings `NavPanel` | Approved | Approved by user, 2026-07-28 |
| CP-S-002 | Appearance section headers | Approved | Approved by user, 2026-07-28 |
| CP-S-003 | Theme variants | Approved | Approved by user, 2026-07-28 |
| CP-S-004 | Accent choices | Approved | Approved by user, 2026-07-28 |
| CP-S-005 | Global shape selector | Approved | Approved by user, 2026-07-28 |
| CP-S-006 | Shape scale/override rows | Approved | Approved by user, 2026-07-28 |
| CP-S-007 | Font selectors | Approved | Approved by user, 2026-07-29 |
| CP-S-008 | Font-size rows | Approved | Approved by user, 2026-07-29 |
| CP-S-009 | Bar workspace/tray rows | Approved | Approved by user, 2026-07-29 |
| CP-S-010 | Footer shell state | Approved | Approved by user, 2026-07-29 |
| CP-S-011 | Footer composition | Approved | Approved by user, 2026-07-29 |
| MS-S-001 | Settings milestone | Approved | Approved by user, 2026-07-29 |
| CP-L-001 | Launcher search field | Approved | Approved by user, 2026-07-29 |
| CP-L-002 | Launcher result row | Approved | Approved by user, 2026-07-29 |
| CP-L-003 | Launcher action row | Approved | Approved by user, 2026-07-29 |
| CP-L-004 | Recent application rows | Approved | Approved by user, 2026-07-29 |
| CP-L-005 | Category navigation | Approved | Approved by user, 2026-07-29 |
| CP-L-006 | Search filters | Approved | Approved by user, 2026-07-29 |
| CP-L-007 | Recent/no-result empty state | Approved | Approved by user, 2026-07-29 |
| MS-L-001 | Launcher milestone | Approved | Approved by user, 2026-07-29 |
| CP-N-001 | Network action row | Approved | Approved by user, 2026-07-29 |
| CP-N-002 | Wi-Fi network delegate | Approved | Approved by user, 2026-07-29 |
| CP-N-003 | Wi-Fi password form | Approved | Approved by user, 2026-07-29 |
| MS-N-001 | Network milestone | Approved | Approved by user, 2026-07-29 |
| CP-A-001 | Audio tab sidebar | Approved | Approved after keyboard-focus correction, 2026-07-29 |
| CP-A-002 | Audio device delegate | Approved | Approved by user, 2026-07-29 |
| CP-A-003 | Audio stream delegate | Approved | Approved by user, 2026-07-29 |
| CP-A-004 | Audio device empty state | Approved | Approved by user, 2026-07-29 |
| CP-A-005 | Audio stream empty state | Approved | Approved by user, 2026-07-29 |
| MS-A-001 | Audio milestone | Approved | Approved by user, 2026-07-29 |
| CP-R-001 | Sidebar tab button | Approved | Approved by user, 2026-07-29 |
| CP-R-002 | Default application row | Approved | Approved after corrections, 2026-07-29 |
| CP-R-003 | Notification rule row | Approved | Approved after corrections, 2026-07-29 |
| CP-R-004 | Notification empty state | Approved | Approved by user, 2026-07-29 |
| CP-R-005 | Overview notification rows | Approved | Approved by user, 2026-07-29 |
| CP-R-006 | Upcoming calendar loading state | Approved | Approved by user, 2026-07-29 |
| CP-R-007 | Diagnostics and refresh/rebuild progress | Approved | Approved after corrections, 2026-07-29 |
| CP-R-008 | Charge-limit row | Approved | Approved by user, 2026-07-29 |
| CP-R-009 | Brightness row | Approved | Approved by user, 2026-07-29 |
| MS-R-001 | Sidebar milestone | Approved | Approved by user, 2026-07-29 |
| CP-Z-001 | Decision/upstream reconciliation | Approved | Approved by user, 2026-07-29 |
| CP-Z-002 | Final verification | Approved | Approved by user, 2026-07-29 |
| CP-Z-003 | Pipeline closure | Approved | Approved by user, 2026-07-29 |

## Active Checkpoint Evidence

### CP-F-001 — Control/candidate/contract inventory

- **State:** Approved
- **Started:** 2026-07-28
- **Baseline commit:** shell `a7f0c822c3121018620366f6354700f413d7428a`; controls
  `00b1b55d88a9d20f527559b8551ad664760c2a7b`
- **Files in scope:** `DESIGN.md`, `TASKS.md`, and this ledger; read-only inspection of the
  candidate QML, tests, CMake registration, and sibling control QML listed in DESIGN §7
- **Public contract:** Inventory only; no application contract or runtime file changed
- **Baseline behavior and geometry:** Candidate contracts and currently fixed geometry are
  recorded in DESIGN §§3 and 7.2; visual geometry remains subject to each component baseline
- **States exercised:** Not applicable to a documentation-only foundation inventory
- **Focused tests added/updated:** None; no runtime behavior changed
- **Commands and results:** `rg`/`find` inventory of application QML, tests, registrations, and
  `../holonight-qt/qml/controls` completed; both recorded baseline revisions still match `HEAD`
- **Baseline screenshot:** Not applicable; screenshot procedure is CP-F-002
- **Post-change screenshot:** Not applicable
- **Visual comparison:** Not applicable
- **Corrections:** None
- **Upstream note:** None; no inventory-time API mismatch found
- **User decision:** Approved
- **Decision date:** 2026-07-28

### CP-F-002 — Verification and screenshot procedure

- **State:** Approved
- **Started:** 2026-07-28
- **Baseline commit:** shell `a7f0c822c3121018620366f6354700f413d7428a`; controls
  `00b1b55d88a9d20f527559b8551ad664760c2a7b`
- **Files in scope:** `DESIGN.md`, `TASKS.md`, and this ledger; read-only inspection of
  `Taskfile.yml`, `tests/CMakeLists.txt`, `docs/qml-testing.md`, and screenshot scripts
- **Public contract:** Procedure only; no application or test contract changed
- **Baseline behavior and geometry:** The live session exposes eDP-1 at 2560×1600 scale 1.25
  and DP-5 at 3840×2160 scale 1.50; exact per-component geometry will be recorded with each pair
- **States exercised:** Live Wayland capture prerequisite and temporary-file validation
- **Focused tests added/updated:** None; the focused command/test-file matrix is recorded in
  DESIGN §6
- **Commands and results:** tool discovery found `grim`, `hyprctl`, `qmltestrunner`, and `ctest`;
  `ctest --test-dir build -N` discovered 1070 tests including
  `test_holonight_qml_harness`; the first sandboxed capture failed with
  `failed to create display`, and the approved live-session retry succeeded
- **Baseline screenshot:** Procedure proof
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-f-002-proof-session-current-full-layout.png`;
  6912×2160 RGBA PNG; SHA-256
  `8e70a4cb17d05113fdd044aa10f6ca923c364cfe24678bf8565b70edec15a826`
- **Post-change screenshot:** Not applicable to this procedure-only checkpoint
- **Visual comparison:** Not applicable; the proof confirms capture creation and validation,
  not a component change
- **Corrections:** Documented that live captures require session Wayland socket access
- **Upstream note:** None
- **User decision:** Approved
- **Decision date:** 2026-07-28

### CP-F-003 — Dynamic Controls discovery and `HnSurfaceFrame` consumers

- **State:** Approved
- **Started:** 2026-07-28
- **Baseline commit:** shell `a7f0c822c3121018620366f6354700f413d7428a`; controls
  `00b1b55d88a9d20f527559b8551ad664760c2a7b`
- **Files in scope:** Read-only inspection of root and shell CMake, installed Controls artifacts,
  `tests/test_qml_smoke.cpp`, the four application consumers, and their QML tests; this ledger
- **Public contract:** No runtime contract changed; dynamic QML module loading remains enabled
- **Baseline behavior and geometry:** `HudFrame` maps its four variants to semantic surface roles;
  the Wi-Fi password dialog uses Popup, tooltip uses Tooltip, and tray menu uses Menu
- **States exercised:** Canonical Core/Controls import, installed artifacts, import ownership,
  HudFrame variants, tooltip surface role, and tray-menu surface role
- **Focused tests added/updated:** None; existing canonical and consumer tests cover discovery
- **Commands and results:** Three anchored `QmlSmoke` CTest cases passed. The full QML harness
  reported 187 passed and 3 unrelated compile failures because `ExternalIcon` could not resolve
  `HnIcon`; within that run, all `HudFrame`, `TooltipPopup`, and `TrayMenuPopup` Controls tests
  passed. `tst_component_instantiation.qml` contains the Wi-Fi password frame assertion but could
  not compile because its unrelated `SidebarNotifications` component also reaches
  `ExternalIcon`. Source/CMake search found exactly four consumers and no
  `HolonightQt::Controls` link; `readelf -d build/holonight-shell` confirmed no Controls library
  dependency. The installed Controls `qmldir`, qmltypes, plugin, and `HnSurfaceFrame.qml` exist.
- **Baseline screenshot:** Not applicable; discovery is verified through loading and
  instantiation tests
- **Post-change screenshot:** Not applicable
- **Visual comparison:** Not applicable
- **Corrections:** None
- **Upstream note:** None; the unrelated `ExternalIcon` harness failure concerns Core icon
  resolution in the application component resource, not Controls discovery
- **User decision:** Approved
- **Decision date:** 2026-07-28

### CP-S-001 — Settings `NavPanel`

- **State:** Approved
- **Started:** 2026-07-28
- **Baseline commit:** shell `a7f0c822c3121018620366f6354700f413d7428a`; controls adoption
  baseline `00b1b55d88a9d20f527559b8551ad664760c2a7b`; upstream retry
  `dbbccd045063b8063512e946da8593f7f048dd8c`
- **Files in scope:** `apps/settings/qml/NavPanel.qml`, `tests/test_settings_app.cpp`,
  `tests/CMakeLists.txt`, and pipeline ledgers
- **Public contract:** required `currentPage`; `pageRequested(pageKey)`; ordered 13-entry page model
- **Baseline behavior and geometry:** 1200×800 logical Settings window on DP-5 at compositor scale
  1.50; 200-pixel navigation column; 16-pixel top inset; 13 rows at 40 pixels with 2-pixel
  spacing; Appearance selected with violet fill and a 3×24 left rail
- **States exercised:** Baseline and shared selected/idle rendering; pointer activation; keyboard
  Space activation; active focus; live current-page synchronization; disabled pointer suppression
- **Focused tests added/updated:** `SettingsNavPanelQmlTest.PreservesPageOrderAndRoutesActivation`
  validates the ordered 13-entry model and pointer routing to `pageRequested("bar")`;
  `test_holonight_settings` now links Qt Quick for its visual QML test window
- **Commands and results:** The upstream `holonight_package_install_test` passed and the fixing
  revision was installed to `/tmp/holonight-qt-prefix`. The focused Settings test passed after
  retry. `task qml-lint`, rebuilt Settings launch/instantiation without QML warnings,
  `scripts/check-qmltypes.sh build`, targeted clang-format, and `git diff --check` passed.
- **Baseline screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-001-baseline-current-s1.0-default.png`;
  1800×1200 RGB PNG for the 1200×800 logical crop; SHA-256
  `776fafdee7397dbe3c72b70bd204668be0c87fe5a87bee99096c9919f1229132`
- **Post-change screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-001-post-current-s1.0-default.png`;
  1800×1200 RGB PNG; SHA-256
  `4f09d1c30b2b8194fafc96ee5661087466c55f4647beb5e03342b66121f10424`
- **Visual comparison:** Window and content geometry are unchanged. All 13 navigation rows remain
  40 logical pixels high. The shared delegate moves labels to its semantic horizontal padding,
  uses the shared selected/hover surfaces, and renders the selection edge at full row height.
  ImageMagick reports 5981.68 changed pixels (normalized 0.00276929); the temporary diff is
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-001-diff.png`.
- **Corrections:** Initial attempt restored after UP-001; retried successfully after upstream fix
  `dbbccd045063b8063512e946da8593f7f048dd8c`
- **Upstream note:** UP-001, verified fixed
- **User decision:** Approved
- **Decision date:** 2026-07-28

### CP-S-002 — Appearance section headers

- **State:** Approved
- **Started:** 2026-07-28
- **Baseline commit:** shell `a7f0c822c3121018620366f6354700f413d7428a`; controls adoption
  baseline `00b1b55d88a9d20f527559b8551ad664760c2a7b`; upstream retry
  `dbbccd045063b8063512e946da8593f7f048dd8c`
- **Files in scope:** `apps/settings/qml/AppearancePage.qml`, focused Settings QML coverage,
  and pipeline ledgers
- **Public contract:** Eight ordered Appearance sections and their existing content; no model,
  service, or interaction contract
- **Baseline behavior and geometry:** 1200×800 logical Settings window on DP-5 at compositor
  scale 1.50; section labels use 12-pixel uppercase secondary text, 8-pixel label/content
  spacing, and a one-pixel divider after each section
- **States exercised:** Dark-theme default Appearance presentation; the control is
  presentation-only and has no pointer, keyboard, focus, selected, disabled, empty, loading, or
  error state
- **Focused tests added/updated:**
  `SettingsAppearanceQmlTest.ShapeControlsBindAndUpdateEditModel` now verifies all eight shared
  headers remain in their original order with dividers enabled while retaining the existing
  shape-control binding assertions
- **Commands and results:** The focused Settings test passed. `task qml-lint`,
  `task qmltypes-check`, rebuilt Settings launch/instantiation without QML warnings, targeted
  clang-format, and `git diff --check` passed.
- **Baseline screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-002-baseline-current-s1.0-default.png`;
  1800×1200 RGB PNG for the 1200×800 logical crop at `2728:352:1200:800`; SHA-256
  `03d9d0caafc01a4293bf6283735fd109b0906df650ef3724775028377dedad1b`
- **Post-change screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-002-post-current-s1.0-default.png`;
  1800×1200 RGB PNG for the identical crop; SHA-256
  `b851f93e7afd2cd0fe43f12fd7c4e1b2dc671044f5172c1fa7f6bd11a2f7f7a6`
- **Visual comparison:** Section content, ordering, and available width remain unchanged. The
  shared compact header replaces uppercase secondary labels with bold semantic title text and
  moves each divider directly below its heading. The first four sections remain visible in the
  same viewport, with their content shifted modestly by the shared header metrics. ImageMagick
  reports 38817.6 changed pixels (normalized 0.0179711); the temporary diff is
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-002-diff.png`.
- **Corrections:** None
- **Upstream note:** None
- **User decision:** Approved
- **Decision date:** 2026-07-28

### CP-S-003 — Appearance theme variants

- **State:** Approved
- **Started:** 2026-07-28
- **Baseline commit:** shell `a7f0c822c3121018620366f6354700f413d7428a`; controls adoption
  baseline `00b1b55d88a9d20f527559b8551ad664760c2a7b`; upstream retry
  `dbbccd045063b8063512e946da8593f7f048dd8c`
- **Files in scope:** Theme-variant delegates in `apps/settings/qml/AppearancePage.qml`, focused
  Settings QML coverage, and pipeline ledgers
- **Public contract:** Five ordered theme families, ten ordered variant IDs, selected-scheme
  synchronization through `editModel.themeScheme`, and pointer/keyboard activation
- **Baseline behavior and geometry:** 1200×800 logical Settings window on DP-5 at compositor
  scale 1.50; five two-column family rows with 36-logical-pixel variant cards, centered
  13-pixel labels, a solid violet selected fill, and no keyboard-focus treatment
- **States exercised:** Default selected state, pointer hover and activation, keyboard focus and
  Space activation, and disabled pointer rejection. Empty, loading, and error states do not apply
  to this fixed catalog.
- **Focused tests added/updated:**
  `SettingsAppearanceQmlTest.ThemeVariantsPreserveCatalogAndUpdateEditModel` verifies the ten
  ordered IDs and labels, selection synchronization, hover, pointer activation, keyboard
  activation, focus, and disabled behavior.
- **Commands and results:** The focused Settings test passed using the installed controls module.
  `task qml-lint`, `task qmltypes-check`, rebuilt live Settings instantiation without QML
  warnings, targeted clang-format, and `git diff --check` passed.
- **Baseline screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-003-baseline-current-s1.0-default.png`;
  1800×1200 RGB PNG for the 1200×800 logical crop at `2728:352:1200:800`; SHA-256
  `eb99336b09ae58ea99c96e2d17f9c61679fc20477f6eb0d5398fe11d0c7c3018`
- **Post-change screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-003-post-current-s1.0-default.png`;
  1800×1200 RGB PNG for the identical crop; SHA-256
  `a75a52be431d012c8907b5aab67f3fd2fa2c0c32569d099c7995caeb61885232`
- **Visual comparison:** Family order, two-column composition, card heights, and downstream
  section positions remain unchanged. Shared cards left-align bold labels, use semantic elevated
  surfaces and passive borders, and show the selected scheme with the shared selection border
  instead of a downstream-calculated violet fill. ImageMagick reports 34209.3 changed pixels
  (normalized 0.0158376); the temporary diff is
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-003-diff.png`.
- **Corrections:** None
- **Upstream note:** None
- **User decision:** Approved
- **Decision date:** 2026-07-28

### CP-S-004 — Appearance accent choices

- **State:** Approved
- **Started:** 2026-07-28
- **Baseline commit:** shell `a7f0c822c3121018620366f6354700f413d7428a`; controls adoption
  baseline `00b1b55d88a9d20f527559b8551ad664760c2a7b`; upstream retry
  `6ffdae297caa665cb9a26fb611837035174eeb31`
- **Files in scope:** Accent-choice delegates in `apps/settings/qml/AppearancePage.qml`, focused
  Settings QML coverage, and pipeline ledgers
- **Public contract:** Five ordered accent IDs, catalog names and default designation, selected
  accent synchronization through `editModel.themeAccent`, and pointer/keyboard activation
- **Baseline behavior and geometry:** 1200×800 logical Settings window on DP-5 at compositor
  scale 1.50; one row of five equal-width 36-logical-pixel choices with centered 13-pixel labels,
  accent-colored selected fill, and an accent-colored outline identifying the default option
- **States exercised:** Default selected state, catalog default designation, pointer hover and
  activation, keyboard focus and Space activation, and disabled pointer rejection. Empty,
  loading, and error states do not apply to this fixed catalog.
- **Focused tests added/updated:**
  `SettingsAppearanceQmlTest.AccentChoicesPreserveCatalogAndUpdateEditModel` verifies the five
  ordered IDs and labels, the default designation, selection synchronization, hover, pointer
  activation, keyboard activation, focus, and disabled behavior.
- **Commands and results:** The focused Settings test passed using the installed controls module.
  Upstream's two pressed-state QML regressions and package-install test passed before retry.
  The installed `HnChoiceCard.qml` is byte-identical to upstream
  `6ffdae297caa665cb9a26fb611837035174eeb31`. `task qml-lint`,
  `task qmltypes-check`, live Settings instantiation without QML warnings, targeted clang-format,
  and `git diff --check` passed.
- **Baseline screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-004-baseline-current-s1.0-default.png`;
  1800×1200 RGB PNG for the 1200×800 logical crop at `2728:352:1200:800`; SHA-256
  `a75a52be431d012c8907b5aab67f3fd2fa2c0c32569d099c7995caeb61885232`
- **Post-change screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-004-post-fixed-current-s1.0-default.png`;
  1800×1200 RGB PNG for the identical crop; SHA-256
  `37742dfb8ef0b3ef001993af7df0c59ec5b6918eddb0b5ee47645db11a26b954`
- **Visual comparison:** The five-option order, single-row composition, equal widths, 36-pixel
  height, and downstream section positions remain unchanged. Shared cards left-align bold
  labels, use semantic elevated surfaces and passive borders, and identify the selected default
  with the shared selection border instead of an accent-colored fill and separate
  default-option outline. ImageMagick reports 9649.66 changed pixels (normalized 0.00446744);
  the temporary diff is
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-004-fixed-diff.png`. The upstream rendered
  regression additionally verifies that the pressed card resolves to the same opaque surface
  over red and green ancestors.
- **Corrections:** Restored the accent delegate and its focused test to the last approved
  CP-S-003 state after identifying the shared-control defect. Retry started after installing
  upstream `6ffdae297caa665cb9a26fb611837035174eeb31`.
- **Upstream note:** UP-002; handoff document
  `../holonight-qt/docs/hn-choice-card-pressed-overlay-composition-bug.md`
- **User decision:** Approved
- **Decision date:** 2026-07-28

### CP-S-005 — Appearance global shape selector

- **State:** Approved
- **Started:** 2026-07-28
- **Baseline commit:** shell `a7f0c822c3121018620366f6354700f413d7428a`; controls adoption
  baseline `00b1b55d88a9d20f527559b8551ad664760c2a7b`; current controls
  `6ffdae297caa665cb9a26fb611837035174eeb31`
- **Files in scope:** Global Shape selector in `apps/settings/qml/AppearancePage.qml`, focused
  Settings QML coverage, and pipeline ledgers
- **Public contract:** Four ordered values (`inherit`, `hybrid`, `rounded`, `chamfered`),
  synchronization through `editModel.shapeCornerStyle`, and pointer/keyboard activation
- **Baseline behavior and geometry:** 1200×800 logical Settings window on DP-5 at compositor
  scale 1.50; one row of four equal-width 36-logical-pixel options with centered 13-pixel labels
  and a violet selected fill
- **States exercised:** Initial selected state, pointer hover and press/release activation,
  keyboard focus and Right navigation, external edit-model synchronization after activation,
  and disabled pointer rejection. Empty, loading, and error states do not apply to this fixed
  four-option model.
- **Focused tests added/updated:**
  `SettingsAppearanceQmlTest.GlobalShapeSelectorPreservesValuesAndUpdatesEditModel` verifies the
  four ordered values and labels, initial/current value synchronization, pointer interaction,
  keyboard navigation, focus, external model updates, and disabled behavior.
- **Commands and results:** The focused Settings test passed. `task qml-lint`,
  `task qmltypes-check`, live Settings instantiation without QML warnings, targeted clang-format,
  and `git diff --check` passed.
- **Baseline screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-004-post-fixed-current-s1.0-default.png`;
  1800×1200 RGB PNG; SHA-256
  `37742dfb8ef0b3ef001993af7df0c59ec5b6918eddb0b5ee47645db11a26b954`
- **Post-change screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-005-post-current-s1.0-default.png`;
  1800×1200 RGB PNG for the identical crop; SHA-256
  `b12222c2791cd533f6b7d2b2c1c280203bdf6ac6bd5c015395ec07a4ed472442`
- **Visual comparison:** The four values, order, equal-width single-row composition, selector
  height, and downstream row positions remain unchanged. The shared segmented control joins the
  options into one bordered surface, uses semantic segment fills, and marks selection with the
  shared bottom indicator instead of a downstream violet fill. ImageMagick reports 12454.3
  changed pixels (normalized 0.00576587); the temporary diff is
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-005-diff.png`.
- **Corrections:** None
- **Upstream note:** None
- **User decision:** Approved
- **Decision date:** 2026-07-28

### CP-S-006 — Appearance shape scale and override rows

- **State:** Approved
- **Started:** 2026-07-28
- **Baseline commit:** shell `a7f0c822c3121018620366f6354700f413d7428a`; controls adoption
  baseline `00b1b55d88a9d20f527559b8551ad664760c2a7b`; current controls
  `351b91d4e437a772e440ed7ae60e222eac5b75a3`
- **Files in scope:** Shape scale row and the base-radius/base-chamfer override rows in
  `apps/settings/qml/AppearancePage.qml`, focused Settings QML coverage, and pipeline ledgers
- **Public contract:** Shape scale range/step/value writes; override switch state; radius and
  chamfer ranges, steps, enabled state, value writes, and displayed rounding
- **Baseline behavior and geometry:** 1200×800 logical Settings window on DP-5 at compositor
  scale 1.50. The Global Shape section presents a label above a full-width scale slider/value
  row. Advanced Shape presents explanatory text followed by two inline switch/slider/value rows.
- **States exercised:** Initial scale values; disabled radius/chamfer sliders; switch activation and
  resulting enabled sliders; slider value writes; row focus routing; and a live pointer toggle
  restored to its initial state. Selection, empty, loading, and error states do not apply.
- **Focused tests added/updated:**
  `SettingsAppearanceQmlTest.ShapeSettingsRowsPreserveControlsAndUpdateEditModel` verifies the
  three shared rows and their titles, stacked geometry selection, compound-control focus routing,
  exact slider ranges/steps, initial disabled state, switch-driven enablement, and edit-model
  writes for all three numeric values.
- **Commands and results:** The two focused upstream geometry/focus regressions passed, and the
  installed `HnSettingsRow.qml` matches upstream commit
  `351b91d4e437a772e440ed7ae60e222eac5b75a3` byte-for-byte. The focused Settings test,
  `task qml-lint`, `task qmltypes-check`, and live Settings instantiation without QML warnings
  passed. Targeted clang-format and final `git diff --check` are recorded before the review handoff.
- **Baseline screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-005-post-current-s1.0-default.png`;
  1800×1200 RGB PNG; SHA-256
  `b12222c2791cd533f6b7d2b2c1c280203bdf6ac6bd5c015395ec07a4ed472442`
- **Post-change screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-006-post-fixed-current-s1.0-default.png`;
  1800×1200 RGB PNG for the identical crop; SHA-256
  `66a3b8fdaf4455dcd07b84a9db11459197bc56a0cb02060094702a271d85061f`.
  Supplemental scrolled capture showing both override rows:
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-006-post-fixed-scrolled-s1.0-default.png`;
  SHA-256 `f3d41a2a553ddb3540baa64e879b54302a46f38a2f0bdb1f009d613f13ef2122`.
- **Visual comparison:** Values, full-width sliders, section ordering, and application-owned
  standard controls remain intact. All three shared rows place their controls at the common
  content origin. The radius and chamfer switch/slider/value compositions have identical x
  geometry, and each switch shares the slider's vertical center. ImageMagick reports 13272.4
  changed pixels (normalized 0.00614462) against CP-S-005; the temporary diff is
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-006-fixed-diff.png`.
- **Corrections:** User review found vertical misalignment between switches and sliders and
  inconsistent horizontal control origins across the two override rows. All three components and
  their focused test were first restored to CP-S-005. After upstream commit
  `351b91d4e437a772e440ed7ae60e222eac5b75a3` fixed the shared geometry, the same checkpoint was
  retried without a downstream visual workaround.
- **Upstream note:** UP-003
- **User decision:** Approved
- **Decision date:** 2026-07-28

### CP-S-007 — Appearance font selectors

- **State:** Approved
- **Started:** 2026-07-28
- **Baseline commit:** shell `a7f0c822c3121018620366f6354700f413d7428a`; controls adoption
  baseline `00b1b55d88a9d20f527559b8551ad664760c2a7b`; current controls
  `351b91d4e437a772e440ed7ae60e222eac5b75a3`
- **Files in scope:** Interface Font and Monospace Font selectors in
  `apps/settings/qml/AppearancePage.qml`, focused Settings QML coverage, and pipeline ledgers
- **Public contract:** Full and fixed-pitch font models, displayed font names, initial/current
  selection synchronization through `editModel.uiFont` and `editModel.fixedFont`, and activated
  value writes
- **Baseline behavior and geometry:** 1200×800 logical Settings window on DP-5 at compositor
  scale 1.50; two full-width standard combo boxes below their section headers
- **States exercised:** Initial configured and unavailable-font selections, valid catalog
  activation and model writes, keyboard focus, popup open/close, and disabled pointer rejection.
  Empty, loading, and error states do not apply to the system font catalogs.
- **Focused tests added/updated:**
  `SettingsAppearanceQmlTest.FontSelectorsPreserveModelsAndUpdateEditModel` verifies the full and
  fixed-pitch catalogs, display roles, configured selection when available, unavailable-font
  behavior, activated writes, keyboard popup open/close, focus, and disabled pointer rejection
- **Commands and results:** The focused Settings test passed. `task qml-lint`,
  `task qmltypes-check`, targeted clang-format, live Settings launch without QML warnings, and
  `git diff --check` passed.
- **Baseline screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-007-baseline-current-s1.0-font-selectors.png`;
  1800×1200 RGB PNG for the 1200×800 logical crop at `2728:352:1200:800`; SHA-256
  `7198d65645c0446a317d880e6bd031b5c23b1d050a7506d4a4898169238ccf86`
- **Post-change screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-007-post-current-s1.0-font-selectors.png`;
  1800×1200 RGB PNG for the identical crop; SHA-256
  `79a97aef129983f71eed13e00d4e873fb69316f2f57b3a8aee456177ec8bc71e`
- **Visual comparison:** Both selectors retain their full-width geometry, selected font text,
  section ordering, and downstream font-size row positions. The shared controls use their
  semantic combo-box sizing and slot-ready text/icon composition without adding downstream
  styling or icons. ImageMagick reports 4724.19 changed pixels (normalized 0.00218713); the
  temporary diff is `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-007-diff.png`.
- **Corrections:** None
- **Upstream note:** None
- **User decision:** Approved
- **Decision date:** 2026-07-29

### CP-S-008 — Appearance font-size rows

- **State:** Approved
- **Started:** 2026-07-29
- **Baseline commit:** shell `a7f0c822c3121018620366f6354700f413d7428a`; controls adoption
  baseline `00b1b55d88a9d20f527559b8551ad664760c2a7b`; current controls
  `4003d45f65a7b7103d5b98cbbebda13a776aade4`
- **Files in scope:** Interface Font Size and Monospace Font Size compositions in
  `apps/settings/qml/AppearancePage.qml`, focused Settings QML coverage, and pipeline ledgers
- **Public contract:** Integer 8–18 point ranges, one-point steps, initial and external
  `editModel` synchronization, rounded slider writes, displayed point values, and keyboard focus
- **Baseline behavior and geometry:** 1200×800 logical Settings window on DP-5 at compositor
  scale 1.50; each font-size section contains a full-width standard slider and right-aligned point
  value
- **States exercised:** Initial 12/14 point values, external model updates, fractional slider
  movement rounded to integer writes, keyboard focus, and the full 8–18 ranges with one-point
  steps
- **Focused tests added/updated:**
  `SettingsAppearanceQmlTest.FontSizeRowsPreserveRangesAndUpdateEditModel` verifies both shared
  row compositions, loaded controls, titles, stacked layout, slider ranges and steps, initial and
  external synchronization, displayed point values, rounded writes, and direct slider focus
- **Commands and results:** The focused Settings test passed. `task qml-lint`,
  `task qmltypes-check`, targeted clang-format, live Settings launch without QML warnings, and
  `git diff --check` passed.
- **Baseline screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-007-post-current-s1.0-font-selectors.png`;
  1800×1200 RGB PNG for the 1200×800 logical crop at `2728:352:1200:800`; SHA-256
  `79a97aef129983f71eed13e00d4e873fb69316f2f57b3a8aee456177ec8bc71e`
- **Post-change screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-008-post-current-s1.0-font-size-rows.png`;
  1800×1200 RGB PNG for the identical crop; SHA-256
  `dda613e69f042ea941e835171fb7f645399917e647e4f0e80aeda46f6c357bcc`.
- **Visual comparison:** Both rows retain full-width sliders, 12/14 point values, section order,
  and common right-aligned value geometry. The shared rows add their semantic `Size` labels and
  vertical padding, increasing the page's scroll extent without clipping or overlap. The capture
  is scrolled farther than the baseline to keep both complete rows visible; ImageMagick reports
  53268.8 changed pixels (normalized 0.0246615), including that scroll-position difference. The
  temporary diff is `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-008-diff.png`.
- **Corrections:** None
- **Upstream note:** None
- **User decision:** Approved
- **Decision date:** 2026-07-29

### CP-S-009 — Bar workspace/tray rows

- **State:** Approved
- **Started:** 2026-07-29
- **Baseline commit:** shell `a7f0c822c3121018620366f6354700f413d7428a`; controls adoption
  baseline `00b1b55d88a9d20f527559b8551ad664760c2a7b`; current controls
  `4003d45f65a7b7103d5b98cbbebda13a776aade4`
- **Files in scope:** Workspace Count and System Tray Max Items compositions in
  `apps/settings/qml/BarPage.qml`, focused Settings QML coverage, and pipeline ledgers
- **Public contract:** Integer workspace 3–10 and tray 2–5 ranges, one-item steps, initial and
  external `editModel` synchronization, rounded slider writes, displayed values, and keyboard
  focus
- **Baseline behavior and geometry:** 1200×800 logical Settings window on DP-5 at compositor
  scale 1.50; each setting used an uppercase 12-pixel label above a full-width standard slider
  and a 24-pixel-wide value
- **States exercised:** Initial configured values, external model updates, fractional slider
  movement rounded to integer writes, keyboard focus, workspace 3–10 and tray 2–5 ranges, and
  one-item steps
- **Focused tests added/updated:**
  `SettingsBarQmlTest.WorkspaceAndTrayRowsPreserveRangesAndUpdateEditModel` verifies both shared
  row compositions, loaded controls, titles, stacked layout, slider ranges and steps, initial and
  external synchronization, displayed values, rounded writes, and direct slider focus
- **Commands and results:** The focused Settings test passed. `task qml-lint`,
  `task qmltypes-check`, targeted clang-format, live Settings launch without QML warnings, and
  `git diff --check` passed.
- **Baseline screenshot:** Not captured before implementation; baseline geometry and styling were
  recorded from the original `BarPage.qml`. This evidence gap prevents a pixel-diff metric but
  does not affect behavioral verification.
- **Post-change screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-009-post-current-s1.0-bar-rows.png`;
  1800×1200 RGB PNG for the 1200×800 logical crop at `2728:352:1200:800`; SHA-256
  `53b1350e15dbd358095223f2fb21061811d73ee847f158ede851127df30eebf3`
- **Visual comparison:** Both rows retain their labels, full-width sliders, configured values,
  order, and common right-aligned value geometry. The shared rows add semantic vertical padding
  and title typography without clipping, overlap, or unnecessary scroll extent. No pixel-diff
  metric is available because the pre-change Bar capture was missed.
- **Corrections:** None
- **Upstream note:** None
- **User decision:** Approved
- **Decision date:** 2026-07-29

### CP-S-010 — Footer shell state

- **State:** Approved
- **Started:** 2026-07-29
- **Baseline commit:** shell `a7f0c822c3121018620366f6354700f413d7428a`; controls adoption
  baseline `00b1b55d88a9d20f527559b8551ad664760c2a7b`; current controls
  `4003d45f65a7b7103d5b98cbbebda13a776aade4`
- **Files in scope:** Shell-status composition in `apps/settings/qml/FooterBar.qml`, focused
  Settings QML coverage, and pipeline ledgers
- **Public contract:** `shellStatus.shellRunning` selects running/not-running semantics and
  `shellStatus.statusText` remains the displayed text; footer version and actions remain unchanged
- **Baseline behavior and geometry:** 1200×800 logical Settings window on DP-5 at compositor
  scale 1.50; the footer is 56 pixels high and the shell state is a 36-pixel-high rounded pill
  with a 10-pixel success/warning dot, 8-pixel dot/text spacing, and 12-pixel status text
- **States exercised:** Running shell maps to shared Success status and not-running shell maps to
  shared Warning status; both retain the service-provided text and the shared dot presentation
- **Focused tests added/updated:**
  `SettingsFooterQmlTest.ShellStatusUsesSharedSemanticIndicator` instantiates the footer with its
  real edit, file, and shell-status services and verifies the shared indicator, displayed text,
  dot presentation, and state-dependent semantic status
- **Commands and results:** The focused Settings footer test passed. `task qml-lint`,
  `task qmltypes-check`, rebuilt Settings live launch without QML warnings, targeted
  clang-format, and `git diff --check` passed.
- **Baseline screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-010-baseline-current-s1.0-footer.png`;
  1800×1200 RGB PNG for the 1200×800 logical crop at `2728:352:1200:800`; SHA-256
  `d7bf871310edad4aa18b8b823059f3772d4b5e54713eeda818e29e11cae89f8d`
- **Post-change screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-010-post-current-s1.0-footer.png`;
  1800×1200 RGB PNG; SHA-256
  `50a6b84e2fb94f867d72bcc14442e4a5db2ac4ef0032940edc734309004fdb61`
- **Visual comparison:** Footer height, left alignment, status text, version, and action geometry
  remain unchanged. The shared indicator reduces the dot from 10 to 7 pixels, uses 4-pixel
  dot/text spacing, and applies the semantic success color to both dot and running-state text.
  ImageMagick reports 1469.22 changed pixels (normalized 0.000680194); the temporary diff is
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-010-diff.png`.
- **Corrections:** None
- **Upstream note:** None
- **User decision:** Approved
- **Decision date:** 2026-07-29

### CP-S-011 — Footer composition

- **State:** Approved
- **Started:** 2026-07-29
- **Baseline commit:** shell `a7f0c822c3121018620366f6354700f413d7428a`; controls adoption
  baseline `00b1b55d88a9d20f527559b8551ad664760c2a7b`; current controls
  `703b9237351416d96664859d8e388085422e1920` (UP-004 fixed by
  `802c6cb5161b1a25a5842684b9653546e46af1bc`)
- **Files in scope:** Footer layout composition in `apps/settings/qml/FooterBar.qml`, focused
  Settings QML coverage, and pipeline ledgers
- **Public contract:** Preserve the 56-pixel footer, shell-status/version leading content,
  Discard Changes/Apply/Save & Apply actions, dirty/saving enablement, load/save service routing,
  and save-error dialog with Retry/Cancel actions
- **Baseline behavior and geometry:** 1200×800 logical Settings window on DP-5 at compositor
  scale 1.50; status/version content is left aligned with 16-pixel outer margin and the three
  36-pixel-high actions are right aligned with 8-pixel spacing and a 16-pixel outer margin
- **States exercised:** Initial clean/disabled actions, dirty/enabled actions, Discard Changes
  routing, live leading/trailing geometry, and warning-free instantiation
- **Focused tests added/updated:**
  `SettingsFooterQmlTest.ActionBarPreservesActionsAndTrailingAlignment` verifies the shared
  action bar, clean/dirty action enablement, Discard Changes routing, and the trailing action's
  16-pixel right inset. The existing semantic shell-status test remains passing.
- **Commands and results:** The two-test `SettingsFooterQmlTest.*` suite passed against the
  installed fixed control. The Settings executable rebuilt, `task qml-lint`,
  `task qmltypes-check`, targeted clang-format, and `git diff --check` passed. Live instantiation
  emitted no QML warnings.
- **Baseline screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-010-post-current-s1.0-footer.png`, reused as
  the unchanged CP-S-011 starting state; 1800×1200 RGB PNG; SHA-256
  `50a6b84e2fb94f867d72bcc14442e4a5db2ac4ef0032940edc734309004fdb61`
- **Post-change screenshot:** Initial rejected attempt:
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-011-post-current-s1.0-footer.png`;
  1800×1200 RGB PNG; SHA-256
  `a54178e7a3e476d8c7a3f1b6cd6a27118db661649fe675daff086696e78f814b`
- **Post-change retry screenshot:**
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-011-post-current-s1.0-footer-retry.png`;
  1800×1200 RGB PNG; SHA-256
  `78955e5e4e4bb0af5f0c93ba37adab2501ef0f3311c2fc06b0c16a82358d32bb`
- **Visual comparison:** The successful retry preserves the 56-pixel footer, status/version
  leading geometry, three 36-pixel-high actions, 8-pixel action spacing, and 16-pixel outer
  insets. The shared action bar now keeps the trailing group at the right edge with no center
  content, matching the alignment direction in the supplied Settings mockup. ImageMagick reports
  RMSE 915.089 (normalized 0.0139634); the temporary retry diff is
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-011-retry-diff.png`.
- **Corrections:** Restored after the initial UP-004 discovery; retried successfully after
  upstream fix `802c6cb5161b1a25a5842684b9653546e46af1bc`
- **Upstream note:** UP-004
- **User decision:** Approved
- **Decision date:** 2026-07-29

### MS-S-001 — Settings milestone

- **State:** Approved
- **Started:** 2026-07-29
- **Prerequisite checkpoint decisions:** CP-S-001 through CP-S-011 approved
- **Dark, scale 1.0:** Passed; 1200×800 logical window, 1800×1200 RGB capture
- **Dark, scale 1.25:** Passed; 1500×1000 logical window, 2250×1500 RGB capture
- **Light, scale 1.0:** Passed; 1200×800 logical window, 1800×1200 RGB capture
- **Light, scale 1.25:** Passed; 1500×1000 logical window, 2250×1500 RGB capture
- **`task qmltypes-check`:** Passed; QML type metadata and module packaging checks passed
- **`task test`:** Passed; 1079/1079 tests passed in 74.32 seconds; the environment-dependent
  `SidebarManagerMonitorValidation.AcceptsAnyCurrentlyConnectedNonEmptyNamedScreen` case was
  reported skipped
- **Live workflow:** Launched the real Settings executable for every isolated theme/scale
  combination on DP-5, confirmed the selected theme, full Appearance content, running shell
  status, footer actions, and clean initial action enablement; no QML warnings were emitted.
  Existing milestone test coverage exercised page routing, edit-model writes, dirty/clean
  transitions, reload routing, and footer action behavior.
- **Screenshots:** Temporary captures under `/tmp/holonight-settings-ms-s-001/screenshots/`:
  `ms-s-001-dark-s1.0.png` SHA-256
  `ab29decf2e3c47e68ddab6bded126de742146c1dec22cd51d33094917401d5a8`;
  `ms-s-001-dark-s1.25.png` SHA-256
  `b90bc7ce55b46ff72bd0e303af163eeea9366c1eb467c06d8f65c8716dd849b9`;
  `ms-s-001-light-s1.0.png` SHA-256
  `65df460a834c6e487099f5554d853c4ac5b0091a85a04da4fce5a1205b209255`;
  `ms-s-001-light-s1.25.png` SHA-256
  `ced38a283ab6db779823a6b5094d8da3e924c7b840036ed9b37e9d4d83b0ee04`
- **Diagnostics:** The matrix used isolated `XDG_CONFIG_HOME` and `HOLONIGHT_CONFIG_FILE` roots
  and did not alter the user's configuration. `task qml-lint` and `git diff --check` passed.
  Visual inspection found no clipping, overlap, misaligned selection, footer regression, or
  fractional-scale artifact.
- **User decision and date:** Approved, 2026-07-29

### CP-L-001 through CP-L-007 — Launcher shared controls

- **State:** Approved after the user's bundled manual review
- **Started:** 2026-07-29
- **Baseline commit:** shell `ee5cebf`; Controls baseline remains the installed dependency
  recorded by the foundation checkpoint
- **Review protocol exception:** The user explicitly authorized completing all seven Launcher
  checkpoints before one approval stop and requested no screenshots or agent-performed visual
  checks. This supersedes the normal per-checkpoint visual stops for CP-L-001 through CP-L-007.
- **CP-L-001:** `LauncherSearchField` now uses `HnSearchField` while retaining two-way query
  synchronization, `forceInputFocus()`, `clearInput()`, Up/Down and Tab/Backtab movement,
  Enter launch, first-Escape clear, and empty-Escape close behavior.
- **CP-L-002:** `LauncherResultRow` now uses `HnListDelegate`; application roles, best-match
  sizing, selection, hover routing, activation, icon fallback, divider behavior, and the
  selected-row `HnKeyHint` are retained.
- **CP-L-003:** `LauncherActionRow` now uses `HnListDelegate`, matching its direct launch
  semantics without a navigation chevron; title, parent application, selected state,
  `actionExec`, and `activated()` remain available.
- **CP-L-004:** Recent applications now use `HnListDelegate`, retaining entry lookup, icon/name
  fallback, recent refresh handling, and desktop-file launch routing.
- **CP-L-005:** Categories now use `HnNavigationDelegate`, retaining category names, counts,
  active-category synchronization, icons, and `setActiveCategory()` routing.
- **CP-L-006:** Search filters now use `HnSegmentedControl` with explicit `all`,
  `applications`, and `actions` values, adapted to the existing `""`, `apps`, and `actions`
  filtering contract.
- **CP-L-007:** Recent, browse-result, and search-result empty presentations now use
  `HnEmptyState` with their original visibility conditions and messages.
- **Focused tests:** `tst_LauncherSharedControls.qml` covers search synchronization and keyboard
  behavior, result-row roles/key hint, recent/category delegates and empty conditions, and the
  segmented-filter adapter. `tst_LauncherActionRow.qml` covers action mapping, shared selection,
  and activation. The existing focused Launcher search-field instantiation test remains passing.
- **Commands and results:** `task qml-lint` passed with no diagnostics. The focused
  `LauncherSharedControlsQmlTests` passed 7/7, `LauncherActionRowQmlTests` passed 5/5, and the
  focused `ComponentInstantiationTests::test_launcher_search_field_tracks_fake_service_query`
  passed. `clang-format --dry-run --Werror tests/FakeQmlServices.h` and `git diff --check`
  passed. Expected offscreen image-provider warnings were emitted for application icons.
- **Screenshots and automated visual comparison:** Not performed, by user instruction.
- **Corrections:** Manual review found insufficient bottom padding/vertical centering in best-match,
  normal result, action, and recent rows, including action subtitle overlap with the divider.
  Result rows now reserve 76 pixels for best-match and 60 pixels for normal two-line content;
  action rows reserve 60 pixels; recent rows reserve 48 pixels. Focused geometry expectations were
  updated and passed the user's manual recheck.
- **User decision:** Approved after the row-centering corrections
- **Decision date:** 2026-07-29

### MS-L-001 — Launcher milestone

- **State:** Approved
- **Prerequisite checkpoint decisions:** CP-L-001 through CP-L-007 approved
- **Automated verification:** Focused Launcher tests and `task qml-lint` passed; the full
  repository test suite and screenshot matrix were intentionally not run
- **Manual visual checklist:**
  - [x] Open the Launcher in the dark theme and confirm its frame, spacing, and columns are intact.
  - [x] Repeat in the light theme and check text, borders, selection, and muted states for contrast.
  - [x] Check scale 1.0 and 1.25 for clipping, overlap, fuzzy edges, or truncated labels.
  - [x] Confirm the search field height/alignment, prompt icon, focus border, typed text, and clear
    button presentation.
  - [x] Type a query and confirm best-match and normal application rows have sensible height,
    icon alignment, title/subtitle layout, selection treatment, dividers, and Enter hint.
  - [x] Confirm action rows are visually distinct but consistent with application rows and do not
    show a misleading navigation chevron.
  - [x] In browse mode, confirm recent rows center their icon/name vertically.
  - [x] Confirm category rows show icons, counts, hover state, and a single active-category
    indicator.
  - [x] In search mode, switch among All, Applications, and Actions and confirm the segmented
    control fits the right panel and visibly tracks the selected filter.
  - [x] Exercise no-recent-apps, no-applications-in-category, and no-search-matches states; confirm
    each empty state is centered, readable, and does not overlap surrounding content.
  - [x] Verify keyboard behavior: Up/Down and Tab/Shift+Tab move selection, Enter launches,
    Escape clears a non-empty query, and Escape closes when the query is empty.
  - [x] Verify mouse behavior: hover changes selection where expected, row clicks launch, category
    clicks filter, recent-app clicks launch, and the clear action returns focus to search.
  - [x] Confirm opening, closing, reopening, and switching between browse/search modes produces no
    layout jump, stale selection, or stale filter state.
- **User decision and date:** Approved, 2026-07-29

### CP-N-001 through CP-N-003 — Network popup shared controls

- **State:** Approved after the user's bundled manual review
- **Started:** 2026-07-29
- **Baseline commit:** shell `b630ad2`; Controls baseline remains the installed dependency
  recorded by the foundation checkpoint
- **Review protocol exception:** The user explicitly authorized completing all Network popup
  changes before one approval stop and delegated visual verification to themselves. No live
  compositor checks or screenshots were performed.
- **CP-N-001:** `NetworkActionRow` now uses two `HnActionDelegate` controls with tintable settings
  and information icons. The settings/info signals and disabled information state remain intact.
- **CP-N-002:** `WifiNetworkDelegate` now uses `HnListDelegate`, preserving every model role, the
  `row` alias, connected selection, signal-strength tiers, security state, dividers, disconnect,
  known/open connect, and unknown-secured password routing.
- **CP-N-003:** `WifiPasswordDialog` now wraps its password input in `HnFormField`, retaining the
  semantic popup frame, SSID/row properties, required validation, open focus, Enter/button submit,
  cancel, password clearing after submit, and `accepted(row, password)`.
- **Focused tests:** `tst_NetworkPopupPolish.qml`, `tst_WifiNetworkDelegate.qml`,
  `tst_NetworkPopupIcon.qml`, and the new `tst_WifiPasswordDialog.qml` cover shared-control
  mapping, signals, selection and divider state, strength/security content, password routing,
  focus, validation, submit, and cancel behavior.
- **Commands and results:** `task qml-lint` passed with no diagnostics. `task qmltypes-check`
  passed both metadata and module-packaging checks. The full CTest run passed 1,078 non-harness
  entries; its QML harness exposed one stale separator assertion, which was corrected. The
  rerun then passed all 228 QML tests. `git diff --check` passed.
- **Screenshots and automated visual comparison:** Not performed, by user instruction.
- **User decision and date:** Approved, 2026-07-29

### MS-N-001 — Network popup milestone

- **State:** Approved
- **Prerequisite checkpoint decisions:** CP-N-001 through CP-N-003 approved
- **Automated verification:** QML lint, qmltypes metadata/packaging, 1,078 non-harness CTest
  entries, and all 228 QML harness tests passed
- **Manual visual checklist:**
  - [x] Open the Network popup in dark and light themes; confirm the popup frame, section spacing,
    headings, separators, and text contrast remain coherent.
  - [x] Check scale 1.0 and 1.25 for clipped SSIDs, overlapping status/strength/security content,
    fuzzy icons, truncated action descriptions, or cropped focus borders.
  - [x] Confirm connected, known secured, unknown secured, and open networks each show the correct
    subtitle, selection treatment, signal percentage/tier, lock visibility, and divider.
  - [x] Hover and click a connected network to disconnect; click a known or open network to
    connect; confirm the row feedback is consistent and each action routes once.
  - [x] Click an unknown secured network; confirm the password dialog opens centered with the SSID,
    focuses the password field, masks input, and keeps Connect disabled while empty.
  - [x] Submit a password with Enter and with the Connect button; confirm both connect, close the
    dialog, and do not retain the submitted password on reopen.
  - [x] Cancel with the button, Escape, and outside click; confirm no connection attempt occurs and
    popup/dialog focus returns cleanly.
  - [x] Confirm Network Settings opens the expected settings surface and Connection Information is
    enabled only with connection data, toggles details once per click, and is visibly disabled
    otherwise.
  - [x] Exercise NetworkManager unavailable, Wi-Fi hardware disabled, Wi-Fi off, scanning, empty
    network list, connected, and error-message states; confirm messages and action availability.
  - [x] Reopen the popup after connect/disconnect/password/cancel flows and confirm there is no
    stale hover, selection, password, error, or information state.
- **User decision and date:** Approved, 2026-07-29

### CP-A-001 — Audio tab sidebar

- **State:** Approved
- **Started:** 2026-07-29
- **Baseline commit:** shell `1e9ff7d0e7d3e9caa68616fa1852bf86c5c3cf09`; Controls
  `d3887af633b6a5f953fa74fc4f1ed56d54fcdf76`
- **Files in scope:** `apps/shell/qml/Popups/Audio/AudioTabSidebar.qml`,
  `apps/shell/qml/Popups/Status/StatusPopup.qml`,
  `tests/qml/tst_AudioSharedControls.qml`, and pipeline ledgers
- **Public contract:** `currentTab`; three ordered translated tab labels; `tabSelected(index)`
- **Baseline behavior and geometry:** 180-logical-pixel sidebar; 8-pixel top inset; AUDIO heading;
  three 40-logical-pixel rows with 4-pixel spacing; the active row used an application-owned
  raised fill and left accent rail.
- **Implementation:** The three rows now use `HnNavigationDelegate`; `checked` follows
  `currentTab`, and `clicked()` routes the original index through `tabSelected(index)`. The
  sidebar width, row heights, label order, heading, and spacing remain unchanged.
- **Focused tests:** `AudioSharedControlsQmlTests` verifies the ordered labels, delegate titles,
  initial and updated checked state, signal routing, keyboard focus, and Space activation. The
  existing `ComponentInstantiationTests::test_audio_tab_sidebar_exposes_expected_tabs` remains
  passing.
- **Commands and results:** The full `test_holonight_qml_harness` passed after the focused test
  was added. `task qml-lint`, `task qmltypes-check`, the resulting shell build, and
  `git diff --check` passed.
- **Screenshots and automated visual comparison:** Not performed, by user instruction.
- **Manual visual checklist:**
  - [x] Open the Audio popup and confirm the sidebar remains 180 logical pixels wide, with the
    AUDIO heading and all three rows fully visible.
  - [x] Confirm Output Devices is selected when the popup first opens and exactly one row has the
    shared selected treatment.
  - [x] Click Input Devices and Applications; confirm the selected treatment moves once and the
    main panel changes to the matching section.
  - [x] Hover each row and confirm hover feedback is visible without obscuring its label or the
    selected indicator.
  - [x] Use Tab/Shift+Tab to focus the rows and Space to activate each one; confirm focus remains
    visible and each activation switches the main panel once.
  - [x] Check dark and light themes for readable idle, hover, selected, and focused text/borders.
  - [x] Check scale 1.0 and 1.25 for clipped labels, cropped focus borders, fuzzy selection edges,
    or changed row spacing.
  - [x] Close and reopen the popup; confirm the default/current selection is synchronized and no
    stale hover or focus state remains.
- **Corrections:** Manual review found that Tab/Shift+Tab could not focus the shared delegates, so
  Space could not activate them through the real popup focus path. The initial automated test
  forced focus directly and therefore missed the entry failure. Each application delegate now
  opts into Tab focus. The rich-content `Loader` in `StatusPopup` is a focus scope and was not
  focus-enabled, preventing its descendants from receiving active focus; it now enables focus
  while active. The regression fixture reproduces that focus-scope boundary before exercising
  Tab, Shift+Tab, and Space.
- **Upstream note:** None
- **User decision:** Approved after manual re-verification of Tab, Shift+Tab, Space, and Escape
- **Decision date:** 2026-07-29

### CP-A-002 through CP-A-005 — Audio rows and empty states

- **State:** Approved
- **Started:** 2026-07-29
- **Baseline commit:** shell `1e9ff7d0e7d3e9caa68616fa1852bf86c5c3cf09`; Controls
  `d3887af633b6a5f953fa74fc4f1ed56d54fcdf76`
- **Files in scope:** `apps/shell/qml/Popups/Audio/AudioDeviceDelegate.qml`,
  `AudioStreamDelegate.qml`, `AudioDeviceList.qml`, `AudioStreamList.qml`,
  `tests/qml/tst_AudioSharedControls.qml`, and pipeline ledgers
- **Review protocol exception:** The user explicitly authorized implementing CP-A-002 through
  CP-A-005 before one approval stop and delegated live visual/behavioral verification to
  themselves. No screenshots were taken.
- **CP-A-002:** Device rows now use `HnListDelegate`, mapping description/name to title/subtitle
  and default-device state to shared checked rendering. Device icons, DEFAULT badge, mute action,
  volume slider, percentage, output/input service routing, and row-click default routing remain
  application-owned. Device subtitles request the upstream single-line, right-elided
  presentation so the conditional badge cannot change the row's line count.
- **CP-A-003:** Stream rows now use `HnListDelegate`, mapping application/media names to
  title/subtitle. Application icon fallback, mute action, volume slider, percentage, and stream-ID
  service routing remain application-owned.
- **CP-A-004:** The zero-device presentation now uses a viewport-centered `HnEmptyState` with the
  existing caller-provided `emptyText`.
- **CP-A-005:** The zero-stream presentation now uses a viewport-centered `HnEmptyState` with the
  existing caller-provided `emptyText`.
- **Focused tests:** `AudioSharedControlsQmlTests` verifies device/stream role mapping, checked
  state, large shared-control sizing, slot creation, volume values/text, retained mute controls,
  vertical-center leading-content policy, and both shared empty-state messages. It also retains
  the approved audio-tab and popup focus coverage.
- **Commands and results:** The full QML harness passed. `task qml-lint`,
  `task qmltypes-check`, the shell build, all 1,079 CTest entries through `task test`, and
  `git diff --check` passed.
- **Manual Audio approval checklist:**
  - [x] Open Output Devices with at least two sinks; confirm each row keeps a readable icon,
    name/description hierarchy, mute control, slider, percentage, spacing, and 64-pixel rhythm.
  - [x] Confirm exactly the default output has the shared selected treatment and DEFAULT badge;
    click another row away from its controls and confirm the default moves once.
  - [x] Toggle output mute and drag/commit output volume; confirm the intended device changes,
    the icon/percentage update, and the row click does not also change the default.
  - [x] Open Input Devices and repeat default, mute, and volume checks; confirm violet accents and
    input service routing remain correct.
  - [x] Open Applications with active streams; confirm application icon fallback, application
    title, media subtitle, mute, slider, percentage, alignment, and long-text elision.
  - [x] Toggle a stream mute and drag/commit its volume; confirm only that stream changes and the
    row does not trigger an unrelated action.
  - [x] With enough rows to scroll, confirm scrolling remains bounded, rows do not overlap, and
    the existing scroll indicator tracks the viewport.
  - [x] Exercise no outputs, no inputs, and no active applications; confirm each shared empty
    state is centered, readable, uses the correct message, and disappears when an item appears.
  - [x] Exercise audio-service unavailable and recovery; confirm the unavailable message remains
    coherent and populated rows return after recovery.
  - [x] Confirm the Master Volume bar remains pinned, synchronized, and unaffected by row
    interactions.
  - [x] Recheck Tab, Shift+Tab, Space, and Escape across the popup, including moving from the tab
    rail into row controls without losing visible focus.
  - [x] Check dark and light themes at scale 1.0 and 1.25 for clipped titles, controls, badges,
    percentages, focus borders, or selection fills.
  - [x] Close and reopen after default/mute/volume/tab changes; confirm no stale hover, focus,
    selected, muted, or empty state remains.
- **Corrections:** The focused empty-state fixture initially asserted effective visibility while
  its temporary TestCase parent was hidden; coverage was narrowed to the observable zero-count
  mapping and shared message contract, with live visibility retained in this checklist. Manual
  review then found that changing the default device left the first row selected because
  `ListView.currentIndex` defaults to zero and shared delegates include `isCurrentItem` in their
  selected state. Synchronizing the current item fixed the live transition but reopening the
  popup could recreate the list with row zero current after the stable default roles were loaded.
  Device lists now disable independent current-item selection and derive selection solely from
  the backend-owned default role. Focused two-row coverage verifies both live default changes and
  recreation with the second device already default.
- **Upstream note:** UP-005 resolved by Controls `a5af7e2`. The installed theme exposes
  `subtitlePresentation: HnListDelegate.SingleLine`; CP-A-002 uses it. UP-006 resolved by Controls
  `67b7f63`; CP-A-002 and CP-A-003 request `leadingContentAlignment: Qt.AlignVCenter`, and focused
  coverage verifies both delegates opt into the new policy.
- **User decision and date:** Approved, 2026-07-29

### MS-A-001 — Audio popup milestone

- **State:** Approved
- **Prerequisite checkpoint decisions:** CP-A-001 through CP-A-005 approved
- **Automated verification:** Focused Audio QML coverage, `task qml-lint`, QML type
  metadata/packaging, all 1,079 CTest entries, and `git diff --check` passed; one environment-
  dependent monitor validation case was skipped as expected.
- **Manual verification:** The user completed the bundled Audio checklist, including output/input
  devices, application streams, default/mute/volume behavior, empty and recovery states, keyboard
  navigation, popup reopening, leading-icon alignment, dark/light themes, and scales 1.0/1.25.
- **Screenshots and automated visual comparison:** Not performed, by user instruction.
- **User decision and date:** Approved, 2026-07-29

### CP-R-001 through CP-R-009 — Right Sidebar bundled implementation

- **State:** Approved
- **Started:** 2026-07-29
- **Review exception:** The user explicitly requested that all Right Sidebar checkpoints be
  implemented together, that automated screenshots and visual validation be omitted, and that a
  single manual checklist be presented after implementation.
- **Files in scope:** `SidebarTabButton.qml`; notification rules and empty state; overview
  notification rows and upcoming loading state; System default-app rows and diagnostics;
  Quick Settings charge-limit and brightness rows; focused QML coverage; pipeline ledgers.
- **Preserved contracts:** Tab metadata and `clicked()`; MIME/category candidate filtering and
  `defaultChanged`; notification enable/urgency writes; grouped notification count/time/overflow
  and tab routing; calendar state machine; diagnostic refresh/rebuild actions; charge-limit
  availability; brightness throttling, commit, and external synchronization.
- **Shared-control mapping:** `HnNavigationDelegate`, `HnSettingsRow`, `HnIconComboBox`,
  `HnEmptyState`, `HnListDelegate`, `HnLoadingState`, and `HnStatusIndicator`.
- **Focused tests:** `RightSidebarSharedControlsQmlTests` covers tab selection/badge/activation,
  notification shared rows/switch/combo composition, and brightness row/throttle composition.
  It also verifies that the default-app combo synchronizes after its shared-control Loader becomes
  ready and tracks changed and unavailable defaults. Existing component-instantiation,
  brightness-throttling, and charge-limit tests remain applicable.
- **Automated verification:** Focused Right Sidebar shared-control tests (6/6), component
  instantiation (15/15), brightness resilience (6/6), and power-extension coverage (4/4) passed.
  `task qml-lint`, `task qmltypes-check`, all 1,079 CTest entries through `task test`, and
  `git diff --check` passed; one environment-dependent monitor test was skipped as expected.
- **Screenshots and automated visual comparison:** Not performed, by user instruction.
- **Manual review:** The user completed the consolidated checklist, reported the sizing and
  scrolling corrections, and verified the final shared grow-then-scroll behavior.
- **Corrections:** Manual review found `ReferenceError: appCombo is not defined`, incorrect
  first-candidate display after reopening System, non-scrolling Notification content, and clipped
  System content. `DefaultAppRow` now accesses the loaded combo through
  `HnSettingsRow.controlItem`, resynchronizes when the Loader becomes ready, and adapts icon names
  without losing desktop-file or MIME-role data. `SidebarContent` now explicitly publishes the
  loaded tab height to its `ScrollView`, disables horizontal scrolling, and enables a vertical
  scrollbar whenever content exceeds the bounded viewport. Re-review confirmed scrolling but
  found that initial-open growth could still finish at the stale 600-pixel fallback: the tab's
  natural height arrived while the opening animation was running, and that animation later
  overwrote the live height update. The shared opening sequence now reconciles its final height
  to the latest bounded request. A second re-review showed that every tab was still 24 pixels
  shorter than its complete natural size because the height forwarded to the surface omitted the
  shared `ScrollView`'s 12-pixel top and bottom insets. `SidebarContent` now includes those insets
  in its preferred height. This applies the same grow-to-complete-natural-height,
  clamp-to-screen, then-scroll-overflow policy to every tab. Component coverage verifies both the
  late animation request and the inset-inclusive height calculation.
- **Upstream note:** None.
- **User decision and date:** Approved, 2026-07-29.

### MS-R-001 — Right Sidebar milestone

- **State:** Approved
- **Prerequisite checkpoint decisions:** CP-R-001 through CP-R-009 approved
- **Automated verification:** Focused Right Sidebar and component-instantiation coverage,
  `task qml-lint`, `task qmltypes-check`, all 1,079 CTest entries, and `git diff --check`
  passed; one environment-dependent monitor validation case was skipped as expected.
- **Manual verification:** The user completed the bundled Right Sidebar checklist and verified
  the corrected default-application state, tab scrolling, and shared grow-to-available-height
  behavior.
- **Screenshots and automated visual comparison:** Not performed, by user instruction.
- **User decision and date:** Approved, 2026-07-29.

### CP-Z-001 — Decision and upstream reconciliation

- **State:** Approved
- **Checkpoint decisions:** CP-F-001 through CP-R-009 and MS-S-001 through MS-R-001 are approved.
  No candidate was skipped or deferred.
- **Upstream findings:** UP-001 through UP-006 are verified fixed. The stale UP-001 and UP-003
  dispositions were reconciled with their successful downstream retries and approved checkpoints.
- **Open blockers:** None.
- **User decision and date:** Approved, 2026-07-29.

### CP-Z-002 — Final repository verification

- **State:** Approved
- **Required checks:** `task format-check`, `task qml-lint`, `task qmltypes-check`, `task test`,
  `git diff --check`, and baseline-to-HEAD diff review.
- **Results:** `task format-check`, `task qml-lint`, and `task qmltypes-check` passed. All 1,079
  CTest entries passed; the environment-dependent
  `SidebarManagerMonitorValidation.AcceptsAnyCurrentlyConnectedNonEmptyNamedScreen` case was
  skipped as expected. `git diff --check` passed against implementation baseline
  `a7f0c822c3121018620366f6354700f413d7428a`.
- **Diff review:** The 49 changed files are confined to the approved Settings, Launcher, Network,
  Audio, and Right Sidebar adoption scopes; their supporting assets and focused tests; and this
  pipeline's SDD records. No excluded control adoption, generated Wayland file, protocol source,
  secret, credential, dependency lockfile, or unrelated file changed.
- **Diagnostics:** CMake emitted the existing Qt `GuiPrivate` version-coupling warning during
  configuration. It did not fail a check.
- **User decision and date:** Approved, 2026-07-29.

### CP-Z-003 — Pipeline closure

- **State:** Approved
- **Changed surfaces:** Settings, Launcher, Network popup, Audio popup, and Right Sidebar now use
  the approved matching `Holonight.Controls` primitives while preserving application-owned
  behavior and public contracts.
- **Supporting changes:** Focused C++/QML regression coverage, shared-control instantiation
  coverage, two Network action icons, and the adoption specification, design, task, review, and
  upstream ledgers.
- **Verification:** Final formatting, QML lint, qmltypes/package validation, all 1,079 CTest
  entries, `git diff --check`, and the full baseline-to-HEAD scope review passed. One
  environment-dependent monitor test was skipped as expected.
- **Milestones:** Settings, Launcher, Network, Audio, and Right Sidebar milestones are approved.
- **Skipped or deferred candidates:** None.
- **Upstream dispositions:** UP-001 through UP-006 are verified fixed; no upstream blocker remains.
- **User decision and date:** Approved, 2026-07-29.

## Milestone Evidence Template

### MS-___ — Surface

- **Prerequisite checkpoint decisions:** —
- **Dark, scale 1.0:** —
- **Dark, scale 1.25:** —
- **Light, scale 1.0:** —
- **Light, scale 1.25:** —
- **`task qmltypes-check`:** —
- **`task test`:** —
- **Live workflow:** —
- **Screenshots:** —
- **Diagnostics:** —
- **User decision and date:** —

## Review Rules

- Only explicit user review changes a checkpoint to `Approved` or `Skipped by decision`.
- A visual or behavioral regression changes it to `Needs correction`; correction evidence is
  appended to the same record.
- An upstream deficiency changes it to `Blocked upstream`, links an `UP-*` entry, and halts the
  pipeline.
- Screenshot files are temporary. The comparison conclusion and decision remain here.
- Only one checkpoint may be active. No evidence from a later component may be bundled into the
  active review.

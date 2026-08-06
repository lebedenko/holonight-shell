# Shared Controls Adoption Upstream Notes

**Upstream project:** `holonight-qt`
**Adoption baseline:** `00b1b55d88a9d20f527559b8551ad664760c2a7b`
**Status:** No open upstream blockers

This is the local actionable ledger for shared-control deficiencies discovered while adopting
`Holonight.Controls`. It is not permission to change the sibling repository. Upstream changes,
new public APIs, or issue filing require their own authorized workflow.

## Dispositions

Use one of:

- `Open — awaiting user disposition`
- `Accepted upstream`
- `Fixed upstream; awaiting downstream retry`
- `Verified fixed`
- `Rejected with rationale`
- `Deferred by decision`

An open item keeps its downstream checkpoint `Blocked upstream`. Do not close an item merely
because a downstream workaround is possible.

## Finding Template

### UP-___ — Concise title

- **Checkpoint:** CP-___
- **Disposition:** Verified fixed
- **Upstream revision:** `00b1b55d88a9d20f527559b8551ad664760c2a7b`
- **Affected control/API:** —
- **Downstream component:** —
- **Minimal reproduction:** —
- **Expected behavior:** —
- **Actual behavior:** —
- **Evidence:** test output, QML warning, screenshot identifier, or measured geometry
- **Why downstream composition is insufficient:** —
- **Suggested upstream correction:** —
- **Compatibility considerations:** —
- **Downstream restoration:** commit/state restored to the last approved checkpoint
- **Owner/link:** —
- **Disposition notes and date:** —

## Findings

### UP-001 — Installed Controls module is not consumable by `qmllint`

- **Checkpoint:** CP-S-001
- **Disposition:** Verified fixed
- **Upstream revision:** `00b1b55d88a9d20f527559b8551ad664760c2a7b`
- **Affected control/API:** Installed `Holonight.Controls` tooling metadata; first observed with
  `HnNavigationDelegate`
- **Downstream component:** `apps/settings/qml/NavPanel.qml`
- **Minimal reproduction:** At the recorded revisions, replace the handcrafted NavPanel delegate
  with `HnNavigationDelegate`, add `import Holonight.Controls`, then run
  `/usr/lib/qt6/bin/qmllint -I /tmp/holonight-qt-prefix/lib/qt6/qml
  apps/settings/qml/NavPanel.qml`.
- **Expected behavior:** `qmllint` resolves the public type from the installed module without
  warnings, matching successful runtime dynamic discovery.
- **Actual behavior:** `qmllint` reports `HnNavigationDelegate was not found` and cascading
  required-property warnings. The installed
  `Holonight/Controls/holonight_controls_qml.qmltypes` is eight lines and contains only
  `Module {}`, although `qmldir` and the QML file list the public type.
- **Evidence:** The focused Settings behavior/instantiation test passed before and after the
  attempted migration. The three canonical Controls smoke tests passed at CP-F-003. The mandatory
  `qml-lint` target completed with unresolved-type warnings for `NavPanel.qml` and
  `SettingsWindow.qml`.
- **Why downstream composition is insufficient:** The control works at runtime but cannot satisfy
  the pipeline's warning-free lint gate. Linking `HolonightQt::Controls`, copying the control, or
  suppressing the warning would either violate the validated dynamic-discovery design or hide the
  packaging/tooling defect.
- **Suggested upstream correction:** Make the installed Controls module lint-consumable so all
  public QML controls are discoverable from its install prefix. Ensure the generated tooling
  metadata or installed `qmldir`/QML-source arrangement exposes the QML types to `qmllint`, and add
  a package-install test that imports and instantiates every public Controls type under
  `qmllint`.
- **Compatibility considerations:** Preserve the existing `Holonight.Controls` URI, public type
  names, runtime plugin loading, installed QML paths, and dynamic downstream consumption.
- **Downstream restoration:** `NavPanel.qml` restored exactly to the last approved source state;
  the focused page-order and pointer-routing regression test remains because it passes against
  that state.
- **Owner/link:** Fixed by upstream commit
  `dbbccd045063b8063512e946da8593f7f048dd8c`; remediation handoff:
  `../holonight-qt/docs/controls-qmllint-install-metadata-remediation.md`
- **Disposition notes and date:** Opened 2026-07-28. User chose to fix this in a separate
  `holonight-qt` session. Upstream package-install lint and the downstream `task qml-lint` passed
  on 2026-07-28 against `dbbccd045063b8063512e946da8593f7f048dd8c`; CP-S-001 was retried
  successfully.

### UP-002 — `HnChoiceCard` replaces its surface with a translucent pressed overlay

- **Checkpoint:** CP-S-004
- **Disposition:** Verified fixed
- **Upstream revision:** `6ffdae297caa665cb9a26fb611837035174eeb31`
- **Affected control/API:** `HnChoiceCard` pressed background composition
- **Downstream component:** Accent choices in `apps/settings/qml/AppearancePage.qml`
- **Minimal reproduction:** Place `HnChoiceCard` on an opaque dark surface, press and hold it,
  and inspect the background. Repeat while `checked` and while the downstream application has
  draft theme choices that differ from the applied `HoloniightPalette`.
- **Expected behavior:** Press feedback layers `pressedOverlay` over the card's normal,
  hovered, or selected opaque surface. The result remains recognizably related to that base
  state and does not expose the ancestor background.
- **Actual behavior:** `HnChoiceCard.background.color` becomes the translucent
  `HoloniightPalette.pressedOverlay` whenever `down` is true. The overlay therefore composites
  directly over the ancestor surface, losing the card's base surface. In Settings this produced
  a muddy green pressed fill on the Cyan choice.
- **Evidence:** User capture `/tmp/scr.png` from 2026-07-28; the pressed fill includes sampled
  pixel `sRGB(82,101,33)`. The attempted checkpoint capture is
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-004-post-current-s1.0-default.png`.
- **Why downstream composition is insufficient:** Overriding the shared background or
  calculating pressed colors in Settings would duplicate control-state styling and violate the
  adoption requirement that shared controls own selection and interaction colors.
- **Suggested upstream correction:** Keep an opaque state-aware base surface and render
  `pressedOverlay` as a separate overlay layer, or provide an equivalent correctly composited
  semantic pressed-surface result within `HnChoiceCard`. Cover checked and unchecked cards over
  different ancestor colors.
- **Compatibility considerations:** Preserve the public properties, signals, geometry,
  accessibility, keyboard behavior, and `ButtonGroup` composition of `HnChoiceCard`.
- **Downstream restoration:** The accent delegate and CP-S-004 focused test were restored to the
  last approved CP-S-003 state.
- **Owner/link:** Fixed by upstream commit
  `6ffdae297caa665cb9a26fb611837035174eeb31`; handoff:
  `../holonight-qt/docs/hn-choice-card-pressed-overlay-composition-bug.md`
- **Disposition notes and date:** Opened 2026-07-28. User requested an upstream bug report and
  addressed it in a separate `holonight-qt` session. The two focused upstream rendered QML
  regressions and package-install test passed on 2026-07-28; the committed control was installed
  byte-for-byte to `/tmp/holonight-qt-prefix`. The downstream focused behavior test, QML lint,
  qmltypes check, warning-free live instantiation, and fresh visual capture passed during the
  CP-S-004 retry.

### UP-003 — Stacked `HnSettingsRow` controls do not span a consistent full-width row

- **Checkpoint:** CP-S-006
- **Disposition:** Verified fixed
- **Upstream revision:** `351b91d4e437a772e440ed7ae60e222eac5b75a3`
- **Affected control/API:** `HnSettingsRow.stacked` control-slot geometry
- **Downstream component:** Shape scale and base-radius/base-chamfer override rows in
  `apps/settings/qml/AppearancePage.qml`
- **Minimal reproduction:** Place two full-width `HnSettingsRow` instances in a `ColumnLayout`,
  set `stacked: true`, give them different-length `titleText` values, and load an identical
  `RowLayout` containing a `Switch`, fill-width `Slider`, and trailing value into each `control`
  slot.
- **Expected behavior:** As described by the gallery's stacked-row example, the control receives
  the full content width below the title. Identical compound controls therefore start at the same
  horizontal position across rows, and controls within each row share a consistent vertical
  center.
- **Actual behavior:** `HnSettingsRow` changes its grid to two columns when stacked, but places
  `controlLoader` at column 1 without spanning both columns. Column 0 retains the current row's
  title width, so each control origin depends on that title. In the Settings review, the radius
  and chamfer switches/sliders began at different x coordinates. The compound switch and slider
  also appeared vertically offset within the loaded row.
- **Evidence:** User review capture `/tmp/codex-clipboard-EMHKNp.png` from 2026-07-28 and checkpoint
  capture
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-006-post-current-s1.0-default.png`.
- **Why downstream composition is insufficient:** Per-row margins, fixed widths, or offsets would
  compensate for title-dependent shared geometry and break responsive sizing. They would also
  duplicate layout behavior that `stacked` explicitly promises to own.
- **Suggested upstream correction:** In stacked mode, place the control loader on the row below
  the title and span the complete grid width, independent of title width. Define vertical
  alignment for compound loaded content so standard controls share a common center. Add a
  rendered geometry regression with different title lengths and identical switch/slider/value
  content.
- **Compatibility considerations:** Preserve the existing public properties, slot aliases,
  semantic size roles, focus forwarding for single controls, non-stacked geometry, and natural
  heights. Avoid requiring downstream fixed widths.
- **Downstream restoration:** Shape scale and both override rows, plus their focused behavior
  test, were restored to the last approved CP-S-005 state.
- **Owner/link:** Fixed by upstream commit
  `351b91d4e437a772e440ed7ae60e222eac5b75a3`; handoff:
  `../holonight-qt/docs/hn-settings-row-stacked-control-alignment-bug.md`
- **Disposition notes and date:** Opened 2026-07-28 after user review. The two focused upstream
  geometry/focus regressions passed, and the fixed control was installed byte-for-byte to
  `/tmp/holonight-qt-prefix`. CP-S-006 was retried with focused downstream behavior and geometry
  coverage, QML lint, qmltypes validation, warning-free live instantiation, and fresh screenshots.
  The user approved CP-S-006 on 2026-07-28.

### UP-004 — `HnActionBar` does not right-align trailing content without center content

- **Checkpoint:** CP-S-011
- **Disposition:** Verified fixed
- **Upstream revision:** `4003d45f65a7b7103d5b98cbbebda13a776aade4`
- **Affected control/API:** `HnActionBar.trailingContent` with `centerContent` unset
- **Downstream component:** `apps/settings/qml/FooterBar.qml`
- **Minimal reproduction:** Give a full-width `HnActionBar` both `leadingContent` and
  `trailingContent`, leave its optional `centerContent` unset, and render it wider than the two
  items' combined implicit widths.
- **Expected behavior:** The documented optional leading/center/trailing composition keeps
  leading content at the left and trailing content at the right even when no center item exists.
- **Actual behavior:** The inactive, invisible center loader is ignored by `RowLayout`, so its
  `Layout.fillWidth` does not consume the remaining width. The trailing loader is placed directly
  after the leading loader; `Layout.alignment: Qt.AlignRight` cannot align it within space its
  layout cell does not own.
- **Evidence:** Rejected downstream capture
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-011-post-current-s1.0-footer.png` and diff
  `/tmp/holonight-shared-controls-review.eVnLYi/cp-s-011-diff.png`. The action group visibly moves
  from the footer's right edge to immediately after the version label.
- **Why downstream composition is insufficient:** Supplying an empty fake `centerContent` merely
  to activate the stretch loader works around shared slot geometry in every consumer and makes
  an optional slot effectively required. Reimplementing the spacer outside the control defeats
  the composition adoption.
- **Suggested upstream correction:** Keep a layout-managed stretch cell active even when
  `centerContent` is null, or otherwise make the trailing loader own/align within the remaining
  row width. Add rendered geometry tests for leading-only, trailing-only, leading+trailing, and
  all-three-slot combinations.
- **Compatibility considerations:** Preserve the public slot properties, item aliases, divider,
  implicit sizing, accessibility role, and current geometry when actual center content exists.
- **Downstream restoration:** `FooterBar.qml` and its tests were restored to the approved
  CP-S-010 state.
- **Owner/link:** Fixed by upstream commit
  `802c6cb5161b1a25a5842684b9653546e46af1bc`; handoff:
  `../holonight-qt/docs/hn-action-bar-trailing-slot-alignment-bug.md`
- **Disposition notes and date:** Opened 2026-07-29. The installed
  `/tmp/holonight-qt-prefix` copy of `HnActionBar.qml` was verified byte-for-byte against upstream
  HEAD `703b9237351416d96664859d8e388085422e1920`. The downstream focused behavior/geometry tests,
  QML lint, qmltypes check, warning-free live instantiation, and visual retry passed.

### UP-005 — `HnListDelegate` cannot keep subtitles to one elided line

- **Checkpoint:** CP-A-002
- **Disposition:** Verified fixed
- **Upstream revision:** `a5af7e2`
- **Affected control/API:** `HnListDelegate.subtitle` presentation policy
- **Downstream component:** `apps/shell/qml/Popups/Audio/AudioDeviceDelegate.qml`
- **Minimal reproduction:** Use an `HnListDelegate` with a long device identifier as `subtitle`,
  a fixed 64-pixel row height, and conditional trailing content such as the DEFAULT badge. Toggle
  the trailing content while keeping the subtitle unchanged.
- **Expected behavior:** A fixed-height dense row can request a one-line, right-elided subtitle,
  so conditional trailing content changes available width without changing the row's text-line
  count or vertical rhythm.
- **Actual behavior:** The subtitle label unconditionally uses `Text.WordWrap` and exposes no
  maximum-line or elide policy. Showing the badge narrows the text column and can wrap the
  identifier onto a second subtitle line, producing three text lines inside a two-line row.
- **Evidence:** User manual review on 2026-07-29; the DEFAULT row wrapped
  `alsa_output.pci-0000_00_1f.3.analog-stereo` while the same row without the badge retained the
  intended two-line hierarchy.
- **Why downstream composition is insufficient:** Reserving badge width wastes space on every
  non-default row and still cannot guarantee one line for arbitrary identifiers. Replacing the
  shared content item or hiding metadata would bypass the adopted delegate contract.
- **Suggested upstream correction:** Expose subtitle line-count, wrap, and elide policy with
  backward-compatible defaults, or provide an explicit single-line subtitle mode. Add geometry
  coverage for conditional trailing content in a fixed-height row.
- **Compatibility considerations:** Preserve current wrapping as the default, along with title,
  metadata, leading/trailing slots, implicit sizing, accessibility, and selection behavior.
- **Downstream restoration:** `AudioDeviceDelegate` requests
  `subtitlePresentation: HnListDelegate.SingleLine`; focused coverage verifies the policy and the
  installed theme contains the API.
- **Owner/link:** —
- **Disposition notes and date:** Opened 2026-07-29 after bundled Audio manual review. Resolved
  by Controls `a5af7e2` and adopted downstream on 2026-07-29.

### UP-006 — `HnListDelegate` cannot vertically center leading content

- **Checkpoint:** CP-A-002 and CP-A-003
- **Disposition:** Verified fixed
- **Upstream revision:** `67b7f63`
- **Affected control/API:** `HnListDelegate.leadingContent` layout alignment
- **Downstream components:** `apps/shell/qml/Popups/Audio/AudioDeviceDelegate.qml` and
  `AudioStreamDelegate.qml`
- **Minimal reproduction:** Use an `HnListDelegate` with a title, subtitle, and square icon in
  `leadingContent`. Render the row at the large control size.
- **Expected behavior:** Consumers can vertically center a primary icon across the two-line text
  block while retaining top alignment as the backward-compatible default where appropriate.
- **Actual behavior:** The internal leading loader hardcodes
  `Layout.alignment: Qt.AlignTop`. The public slot cannot override the loader's layout alignment,
  leaving Audio icons aligned beside the title rather than centered across title and subtitle.
- **Evidence:** User manual review on 2026-07-29 of output-device and application-stream rows.
- **Why downstream composition is insufficient:** Adding invisible height or top padding around
  every icon would encode assumptions about shared font metrics, semantic spacing, theme, and
  display scale. It would duplicate the workaround in both Audio delegates.
- **Suggested upstream correction:** Expose a `leadingContentAlignment` flag property used by the
  leading loader, defaulting to `Qt.AlignTop` for compatibility. Add coverage for top and vertical
  center alignment with two-line content.
- **Compatibility considerations:** Preserve the current default, slot ownership, implicit
  sizing, spacing, title/subtitle behavior, and leading/trailing item aliases.
- **Downstream restoration:** Set both Audio row delegates to the upstream vertical-center mode
  and add focused property/geometry coverage.
- **Owner/link:** —
- **Disposition notes and date:** Opened 2026-07-29 after bundled Audio manual review. Resolved by
  Controls `67b7f63`; the installed module contains the API, both Audio delegates opt into
  `Qt.AlignVCenter`, and focused coverage verifies the requested policy on 2026-07-29.

# U-11 — Settings Application

**Task**: T-011 · **Skills**: qt-cpp-review + qt-qml-review (mixed unit) · **Scope**: 20 files
- C++ (13 files): `apps/settings/src/ConfigFileService.{h,cpp}`, `FontListModel.{h,cpp}`, `main.cpp`, `SettingsApplication.{h,cpp}`, `SettingsEditModel.{h,cpp}`, `ShellStatusService.{h,cpp}`, `ThemeConfigFile.{h,cpp}`
- QML (7 files): `apps/settings/qml/SettingsWindow.qml`, `NavPanel.qml`, `ContentStack.qml`, `PlaceholderPage.qml`, `FooterBar.qml`, `AppearancePage.qml`, `BarPage.qml`

## Prior Context / Mandatory-check verdicts

1. **Architectural isolation from the shell's service graph** (DESIGN.md §1.1) — CONFIRMED clean. `apps/settings/CMakeLists.txt` links only `holonight_config`, `HolonightQt::Config`, `Qt6::{Core,Gui,Quick,Qml}`, and `holonight_componentsplugin`. A grep of every `#include` in `apps/settings/src/` found zero references to `libs/holonight-services`, `libs/holonight-surfaces`, `libs/holonight-platform`, or `libs/holonight-core`. The one non-obvious angle-bracket include, `<holonight/theme_catalog.h>` (in `ThemeConfigFile.cpp`), was traced to the external `HolonightQt::Config` package installed at `/usr/include/holonight/` — not a shell-service leak.
2. **`SettingsEditModel`'s `current_`/`snapshot_` dirty-tracking pattern** — CONFIRMED correct, independently verified by three separate agents (Model Contracts, Performance, API/Correctness). `ParsedConfig` and every nested config struct use a C++20 defaulted `operator==`, so `recomputeDirty()` performs a genuine deep field-by-field comparison, not a shallow/pointer one. No double-counting, no stale-flag desync found across load/save/discard cycles.
3. **`ConfigFileService`'s TOML read/write safety** — traced the null-vs-empty-`QString` question explicitly (the project's documented SQLite `NOT NULL` gotcha does **not** reproduce here): `uiFont`/`fixedFont` flow through `tomlQuote()` via plain `QTextStream` concatenation, which produces identical, valid output (`""`) for both a null and an explicit-empty `QString` — no TOML-writing equivalent of the SQL-binding trap exists. However, this pass surfaced a **separate, serious data-integrity bug** — see D-C1 below.
4. **`FontListModel`/`ShellStatusService` standard Qt correctness** — `FontListModel`'s `roleNames()`/`data()`/`rowCount()` contract is fully consistent (only `Qt::DisplayRole` declared and handled). Both services turned out to be **entirely synchronous, single-threaded** (no `QThread`/`QtConcurrent`/`moveToThread` anywhere in the app) — `FontListModel::rebuild()` scans `QFontDatabase::families()` and `ShellStatusService::detectShellProcess()` walks `/proc` directly on the main/GUI thread, both confirmed clean of any cross-thread marshalling risk since there is no cross-thread activity at all in this app.

## Tool Sign-off

**C++ Phase 1 (Python lint)**: 14 hits, all `VAR-3` (direct brace initialization, pure style) across `ConfigFileService.h:34`, `FontListModel.h:31`, `SettingsEditModel.h:75-81` (7 hits), `ShellStatusService.h:29`, `ThemeConfigFile.cpp:44,49,54,79,85`.

**QML Phase 1 (Python lint)**: 29 hits — `ORD-1` (attribute ordering, 13, style noise, excluded), `STY-3` (anchors dot-notation, 4, style noise, excluded), `BND-1` (`property var`, 4), `PRF-3` (`clip: true`, 2), `PRF-1` (transparent Rectangle, 1), `STY-1` (missing `id: root`, 1).

**QML Phase 1b (qmllint)**: ran clean via the project's import-path configuration — **0 warnings across 7 files**.

**Phase 2 (twelve parallel deep-analysis agents — 6 for C++, 6 for QML)**: all twelve completed. One cross-agent contradiction was resolved by consulting the actual project memory file directly (see Ruled Out).

## Confirmed Findings

### D-C1 — `ConfigWriter` silently deletes `weather.latitude`/`longitude`/`city` on every save [C++] (92/100)
- **Severity**: High · **Effort**: Low
- **Location**: `libs/holonight-config/src/ConfigWriter.cpp:168-177` (the `[weather]` block writes `api_key`/`geo_api_key`/`units`/`lang`/`refresh_interval` but hardcodes `latitude`/`longitude` as commented-out placeholder text and never writes `city` at all), paired with the parse side at `libs/holonight-config/src/ConfigParsers.cpp:254-287` (which correctly reads all three fields from disk)
- **Rationale**: `ConfigFileService::save()` is the sole production call site of `ConfigWriter::write()` besides a one-time bootstrap default-file writer. Traced end-to-end: a user's `weather.latitude`/`longitude`/`city` load correctly into `ParsedConfig`, pass through `SettingsEditModel` untouched (no UI in this app edits these fields), and reach `ConfigWriter::write()` — which then emits `# latitude = 49.83968 ...` as a literal comment and omits `city` entirely. Any user with a pinned weather location loses that data — silently converted to a commented-out example — the instant they open holonight-settings and click Apply/Save, even if they touched nothing weather-related. This reverts their weather source to IP geolocation with no warning.
- **Suggested Direction**: In `ConfigWriter::write`, conditionally emit `latitude =`/`longitude =`/`city =` when those `ParsedConfig::weather` fields are set, mirroring how every other field in the struct already round-trips. This is a shared-library fix (`libs/holonight-config`) but is only exercised at runtime through this app's save path.

### D-C2 — Unregistered `HolonightTheme` singleton breaks the settings app's default landing page [QML] (95/100)
- **Severity**: High · **Effort**: Medium
- **Location**: `apps/settings/qml/AppearancePage.qml:58` (`model: HolonightTheme.themeFamilies`), `:121` (`model: HolonightTheme.accentOptionsForScheme(editModel.themeScheme)`)
- **Rationale**: `HolonightTheme` is referenced in exactly one file in the entire repository and is never registered anywhere — no `qmlRegisterSingletonType`, no `QML_SINGLETON`, no `HolonightTheme.qml`. The only theme-related C++ class in this app, `ThemeConfigFile`, is a non-QObject static helper with no matching API surface. Since `AppearancePage` is the default/first-shown page (both `NavPanel` and `ContentStack` default `currentPage` to `"appearance"`), every user hits a silent `ReferenceError` on first launch — the Theme and Accent picker sections of the landing page render empty, with no visible error (compounded by D-Q1 below, since `ContentStack`'s `Loader` has no error handling either).
- **Suggested Direction**: Register a `HolonightTheme` `QML_SINGLETON` (likely in `libs/holonight-config` alongside `ThemeConfigFile`, or a new small class) exposing `themeFamilies` and `accentOptionsForScheme(scheme)`, sourced from the same catalog `ThemeConfigFile`/`HoloniightPalette` already use, and wire it into `apps/settings/CMakeLists.txt`'s QML module sources. Until fixed, treat AppearancePage's Theme/Accent sections as non-functional, not merely unstyled.

### D-C3 — ComboBox/Slider bindings silently destroyed by control-internal writes after first user interaction [QML] (82/100, corroborated by two independent agents at 82 and 80)
- **Severity**: Medium · **Effort**: Small
- **Location**: `AppearancePage.qml:156-163` (uiFontCombo), `:171-178` (fixedFontCombo), `:190-195`/`:214-219` (font-size Sliders); `BarPage.qml:54-58`/`:85-89` (workspace-count/tray-max-items Sliders)
- **Rationale**: Each of these 6 controls declares `value`/`currentIndex` as a live binding to an `editModel` property, with a separate write-back in `onMoved`/`onActivated`. `Slider`/`ComboBox` internally write their own `value`/`currentIndex` Q_PROPERTY as part of handling drag/selection — any imperative write to a QML property, including one from a Control's own implementation, permanently destroys a prior declarative binding on that property. Once a user interacts with any of these 6 controls once, that control desyncs from `editModel`; a later external change (e.g. "Discard Changes" → `fileService.load()` → `SettingsEditModel::setFromParsedConfig()` re-emitting the underlying `*Changed` signals) will not be reflected on screen for that control while the page instance stays alive.
- **Suggested Direction**: Re-establish the binding after each write-back — e.g. wrap in `Binding { target: control; property: "value"; value: editModel.x; restoreMode: Binding.RestoreBinding }`, or re-apply `Qt.binding(() => editModel.x)` inside `onMoved`/`onActivated` immediately after updating the model.

### D-C4 — Mode→scheme mapping ternary duplicated across 3 classes / up to 6 call sites [C++] (88/100, independently found by two agents)
- **Severity**: Low-Medium · **Effort**: Low
- **Location**: `ThemeConfigFile.cpp:101-103` (`writeMode`), `ConfigFileService.cpp:33-34` (`migrateLegacyTheme`), `SettingsEditModel.cpp:28-30` (`setThemeModeSnapshot`), `:62-64` (`markSaved` 2-arg overload), `:122-123` (`setThemeMode`)
- **Rationale**: The identical `normalizeMode(x) == "light" ? "holonight-light" : ThemeConfigFile::defaultScheme()` branch is hand-written 5-6 times across three classes instead of living behind a single helper. No live bug today (all sites currently agree), but a future catalog change (a second light/dark tier, real "system" support) risks updating some sites and missing others.
- **Suggested Direction**: Extract a single `static QString ThemeConfigFile::schemeForMode(const QString&)` helper; replace all call sites.

### D-C5 — Asymmetric error reporting: load failures are invisible, save failures are not [C++] (85/100)
- **Severity**: Medium · **Effort**: Low-Medium
- **Location**: `ConfigFileService.cpp:45-68` (`load()`) vs `:70-103` (`save()`); `FooterBar.qml:91` ("Discard Changes" → `fileService.load()`), `:136-142` (`onSaveError` wired to a modal dialog, no `load` equivalent)
- **Rationale**: `save()` has a full `saveStarted`/`saveFinished(bool)`/`saveError(QString)` signal contract that FooterBar.qml uses to show a "Save Failed" dialog. `load()` has no equivalent — it always `return true` even when TOML parsing throws (caught and only `qCWarning`'d), with no `loadError` signal. If `config.toml` is corrupt, clicking "Discard Changes" (or the app's own startup load) silently resets every field to hardcoded defaults with zero user-visible indication — the user may believe their prior settings were restored when they were actually wiped.
- **Suggested Direction**: Emit a `loadError(QString)` signal from the `catch` branch in `ConfigFileService::load()`, and wire it in FooterBar.qml the same way `onSaveError` is handled.

### D-C6 — `FontListModel::roleNames()` rebuilds its `QHash` on every call [C++] (85/100)
- **Severity**: Low · **Effort**: Trivial
- **Location**: `FontListModel.cpp:49`
- **Rationale**: Constructs a fresh single-entry `QHash` on every invocation instead of caching it. Standard anti-pattern; low real-world impact since the map has one entry and view code typically calls `roleNames()` once at attach time, not per-row.
- **Suggested Direction**: Return a `static const QHash<int, QByteArray>` or a member initialized once in the constructor.

### D-Q1 — `ContentStack`'s page-switching `Loader` has no error handling [QML] (70/100 — see note)
- **Severity**: Medium · **Effort**: Low
- **Location**: `ContentStack.qml:12-21`
- **Rationale**: The `Loader` binds only `source:` with no `onStatusChanged`/`Loader.Error` handling. Combined with D-C2, this means the app's own default page currently fails silently with a blank content pane and no user-facing diagnostic — only a console-side QML error. General robustness gap for the primary page-dispatch mechanism, with 11 of 13 nav entries currently resolving to `PlaceholderPage` (only `AppearancePage`/`BarPage` meaningfully exercise this Loader today).
- **Note**: originally scored 70/100 (investigation-tier) in isolation, but its user-facing impact is elevated by directly compounding D-C2's silent failure — included here as a paired finding rather than a standalone investigation target.
- **Suggested Direction**: Add an `id`, an `onStatusChanged` handler checking `Loader.Error`, and a minimal fallback message or console warning naming the failing page key.

### D-Q2 — `parent.modelData` anti-pattern in Repeater delegates instead of an explicit id [QML] (84/100, corroborated across two agents)
- **Severity**: Medium · **Effort**: Low
- **Location**: `NavPanel.qml:57,64,65,74,75`; `AppearancePage.qml:68,95,102,137,144`; and specifically `LauncherRightPanelSearch.qml`-style fragility in `AppearancePage.qml`'s family delegate (`id: familyDelegate` exists at line 60, yet the child `Text` at line 68 still uses `parent.modelData.name` instead of `familyDelegate.modelData.name`)
- **Rationale**: This is the exact fragility class already recorded in this project's own memory (`feedback_holonight_combobox_parent.md`: "parent.text/parent.highlighted ... resolve to QQuickItem; always use explicit id ... and reference by id"). Nothing is broken today (children happen to be direct delegate-root children), but inserting any wrapper `Item` for padding/animation would silently break `modelData` resolution with no compile-time warning — this project has hit this exact bug class before.
- **Suggested Direction**: Give every delegate root an explicit id and reference that id instead of `parent` throughout the delegate subtree.

## Investigation Targets (human verification needed)

### I-C1 — Integer setters in `SettingsEditModel` have no range/positivity validation (65/100)
- **Location**: `SettingsEditModel.cpp:135-178` (`setUiFontSize`, `setFixedFontSize`, `setWorkspaceCount`, `setTrayMaxItems`)
- **Finding**: None of these setters clamp or reject out-of-range values before storing and eventually writing to disk. Currently masked entirely by QML `Slider` bounds — the gap is only reachable via non-UI paths (tests, a future direct binding).
- **How to verify**: Call these setters directly (e.g. from a test) with negative/huge values and confirm they reach `ConfigWriter::write()` unclamped; if so, mirror `libs/holonight-config`'s own `clampRange()`/positivity helpers inline in each setter.

### I-C2 — `setFromParsedConfig` bypasses the class's own guarded-recompute convention (65/100)
- **Location**: `SettingsEditModel.cpp:7-25`
- **Finding**: Unlike every other mutator, this method wraps a no-op `blockSignals(true)/(false)` pair (meaningless for plain struct assignment), unconditionally emits all nine `*Changed()` signals regardless of actual diff, and force-sets `is_dirty_ = false` directly instead of calling `recomputeDirty()`. Currently safe only because its sole caller (`ConfigFileService::load()`) always immediately follows it with `setThemeAppearanceSnapshot()` in the same call stack.
- **How to verify**: Confirm no other/future caller of `setFromParsedConfig` skips the paired theme-snapshot sync; if a defensive fix is wanted, replace the forced `is_dirty_ = false` with a `recomputeDirty()` call.

### I-C3 — `SettingsEditModel::markSaved(ParsedConfig, QString)` 2-arg overload is dead in production (75/100)
- **Location**: `SettingsEditModel.h:54`, `SettingsEditModel.cpp:61-66`
- **Finding**: Never called from production code — `ConfigFileService::save()` always uses the 3-arg overload. The only caller anywhere in the repo is `tests/test_settings_app.cpp:59`, and the 2-arg overload's own `Appearance.mode` field is discarded once forwarded, making it doubly unused.
- **How to verify**: Confirm via repo-wide grep (already done — zero production callers); either delete the overload and have the test exercise the real 3-arg path, or wire an actual caller so test coverage doesn't diverge from production usage.

### I-C4 — Hardcoded `"cyan"` default-accent literal diverges from the catalog's actual default (`"default"`) (65/100)
- **Location**: `SettingsEditModel.h:77-78`
- **Finding**: `ThemeConfigFile::defaultAccent()` returns `"default"` per the external theme catalog, but the in-class member initializers hardcode `"cyan"` — a different, non-default catalog option. Masked today because `SettingsApplication`'s constructor always calls `load()` before QML binds, overwriting the stale literal.
- **How to verify**: Any future direct-construction path (unit test, alternate entry point) reading `themeAccent()` before the first `load()` would observe `"cyan"` instead of the documented default — replace the literal with a call to `ThemeConfigFile::defaultAccent()`.

### I-C5 — `FontListModel::rebuild()` re-scans the full system font list on every `fixedPitchOnly` toggle (60/100)
- **Location**: `FontListModel.cpp:16-30`
- **Finding**: Calls `QFontDatabase::families()` fresh every time instead of caching the unfiltered list once and filtering locally. Likely low-impact since Qt caches its own internal font database, but is a code-clarity gap.
- **How to verify**: Profile `setFixedPitchOnly` toggle latency; if measurable, cache `all_families_` once at construction and filter from that cache in `rebuild()`.

### I-Q1 — `pragma ComponentBehavior: Bound` missing on files with delegates reaching outer-scope ids (70/100, corroborated by two agents)
- **Location**: `NavPanel.qml` (delegate reads `root.currentPage`), `AppearancePage.qml` (nested delegates reach `editModel.themeScheme`/`themeAccent` and, in the family/variant case, `familyDelegate.modelData.variants`)
- **Finding**: Neither file opts into `Bound` mode despite mixing `required property` delegates with outer-scope id access — qmllint cannot statically verify these bindings without it.
- **How to verify**: Add the pragma to both files and run `task qml-lint`; fix any newly-surfaced violations.

### I-Q2 — Duplicate, independently-set "current page" state between `NavPanel` and `ContentStack` (68/100)
- **Location**: `NavPanel.qml:11`, `ContentStack.qml:6`, wired via `SettingsWindow.qml:31`
- **Finding**: Page-selection state is duplicated across two sibling components with no binding tying them together — only kept in sync today because the sole mutation path is the nav's own click handler. A future independent mutation (keyboard shortcut, deep link) could desync the nav highlight from the displayed page.
- **How to verify**: Decide whether to make one of the two the single source of truth (likely `ContentStack.currentPage`, bound into `NavPanel` as a passed-in property) before adding any second page-navigation trigger.

### I-Q3 — "Apply" and "Save & Apply" footer buttons are behaviorally identical and neither is gated on `isDirty` (62/100)
- **Location**: `FooterBar.qml:94-106`
- **Finding**: Both buttons bind `enabled: !fileService.isSaving` and both call `fileService.save()` — no functional difference, and unlike "Discard Changes" (correctly gated on `isDirty`), a user can trigger a no-op save with nothing changed.
- **How to verify**: Confirm whether this is an intentional stub for a not-yet-differentiated flow; if not, differentiate the two buttons or collapse them into one, and gate on `isDirty` for consistency.

### I-Q4 — `HolonightTheme` singleton (once registered per D-C2) is used directly as a view data source, bypassing `editModel` (62/100)
- **Location**: `AppearancePage.qml:58,121`
- **Finding**: Every other read/write in this file goes through `editModel`, but the theme/accent option lists reach directly into the `HolonightTheme` singleton — harder to unit-test in isolation than if the lists were exposed through `editModel`.
- **How to verify**: If page-level test isolation matters for the QML test harness, consider plumbing `themeFamilies`/`accentOptionsForScheme` through `editModel` instead.

### I-Q5 — Repeated per-delegate `Qt.rgba` recomputation in `NavPanel` on every nav click (74/100)
- **Location**: `NavPanel.qml:43-47`
- **Finding**: All 13 nav-item delegates recompute two `Qt.rgba(...)` calls whenever `root.currentPage` changes (i.e., every click), even though both results are pure functions of the palette and a fixed alpha, not of `modelData`.
- **How to verify**: Hoist to `readonly property color selectedBg/unselectedBg` on root; confirm no visual regression.

### I-Q6 — `Text` elements across the app lack explicit `textFormat: Text.PlainText` (64/100)
- **Location**: ~20 `Text` elements across all 7 QML files, notably the frequently-updating slider readouts in `AppearancePage.qml`/`BarPage.qml`
- **Finding**: Default `Text.AutoText` runs a markup-detection scan on every text change; all observed content is plain literals/interpolations with no intended markup.
- **How to verify**: Add `textFormat: Text.PlainText` to the frequently-changing ones first (slider readouts, footer status/version) and confirm no regression.

## Ruled Out (cross-agent contradiction resolved via project memory)

- **"ColumnLayout margin" findings in `NavPanel.qml`, `FooterBar.qml`, `AppearancePage.qml`, `BarPage.qml`** — INITIALLY reported by the Layout & Anchoring agent as 4 confirmed bugs (88-92/100 confidence), directly contradicted by the Bindings & Properties agent's independent read of the same `AppearancePage.qml`/`BarPage.qml` code ("correctly follow the documented pattern"). Resolved by reading the actual project memory file (`project_holonight_settings.md`) directly rather than relying on this reviewer's own paraphrase used to brief the agents: the memory states verbatim — *"Fix: wrap in `Item { width: root.width; implicitHeight: content.implicitHeight + 48 }` containing the `ColumnLayout` with `anchors.topMargin: 24; anchors.leftMargin: 24; anchors.rightMargin: 24`. Both AppearancePage.qml and BarPage.qml use this pattern."* — i.e., margins **do** belong directly on the `ColumnLayout` itself; the wrapping `Item` exists to break a `Flickable`-content-sizing circular dependency (computing `implicitHeight` explicitly), **not** because `anchors.margins` fails to apply to a `ColumnLayout`. Both `AppearancePage.qml:34-47` and `BarPage.qml:12-25` match this documented pattern exactly. **`NavPanel.qml:29-32` and `FooterBar.qml:44-48`** use a simpler variant (`anchors.fill: parent` + per-edge margins directly on the Layout, no wrapping `Item`) — since neither is inside a `Flickable` needing content-height computation, and `anchors.*Margin` is a standard QML `Anchors`-system property that works identically on any `Item` subtype regardless of Qt version, there is no basis to expect this to render incorrectly. **No findings recorded for any of the four files.** This reviewer's own imprecise briefing to the agent (describing the gotcha backwards — as "margins broken directly on the layout, fixed by moving them to the wrapping Item") caused this false-positive cluster; corrected here before it entered the report.
- **Multi-cycle binding loops, `Qt.binding()` closures, alias-to-alias chains [QML]** — none found anywhere in the 7-file scope.
- **Deprecated `Connections` handler syntax, `Qt5Compat.GraphicalEffects` imports, `Binding.restoreMode` issues [QML]** — none found; all `Connections` blocks use modern `function onFoo()` syntax; no legacy-effects imports anywhere.
- **`Component.createObject()`/dynamic object creation, `Loader` `source`/`sourceComponent` mixing [QML]** — zero occurrences in scope; `WidgetSurface`-style Loader dispatch doesn't exist here (`ContentStack`'s Loader uses only `source:`).
- **C++/QML object-ownership boundary [C++]** — `SettingsApplication` owns all service objects as `unique_ptr` members in a declaration/destruction order that guarantees `QQuickView` (holding context-property references) is destroyed first; no dangling, no manual `new`/`delete` anywhere in scope.
- **Thread safety [C++]** — the entire app is confirmed single-threaded; no `QThread`/`QtConcurrent`/`moveToThread`/`QFuture` anywhere. Both `FontListModel`'s font scan and `ShellStatusService`'s `/proc` walk run synchronously on the main thread — a potential main-thread-blocking *performance* concern (flagged as context for I-C5), not a thread-safety defect.
- **Hardcoded hex colors [QML]** — grepped all 7 files for `#[0-9a-fA-F]{3,8}`; zero matches. All color usage goes through `HoloniightPalette.*` tokens.
- **ComboBox `parent.text`/`parent.highlighted` delegate-override regression [QML]** — checked specifically per project memory's documented prior bug; not present here, since both ComboBoxes in scope use the stock `QtQuick.Controls.Basic` delegate with no custom override (see D-C3 for the *different*, real binding-lifecycle issue these same controls have).
- **`reuseItems: true` / pooled-delegate handling [QML]** — not present anywhere in scope; no `ListView` exists at all in this unit (all lists are `Repeater` over small bounded JS arrays).

## Summary

| Category | Lint | Deep (Confirmed) | Investigate | Total |
|----------|------|-------------------|-------------|-------|
| C++ — Model Contracts | 0 | 0 | 0 | 0 |
| C++ — Ownership & Lifecycle | 0 | 0 | 0 | 0 |
| C++ — Thread Safety | 0 | 0 | 0 | 0 |
| C++ — API & Correctness | 0 | 1 | 2 | 3 |
| C++ — Error Handling | 0 | 2 | 1 | 3 |
| C++ — Performance & Quality | 14 (VAR-3, style) | 1 | 2 | 3 |
| QML — Bindings & Properties | 4 (BND-1) | 1 | 0 | 1 |
| QML — Layout & Anchoring | 0 | 0 | 0 | 0 |
| QML — Component Loading | 0 | 1 | 1 | 2 |
| QML — ListView & Delegates | 0 | 1 (merged) | 0 | 1 |
| QML — States & Structure | 0 | 0 | 2 | 2 |
| QML — Performance & Quality | 11 (PRF/STY, style) | 2 | 4 | 6 |
| **Total** | **29** | **8** | **12** | **49** |

(C++ 14 VAR-3 style hits and QML 17 ORD-1/STY-3/STY-1 style-noise hits excluded from per-category deep/investigate counts per established convention; two cross-agent-corroborated findings — D-C4/theme-mapping-duplication and D-C3/D-Q2 delegate-fragility patterns — are counted once each above, not double-counted per agent. One four-finding false-positive cluster (ColumnLayout margins) was caught and excluded before entering this report; see Ruled Out.)

Findings below confidence 60 are suppressed entirely. Two High-severity confirmed findings (D-C1 silent weather-location data loss, D-C2 unregistered HolonightTheme singleton) warrant prompt follow-up given this is a POC-readiness review — both are silent, user-facing defects with no error indication.

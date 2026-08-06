# Phase 43 — Settings Application Cleanup: Design

## Model State Loading

`SettingsEditModel::setFromParsedConfig()` retains the previous parsed value,
installs the new current and snapshot values, and compares the six parsed fields
that have QML notifications. It emits only the affected signals, then calls
`recomputeDirty()`. Theme signals remain owned by the separate theme-appearance
snapshot path because theme state no longer lives in `ParsedConfig`.

The dead string-mode `markSaved()` adapter is removed. Production and tests use
the existing `ThemeConfigFile::Appearance` overload, keeping one snapshot API.

## Font Family Snapshot

`FontListModel` captures `QFontDatabase::families()` once during construction.
`rebuild()` either copies that snapshot or filters it with
`QFontDatabase::isFixedPitch()`. This preserves ordering and model-reset
behavior while avoiding a repeated full family enumeration on toggles.

## QML Contracts and Page State

`SettingsWindow.currentPage` is the single page-selection owner. `NavPanel` and
`ContentStack` require the value, and navigation requests update the window
property. Both consumers therefore update from the same binding.

`NavPanel.qml` and `AppearancePage.qml` opt into bound component behavior.
Delegate roots receive ids so child bindings and click handlers reference typed
delegate properties instead of dynamic `parent` lookup. Palette-derived
selected and transparent navigation colors are cached once on the panel root.

Every atomic `Text` item under `apps/settings/qml` declares
`textFormat: Text.PlainText`; wrapped error text retains its existing wrapping.

## Verification Strategy

- Add a signal-spy regression test for identical loads and dirty-state restore.
- Exercise fixed-pitch filtering and restoration against the cached family set.
- Build the settings executable so QML cache generation validates required
  properties and bound delegates.
- Run `task qml-lint`, `task qmltypes-check`, focused settings tests, then
  `task test` and `task architecture-check`.
- Manually navigate Appearance, Bar, and placeholder pages and exercise edit,
  discard, and save behavior before acceptance.

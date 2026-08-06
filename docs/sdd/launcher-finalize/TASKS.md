# SDD Tasks — launcher-finalize

- [x] T-001: LauncherSurface keep-alive lifecycle (C++)
  - REQs: REQ-F-001, REQ-F-004
  - Check: `show()` and `hide()` toggle only the QML root `visible` property after initialization; `destroySurface()` is never called during toggle and only runs in the destructor.

- [x] T-002: LauncherSurface reset and focus on open
  - REQs: REQ-F-002, REQ-F-003
  - Check: After opening the launcher, the search query is empty and the search field has input focus, ready to receive keystrokes.

- [x] T-003: Launcher.qml — add `resetAndFocus()` function
  - REQs: REQ-F-002, REQ-F-003
  - Check: `resetAndFocus()` function exists on the root item and clears the query, calls `root.forceActiveFocus()` and `searchField.forceInputFocus()` when invoked from C++.

- [x] T-004: Launcher.qml — open animation
  - REQs: REQ-F-006, REQ-F-007
  - Check: When `visible` becomes `true`, the panel animates smoothly from scale 0.95/opacity 0.0 to 1.0/1.0 over 150ms using `Easing.OutCubic`.

- [x] T-005: Launcher.qml — close animation and notifyHideReady
  - REQs: REQ-F-005, REQ-F-008
  - Check: Pressing Esc, clicking outside, or launching an app triggers a smooth scale-and-fade close animation over 150ms; the surface becomes invisible only after animation completes.

- [x] T-006: LauncherResultRow.qml — best-match row styling
  - REQs: REQ-F-009, REQ-F-010, REQ-F-011
  - Check: First result row (index 0 with non-empty query) is 72px tall with accent-violet app name text; subsequent rows are 64px with default onSurface text.

- [x] T-007: Launcher.qml — wire isBestMatch into delegate
  - REQs: REQ-F-009, REQ-F-010, REQ-F-011
  - Check: The ListView delegate passes `isBestMatch: index === 0 && LauncherService.query.length > 0` to LauncherResultRow.

- [x] T-008: LauncherSearchField.qml — clear button polish
  - REQs: REQ-F-012, REQ-F-013
  - Check: Clear button displays "×" (U+00D7) glyph at 20px size; on hover it shows onSurface color (bright); at rest it shows textSubtle color (dim).

- [x] T-009: Build and qmllint
  - REQs: REQ-NF-001
  - Check: `task build` completes without errors and `task qml-lint` reports no QML validation issues in launcher components.

- [x] T-010: Manual smoke test
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-005, REQ-F-006, REQ-F-008, REQ-F-012, REQ-F-025, REQ-F-026, REQ-F-027
  - Check: Open launcher with open animation visible; search for "fire", verify best-match row is taller and violet; press Esc and verify close animation; reopen and confirm query is cleared; click clear button and verify it shows "×"; test keyboard focus by opening and typing without click; test arrow key selection, Enter to launch, Esc to close, click outside to close.

# SDD Tasks — active-window-icon

- [x] T-001: Add Qt6::Concurrent to CMakeLists.txt dependencies
  - REQs: REQ-NF-001
  - Check: `find_package(Qt6 ... Concurrent)` and `target_link_libraries` both include `Qt6::Concurrent`.

- [x] T-002: Add category Q_PROPERTY and categoryChanged signal to ActiveWindowService.h
  - REQs: REQ-F-005
  - Check: `Q_PROPERTY(QString category READ category NOTIFY categoryChanged)` is declared and `[[nodiscard]] QString category() const` returns `category_` member.

- [x] T-003: Add private members (category_, category_cache_, resolved_classes_) to ActiveWindowService.h
  - REQs: REQ-NF-003, REQ-F-005
  - Check: Header declares `QString category_`, `QHash<QString, QString> category_cache_`, and `QSet<QString> resolved_classes_`.

- [x] T-004: Add private method declarations to ActiveWindowService.h
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003
  - Check: Header declares `void setCategory(const QString&)`, `void scheduleResolveCategory(const QString&)`, `static QString scanDesktopFiles(const QString&)`, and `static QString mapCategoriesToIcon(const QString&)`.

- [x] T-005: Implement mapCategoriesToIcon static helper in ActiveWindowService.cpp
  - REQs: REQ-F-003
  - Check: Function splits `Categories=` field on `;` and returns first token matching the priority table (WebBrowser→browser, TextEditor/Development→editor, TerminalEmulator→terminal, FileManager→files, InstantMessaging/Chat→chat, Audio/Music→music, Video→video, Settings/System→settings) or empty string.

- [x] T-006: Implement scanDesktopFiles static method in ActiveWindowService.cpp (Pass 1 exact match)
  - REQs: REQ-F-001, REQ-F-002, REQ-C-005
  - Check: Function tries `{appClass}.desktop` and `{appClass_lowercase}.desktop` in `~/.local/share/applications/` then `/usr/share/applications/`, reads Categories field on first hit, and calls mapCategoriesToIcon.

- [x] T-007: Implement scanDesktopFiles Pass 2 (name/exec scan) in ActiveWindowService.cpp
  - REQs: REQ-F-002, REQ-F-004, REQ-C-005
  - Check: If Pass 1 finds no file, iterates all `*.desktop` files in both directories, reads Name= and Exec= fields, matches case-insensitively against appClass, returns resolved category from matched file or empty string on miss.

- [x] T-008: Implement setCategory private method in ActiveWindowService.cpp
  - REQs: REQ-F-005
  - Check: Method guards against duplicate signal emit (only emits if value differs from category_) and calls categoryChanged().

- [x] T-009: Implement scheduleResolveCategory in ActiveWindowService.cpp with QtConcurrent dispatch
  - REQs: REQ-F-001, REQ-F-006, REQ-NF-001, REQ-NF-003
  - Check: If appClass is empty, sets category to empty; if appClass is in resolved_classes_, applies cached value immediately; otherwise dispatches scanDesktopFiles to background thread via QtConcurrent::run and applies result via QFutureWatcher, guarding with `app_class_ == app_class` check before setCategory.

- [x] T-010: Call scheduleResolveCategory from setAppClass in ActiveWindowService.cpp
  - REQs: REQ-F-001, REQ-F-006
  - Check: setAppClass calls scheduleResolveCategory(value) before emitting appClassChanged, ensuring category is always in sync with appClass.

- [x] T-011: Create AppWindowIcon.qml scaffold with Canvas, category property, and repaint triggers
  - REQs: REQ-F-007, REQ-F-008, REQ-F-020
  - Check: Component at `src/qml/Topbar/AppWindowIcon.qml` has 16×16 size, `readonly property color _stroke: HoloniightPalette.onSurface`, Canvas with `opacity: 0.9`, and `onCategoryChanged` and `on_StrokeChanged` handlers that call `iconCanvas.requestPaint()`.

- [x] T-012: Implement onPaint dispatch logic in AppWindowIcon.qml (category branching)
  - REQs: REQ-F-008, REQ-F-017
  - Check: Canvas onPaint resets context, sets `strokeStyle = root._stroke`, `lineCap = "round"`, `lineJoin = "round"`, then dispatches to drawBrowser, drawEditor, drawTerminal, drawFiles, drawChat, drawMusic, drawVideo, drawSettings, or drawWindow based on category.

- [x] T-013: Implement drawBrowser in AppWindowIcon.qml
  - REQs: REQ-F-009
  - Check: Function draws a circle outline with one vertical and two horizontal ellipse arcs to suggest a globe, at 16×16 logical scale.

- [x] T-014: Implement drawEditor in AppWindowIcon.qml
  - REQs: REQ-F-010
  - Check: Function draws two opposing angle-bracket glyphs `< >` at 16×16 logical scale.

- [x] T-015: Implement drawTerminal in AppWindowIcon.qml
  - REQs: REQ-F-011
  - Check: Function draws a `>` chevron and a short `_` horizontal line (prompt `>_` glyph) at 16×16 logical scale.

- [x] T-016: Implement drawFiles in AppWindowIcon.qml
  - REQs: REQ-F-012
  - Check: Function draws an outlined folder shape (rectangular body with small tab on top-left) at 16×16 logical scale.

- [x] T-017: Implement drawChat in AppWindowIcon.qml
  - REQs: REQ-F-013
  - Check: Function draws a rounded speech bubble (rounded rectangle with small triangular tail at bottom-left) at 16×16 logical scale.

- [x] T-018: Implement drawMusic in AppWindowIcon.qml
  - REQs: REQ-F-014
  - Check: Function draws three vertical bars of varying height to suggest a waveform at 16×16 logical scale.

- [x] T-019: Implement drawVideo in AppWindowIcon.qml
  - REQs: REQ-F-015
  - Check: Function draws a rounded rectangle frame containing a right-pointing triangle (play-frame) at 16×16 logical scale.

- [x] T-020: Implement drawSettings in AppWindowIcon.qml
  - REQs: REQ-F-016
  - Check: Function draws a small circle surrounded by evenly-spaced rectangular teeth (cog) at 16×16 logical scale.

- [x] T-021: Implement drawWindow in AppWindowIcon.qml
  - REQs: REQ-F-017
  - Check: Function draws a rounded rectangle with horizontal divider line near top (matching BarIcon's "window" shape) at 16×16 logical scale, used when category is empty or unrecognized.

- [x] T-022: Register AppWindowIcon.qml in CMakeLists.txt with QT_RESOURCE_ALIAS
  - REQs: REQ-C-001
  - Check: CMakeLists.txt contains `set_source_files_properties(src/qml/Topbar/AppWindowIcon.qml PROPERTIES QT_RESOURCE_ALIAS "Topbar/AppWindowIcon.qml")` adjacent to ActiveWindowSection entry.

- [x] T-023: Add AppWindowIcon.qml to qt6_add_qml_module QML_FILES in CMakeLists.txt
  - REQs: REQ-C-001
  - Check: `src/qml/Topbar/AppWindowIcon.qml` is listed in the QML_FILES block, adjacent to ActiveWindowSection.qml entry.

- [x] T-024: Update ActiveWindowSection.qml to wrap title in Row with AppWindowIcon
  - REQs: REQ-F-018, REQ-F-019, REQ-F-020
  - Check: Title Label is wrapped in a Row with `spacing: 6`, AppWindowIcon is first child bound to `ActiveWindowService.category`, and both are inside the existing `visible: ActiveWindowService.title !== ""` guard (no separate visible binding needed on icon).

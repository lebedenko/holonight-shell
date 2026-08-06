# SDD Tasks — mime-desktop-integration

- [x] T-001: Add `mime_types` field to `DesktopEntry` struct
  - REQs: REQ-F-001, REQ-C-004
  - Check: `DesktopEntry` struct in `src/services/launcher/DesktopEntryScanner.h` contains `QStringList mime_types{}` field after `QVector<DesktopAction> actions`.

- [x] T-002: Extend `DesktopEntrySerializer::toJson()` to serialize `mime_types`
  - REQs: REQ-F-001, REQ-NF-003
  - Check: `DesktopEntrySerializer::toJson()` creates a `QJsonArray mime_types_array` and assigns it to `obj[QStringLiteral("mime_types")]` after the actions block.

- [x] T-003: Extend `DesktopEntrySerializer::fromJson()` to deserialize `mime_types`
  - REQs: REQ-F-001, REQ-NF-003
  - Check: `DesktopEntrySerializer::fromJson()` reads the `"mime_types"` array and populates `entry.mime_types`, skipping empty parts (trimmed strings that become empty are discarded).

- [x] T-004: Extend `DesktopEntryScanner::applyDesktopEntryField()` to parse `MimeType=` lines
  - REQs: REQ-F-002, REQ-NF-003
  - Check: Parsing a `.desktop` file with `MimeType=text/html;x-scheme-handler/https;` yields `DesktopEntry` with `mime_types == ["text/html", "x-scheme-handler/https"]` (trailing semicolon and empty parts discarded).

- [x] T-005: Add `MimeTypesRole` to `LauncherModel::Role` enum
  - REQs: REQ-F-016
  - Check: `LauncherModel::Role` enum contains `MimeTypesRole` after `MappedCategoryRole`.

- [x] T-006: Implement `MimeTypesRole` in `LauncherModel::data()` and `roleNames()`
  - REQs: REQ-F-016
  - Check: `LauncherModel::data()` returns a `QVariantList` of MIME types for `MimeTypesRole`; `roleNames()` includes `"mimeTypes"` mapped to `MimeTypesRole`.

- [x] T-007: Add `entriesForMimeTypes(QStringList)` Q_INVOKABLE to `LauncherService`
  - REQs: REQ-F-016, REQ-NF-005
  - Check: `LauncherService` declares `Q_INVOKABLE QVariantList entriesForMimeTypes(const QStringList& mime_types)` and iterates `model_.allEntryAt()` to return entries matching any MIME in the input list (no subprocess or I/O).

- [x] T-008: Implement `MimeService` header file
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009, REQ-NF-001, REQ-NF-002
  - Check: `src/services/mime/MimeService.h` declares `MimeService` as a `QML_SINGLETON` with six Q_PROPERTY getters (`defaultBrowser`, `defaultTerminal`, `defaultFileManager`, `defaultImageViewer`, `defaultTextEditor`, `defaultVideoPlayer`), six Q_INVOKABLE setters, and six NOTIFY signals; includes abstract `IMimeResolver` interface with `queryDefault()` / `setDefault()` methods for MIME roles and `queryDefaultBrowser()` / `setDefaultBrowser()` methods for the browser role.

- [x] T-009: Implement `MimeService` body with subprocess and caching logic
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009, REQ-NF-001, REQ-NF-002, REQ-NF-004
  - Check: `src/services/mime/MimeService.cpp` implements `queryDefault()` and `setDefault()` via `QProcess` to spawn `xdg-mime` subprocesses with 5-second kill-guard timers for non-browser roles; implements browser get/check/set via `xdg-settings default-web-browser`; caches MIME results in `QHash<QString, QString> mime_cache_` and the browser result in `browser_default_`; implements `resolveRole()` to return the first non-empty cached MIME in a non-browser role's list; emits NOTIFY signals when role values change; includes `NullMimeResolver` test seam with configurable canned answers.

- [x] T-010: Implement `KdeCompatService` header file
  - REQs: REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-013, REQ-F-014, REQ-C-002
  - Check: `src/services/kde-compat/KdeCompatService.h` declares `KdeCompatService` as a `QML_SINGLETON` with two Q_PROPERTYs (`kdeWarningActive`, `rebuildInProgress`) and Q_INVOKABLEs `recheckDiagnostics()` and `rebuildCaches()`; includes `warningEmitted()` signal.

- [x] T-011: Implement `KdeCompatService` body with diagnostic and rebuild logic
  - REQs: REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-013, REQ-F-014, REQ-C-002
  - Check: `src/services/kde-compat/KdeCompatService.cpp` includes `KdeCompatService.moc`; detects `kbuildsycoca6` presence via `QStandardPaths::findExecutable()`; `recheckDiagnostics()` reads `XDG_MENU_PREFIX` via `qgetenv()` and emits `warningEmitted()` only if both conditions hold (no env mutation); `rebuildCaches()` spawns `update-desktop-database` then `kbuildsycoca6 --noincremental` sequentially, calling `recheckDiagnostics()` upon completion.

- [x] T-012: Wire `MimeService` and `KdeCompatService` into CMakeLists.txt
  - REQs: REQ-F-003, REQ-F-010
  - Check: `CMakeLists.txt` adds `src/services/mime/MimeService.h` and `src/services/mime/MimeService.cpp` to `holonight_services` target; adds `src/services/kde-compat/KdeCompatService.h` and `src/services/kde-compat/KdeCompatService.cpp` to `holonight_services` target; adds `src/services/mime` and `src/services/kde-compat` to target include directories; registers both classes as QML singletons in the module registration.

- [x] T-013: Implement `DefaultAppRow.qml` component
  - REQs: REQ-F-016, REQ-F-017, REQ-NF-005
  - Check: `src/qml/RightSidebar/DefaultAppRow.qml` defines an `Item` with `required property string label`, `required property var mimeTypesForFilter`, and `required property string currentDefault`; populates `candidates` in `Component.onCompleted` via `LauncherService.entriesForMimeTypes()`; displays a `RowLayout` with a label and a `Controls.ComboBox` bound to candidates; emits `defaultChanged(desktopFile)` signal on selection change.

- [x] T-014: Implement `SidebarSystem.qml` with role selectors and KDE diagnostic section
  - REQs: REQ-F-015, REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-019
  - Check: `src/qml/RightSidebar/SidebarSystem.qml` replaces the stub with an `Item` root containing a `ColumnLayout` with: a "Default Applications" header, six `DefaultAppRow` instances (browser, terminal, file-manager, image-viewer, text-editor, video-player) bound to `MimeService` properties, a `ContentSeparator` and inline `KdeCompatRow` (warning icon + text + "Rebuild caches" button) visible only when `KdeCompatService.kdeWarningActive` is true; uses `HoloniightPalette` tokens for all colors.

- [x] T-015: Add `DefaultAppRow.qml` to CMakeLists.txt QML files list
  - REQs: REQ-F-016
  - Check: `CMakeLists.txt` includes `src/qml/RightSidebar/DefaultAppRow.qml` in the `HOLONIGHT_QML_FILES` list.

- [x] T-016: Build verification — configure, build, and run qmltypes check
  - REQs: REQ-F-001 through REQ-F-019, REQ-NF-001 through REQ-NF-005, REQ-C-001 through REQ-C-007
  - Check: `task build` completes without errors; `task qmltypes-check` confirms that the generated `HolonightShell.qmltypes` includes both `MimeService` and `KdeCompatService` with their full Q_PROPERTY and Q_INVOKABLE signatures.

# SPEC — mime-desktop-integration

## 1. Purpose

Enable users to view and configure system default MIME type handlers for six common application roles (browser, terminal, file manager, image viewer, text editor, video player) via a dedicated UI section in the sidebar. Provide diagnostic tooling for KDE environment compatibility.

## 2. Scope & Non-Goals

### Scope

- Extend `DesktopEntry` struct with MIME type information parsed from `.desktop` files.
- Implement `MimeService` QML singleton to query and set system defaults via `xdg-mime`, with the
  browser role using `xdg-settings default-web-browser` for compatibility with browser self-checks.
- Implement `KdeCompatService` QML singleton to detect KDE environment incompleteness.
- Populate a UI section in `SidebarSystem.qml` with role-based MIME handler selectors.
- Support six preset roles with hardcoded MIME type mappings (no per-MIME customization).
- Provide on-demand rebuild action for KDE application caches.

### Non-Goals

- Full per-MIME association editor (role-based presets only).
- Automatic fixing of environment variables (diagnostic + manual rebuild only).
- Portal backend implementation.
- `DesktopEntryService` filesystem watching (delegated to pipeline #7 launcher-fs-watch).
- Support for custom MIME roles beyond the hardcoded six.

## 3. Functional Requirements

### 3.1 DesktopEntry Extension

**REQ-F-001** (Ubiquitous)  
The `DesktopEntry` struct shall include a `mimeTypes: QStringList` field to store MIME types declared in the source `.desktop` file.

*Acceptance Criterion*: A populated `DesktopEntry` object deserialized from a `.desktop` file declaring `MimeType=text/html;text/plain;` has `mimeTypes = ["text/html", "text/plain"]`.

---

**REQ-F-002** (Ubiquitous)  
The `DesktopEntryScanner` shall parse the `MimeType=` line from `.desktop` files and populate the `mimeTypes` field as a semicolon-delimited list.

*Acceptance Criterion*: Parsing a `.desktop` file with `MimeType=text/html;x-scheme-handler/https;` correctly yields `["text/html", "x-scheme-handler/https"]`; both forms `text/html` and `text/html;` (with trailing semicolon) are accepted.

---

### 3.2 MimeService (QML Singleton)

**REQ-F-003** (Ubiquitous)  
The `MimeService` QML singleton shall provide read access to the system default MIME handler for a given MIME type via `xdg-mime query default <mime>`.

*Acceptance Criterion*: Calling `MimeService.queryDefault("text/html")` returns a desktop filename (e.g., `firefox.desktop`), or an empty string if no default is set.

---

**REQ-F-004** (Ubiquitous)  
The `MimeService` QML singleton shall provide write access to set the system default MIME handler for a given MIME type via `xdg-mime default <desktop-file> <mime>`.

*Acceptance Criterion*: Calling `MimeService.setDefault("firefox.desktop", "text/html")` executes the subprocess and, on success, subsequent `queryDefault("text/html")` returns `"firefox.desktop"`.

---

**REQ-F-005** (Ubiquitous)  
The `MimeService` shall expose role-based accessor methods: `getDefaultBrowser()`, `getDefaultTerminal()`, `getDefaultFileManager()`, `getDefaultImageViewer()`, `getDefaultTextEditor()`, `getDefaultVideoPlayer()`.

*Acceptance Criterion*: Each method returns a desktop filename string (e.g., `"firefox.desktop"`) or empty string; non-browser roles internally query their representative MIME types in order and return the first non-empty result. The browser role returns the result of `xdg-settings get default-web-browser` only when `xdg-settings check default-web-browser <desktop-file>` returns `yes`; otherwise it returns an empty string.

---

**REQ-F-006** (Ubiquitous)  
The `MimeService` shall expose role-based setter methods: `setDefaultBrowser(desktopFile)`, `setDefaultTerminal(desktopFile)`, `setDefaultFileManager(desktopFile)`, `setDefaultImageViewer(desktopFile)`, `setDefaultTextEditor(desktopFile)`, `setDefaultVideoPlayer(desktopFile)`.

*Acceptance Criterion*: The browser setter applies `xdg-settings set default-web-browser <desktop-file>`. Each non-browser setter applies `xdg-mime default` to all MIME types in that role's hardcoded mapping.

---

**REQ-F-007** (Ubiquitous)  
The `MimeService` shall define the following hardcoded MIME type mappings per role:

- **browser**: `text/html`, `x-scheme-handler/http`, `x-scheme-handler/https`
- **terminal**: `application/x-terminal-emulator`
- **file-manager**: `inode/directory`
- **image-viewer**: `image/jpeg`, `image/png`, `image/gif`, `image/webp`
- **text-editor**: `text/plain`
- **video-player**: `video/mp4`, `video/x-matroska`, `video/webm`

*Acceptance Criterion*: The role definitions are visible in the C++ implementation; querying a role returns defaults for all listed MIME types.

---

**REQ-F-008** (Event-driven)  
When `MimeService.setDefault(desktopFile, mimeType)` completes successfully, the service shall invalidate its cache for that MIME type and re-query on the next `queryDefault()` call.

*Acceptance Criterion*: Setting a default, querying immediately, and verifying the new value is returned (not the stale cached value).

---

**REQ-F-009** (Ubiquitous)  
The `MimeService` shall cache query results at startup; all six role queries shall execute asynchronously (non-blocking to the QML thread).

*Acceptance Criterion*: `MimeService` initialization begins asynchronous default queries for all six roles; the browser role uses `xdg-settings`, and non-browser roles use concurrent `xdg-mime query default` subprocesses. QML updates reflect results when each completes, without UI freezing.

---

### 3.3 KdeCompatService (QML Singleton)

**REQ-F-010** (Ubiquitous)  
The `KdeCompatService` QML singleton shall perform a one-time diagnostic check at startup: if `kbuildsycoca6` is present in PATH and `XDG_MENU_PREFIX` environment variable is unset or empty, the service shall emit a warning signal.

*Acceptance Criterion*: On a KDE system missing `XDG_MENU_PREFIX`, `KdeCompatService` emits `warningEmitted()` signal during initialization; on non-KDE systems (no `kbuildsycoca6` in PATH), no signal is emitted.

---

**REQ-F-011** (Ubiquitous)  
The `KdeCompatService` shall expose a `recheckDiagnostics()` invokable method that re-runs the diagnostic check and re-emits the warning signal if the condition still holds.

*Acceptance Criterion*: Calling `recheckDiagnostics()` after manually setting `XDG_MENU_PREFIX` re-evaluates the condition and either emits the warning again or suppresses it.

---

**REQ-F-012** (Ubiquitous)  
The `KdeCompatService` shall be a graceful no-op on non-KDE systems and systems where `kbuildsycoca6` is absent from PATH.

*Acceptance Criterion*: On a system without `kbuildsycoca6`, `KdeCompatService` initializes without error, does not emit warnings, and `recheckDiagnostics()` returns silently.

---

**REQ-F-013** (Ubiquitous)  
The `KdeCompatService` shall expose an invokable method `rebuildCaches()` that executes the following subprocesses in order: first `update-desktop-database`, then `kbuildsycoca6 --noincremental`.

*Acceptance Criterion*: Calling `rebuildCaches()` spawns two subprocesses in sequence; if `kbuildsycoca6` is absent, the method returns silently without error.

---

**REQ-F-014** (Event-driven)  
When `KdeCompatService.rebuildCaches()` completes, the service shall call `recheckDiagnostics()` to re-evaluate the environment state.

*Acceptance Criterion*: After cache rebuild, the warning signal is re-emitted if `XDG_MENU_PREFIX` is still unset; no warning if the environment is now compliant.

---

### 3.4 SidebarSystem UI

**REQ-F-015** (Ubiquitous)  
The `SidebarSystem.qml` component shall display a list of six role-based MIME handler selectors (browser, terminal, file manager, image viewer, text editor, video player).

*Acceptance Criterion*: The sidebar system section renders six labeled comboboxes or similar selection controls, one per role.

---

**REQ-F-016** (Ubiquitous)  
Each role selector shall populate its list of available applications by filtering the global launcher model (`LauncherService`) to include only entries that declare at least one MIME type matching that role's hardcoded mapping.

*Acceptance Criterion*: The browser selector shows only applications with `mimeTypes` containing `text/html`, `x-scheme-handler/http`, or `x-scheme-handler/https`; terminal selector shows only applications with `application/x-terminal-emulator`, etc.

---

**REQ-F-017** (Ubiquitous)  
Each role selector shall display the currently-selected default application (fetched from `MimeService.<role>` getter at initialization) and allow the user to select a different application.

*Acceptance Criterion*: The browser selector initially shows the result of `MimeService.getDefaultBrowser()`; selecting a different app in the dropdown updates the system default via `MimeService.setDefaultBrowser()`.

---

**REQ-F-018** (Ubiquitous)  
The `SidebarSystem` section shall include a "Rebuild KDE caches" button that is visible only if `KdeCompatService` has emitted a warning signal.

*Acceptance Criterion*: On KDE systems with `XDG_MENU_PREFIX` unset, a "Rebuild caches" button appears; on other systems, it does not.

---

**REQ-F-019** (Event-driven)  
When the "Rebuild KDE caches" button is clicked, the sidebar shall call `KdeCompatService.rebuildCaches()`, which internally calls `recheckDiagnostics()` upon completion.

*Acceptance Criterion*: Clicking the rebuild button executes the cache rebuild action and updates the warning indicator based on the new environment state.

---

## 4. Non-Functional Requirements

**REQ-NF-001** (Ubiquitous)  
All `xdg-mime` and `xdg-settings` subprocess calls shall execute asynchronously via `QProcess` and shall not block the QML/UI thread.

*Acceptance Criterion*: Launching `MimeService.queryDefault()` does not freeze the sidebar or topbar; results are delivered via signal/property change once the subprocess exits.

---

**REQ-NF-002** (Ubiquitous)  
The `MimeService` shall cache query results; repeated calls to `getDefaultBrowser()` (or any role getter) without an intervening `setDefault()` call shall not spawn additional subprocesses.

*Acceptance Criterion*: Calling `getDefaultBrowser()` six times in succession results in only one `xdg-settings get/check default-web-browser` query sequence; subsequent calls return the cached value immediately.

---

**REQ-NF-003** (Ubiquitous)  
The `DesktopEntryScanner.parseDesktopFile()` shall extract the `MimeType=` line and split on semicolons, yielding a list of trimmed MIME type strings; empty entries (from double semicolons or trailing semicolons) shall be discarded.

*Acceptance Criterion*: Parsing `MimeType=text/html;;text/plain;` yields `["text/html", "text/plain"]` (no empty strings).

---

**REQ-NF-004** (Ubiquitous)  
Subprocess execution (for `xdg-mime`, `xdg-settings`, `kbuildsycoca6`, `update-desktop-database`) shall set a timeout to prevent indefinite hangs; if a subprocess does not complete within the timeout, the operation shall fail gracefully without crashing the shell.

*Acceptance Criterion*: A subprocess timeout logs a warning and returns an empty/error result; the shell remains responsive and no uncaught exception is raised.

---

**REQ-NF-005** (Ubiquitous)  
The combobox lists in `SidebarSystem.qml` shall be populated from the existing global launcher model without spawning additional `DesktopEntryScanner` threads; filtering shall be performed in QML or via a lightweight C++ adapter.

*Acceptance Criterion*: No new database queries or filesystem scans are triggered when populating role selectors; list updates occur immediately when the launcher model is ready.

---

## 5. Constraints

**REQ-C-001** (Ubiquitous)  
The `MimeService` shall call standard XDG utilities only: `xdg-mime` for MIME-backed roles and `xdg-settings` for the browser role. No custom MIME database querying or `/usr/share/applications` parsing shall be performed by this service.

*Acceptance Criterion*: Code review confirms MIME role queries and sets use `xdg-mime`, while browser default queries and sets use `xdg-settings default-web-browser` as the system interface.

---

**REQ-C-002** (Ubiquitous)  
The `KdeCompatService` shall perform no automatic environment variable modification (no `setenv()` calls); it shall only emit warnings and provide an on-demand rebuild action.

*Acceptance Criterion*: `KdeCompatService` implementation contains no code that modifies `XDG_MENU_PREFIX` or any other environment variable; diagnostics are read-only.

---

**REQ-C-003** (Ubiquitous)  
The six hardcoded roles and their MIME type mappings shall not be user-configurable; custom per-MIME role definitions are explicitly out of scope.

*Acceptance Criterion*: The role mappings are compiled into the code and cannot be modified via QML properties or runtime configuration files.

---

**REQ-C-004** (Ubiquitous)  
The `DesktopEntry` struct's `mimeTypes` field shall be read-only after deserialization; the `DesktopEntryScanner` is the sole source of truth for MIME type information.

*Acceptance Criterion*: The `mimeTypes` field is a `const QStringList` or equivalent immutable collection after the entry is constructed.

---

**REQ-C-005** (Ubiquitous)  
The sidebar system section shall consume the global launcher model (`LauncherService`) without modifying it; no entries shall be added, removed, or reordered by the MIME integration.

*Acceptance Criterion*: The launcher model remains unchanged before and after opening the sidebar system section; all filtering is applied in the presentation layer only.

---

**REQ-C-006** (Ubiquitous)  
Error handling for failed subprocess calls (e.g., `xdg-mime query default` or `xdg-settings check default-web-browser` returning non-zero exit code) shall log a warning but not propagate as an exception; the UI shall display a sensible fallback (empty selection or "not configured").

*Acceptance Criterion*: Setting a default on a system where `xdg-mime` is not installed logs an error, the selection updates to reflect the failure state, and the shell does not crash.

---

**REQ-C-007** (Ubiquitous)  
The `MimeService` shall use the `application/x-terminal-emulator` MIME type as the representative type for the terminal role, following XDG conventions established in wide use across Linux distributions.

*Acceptance Criterion*: The terminal role's hardcoded mapping includes only `application/x-terminal-emulator`; no other terminal-related MIME type is queried.

---

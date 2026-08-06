# launcher-fs-watch SPEC

## Scope

This feature enables the holonight-shell to automatically reflect changes in installed applications and MIME type associations in the Default Applications sidebar dropdowns, without requiring a shell restart.

Two synchronized file-watch pipelines work together:

1. **LauncherService signal pipeline**: After `LauncherService` completes a model refresh (initial load or filesystem-triggered re-scan), it emits a new `entriesUpdated` signal. `MimeService` subscribes to this signal to react to app installs/removals.

2. **MimeService watch pipeline**: `MimeService` maintains its own `QFileSystemWatcher` + 500ms debounce timer to watch MIME association files (`mimeapps.list`, `mimeinfo.cache`) across all XDG applications directories. When watched files change, `MimeService::refreshAllRoles()` is triggered to reload the current defaults for the 6 standard MIME roles (browser, terminal, file manager, image viewer, text editor, video player).

This allows the Default Applications QML rows (`DefaultAppRow.qml`) to reflect changes to both installed apps and their MIME defaults automatically and immediately.

---

## Functional Requirements

### REQ-F-001: LauncherService Emits entriesUpdated Signal

**EARS Sentence:** When `LauncherService` completes a model refresh (after `DesktopEntryScanner::scan()` returns and entries are validated), the service shall emit a new `entriesUpdated` signal.

**Acceptance Criterion:** After an initial launcher load and after a filesystem-triggered re-scan, the `entriesUpdated` signal is emitted exactly once per model refresh cycle. The signal fires regardless of whether entries actually changed (e.g., on rapid consecutive file events that debounce into one scan).

---

### REQ-F-002: MimeService Subscribes to LauncherService entriesUpdated

**EARS Sentence:** `MimeService` shall subscribe to `LauncherService::entriesUpdated` during construction and invoke `refreshAllRoles()` when the signal is emitted.

**Acceptance Criterion:** After `MimeService` is constructed, it establishes a signal-slot connection to `LauncherService::entriesUpdated`. When the signal fires, `MimeService::refreshAllRoles()` is called without user intervention. This connection persists for the lifetime of the service.

---

### REQ-F-003: MimeService Watches mimeapps.list Files

**EARS Sentence:** `MimeService` shall watch `~/.config/mimeapps.list` and `~/.local/share/applications/mimeapps.list` for file modifications, and on change (after debouncing), shall invoke `refreshAllRoles()`.

**Acceptance Criterion:** When `~/.config/mimeapps.list` or `~/.local/share/applications/mimeapps.list` is written to by any external tool (e.g., `xdg-mime default`, another application, a user-run script), the file modification triggers the debounce timer. After the timer expires (500ms with no further changes), `refreshAllRoles()` is called. The 6 MIME roles reflect the new defaults within 500ms.

---

### REQ-F-004: MimeService Watches mimeinfo.cache Files

**EARS Sentence:** `MimeService` shall watch `mimeinfo.cache` file(s) inside every XDG applications directory (as reported by `DesktopEntryScanner::applicationDirs()`), and on change (after debouncing), shall invoke `refreshAllRoles()`.

**Acceptance Criterion:** When `mimeinfo.cache` is updated in any watched XDG applications directory (e.g., after `update-desktop-database` runs), the file modification triggers the debounce timer. After the timer expires (500ms with no further changes), `refreshAllRoles()` is called. If a directory in the watch list is deleted at runtime, the watcher gracefully handles the removal without crashing.

---

### REQ-F-005: App Install Reflected in Dropdowns Within 500ms

**EARS Sentence:** When a new `.desktop` file is placed in a watched XDG applications directory, that application shall appear in relevant `DefaultAppRow` dropdown choices within approximately 500ms (the debounce interval), without a shell restart.

**Acceptance Criterion:** A test app desktop file is written to `~/.local/share/applications/` while the shell is running. Within 500ms, the app appears in dropdown choices for matching MIME roles (e.g., a terminal app shows in the terminal selector). Verification: inspect the dropdown via socket command (`toggle-launcher`) or visual inspection if a test Wayland session is running.

---

### REQ-F-006: App Removal Reflected in Dropdowns Within 500ms

**EARS Sentence:** When a `.desktop` file is removed from a watched XDG applications directory, that application shall disappear from relevant `DefaultAppRow` dropdown choices within approximately 500ms, without a shell restart.

**Acceptance Criterion:** A previously available app desktop file is deleted from `~/.local/share/applications/` while the shell is running. Within 500ms, the app no longer appears in dropdown choices for any MIME role. If the removed app was the currently selected default for a role, the dropdown updates to reflect the next available default or shows empty.

---

### REQ-F-007: MIME Default Change Reflected in Dropdowns Within 500ms

**EARS Sentence:** When an external tool (e.g., `xdg-mime default`) writes a new MIME type default association to `mimeapps.list`, the `DefaultAppRow` selector for that MIME role shall update within approximately 500ms to reflect the new default, without a shell restart.

**Acceptance Criterion:** The system calls `xdg-mime default firefox.desktop x-scheme-handler/https` (or similar) while the shell is running. Within 500ms debounce expiry, `MimeService::refreshAllRoles()` re-queries the defaults, and the browser `DefaultAppRow` selected value changes to Firefox. No user action or restart is required.

---

### REQ-F-008: Watcher Paths Derived from DesktopEntryScanner

**EARS Sentence:** `MimeService` shall initialize its file watcher with XDG applications directory paths obtained from `DesktopEntryScanner::applicationDirs()`, ensuring consistency with the set of directories scanned for desktop entries.

**Acceptance Criterion:** At `MimeService` construction, the set of watched `mimeinfo.cache` paths is derived directly from `DesktopEntryScanner::applicationDirs()`. If the scanner watches `/usr/share/applications/` and `~/.local/share/applications/`, then `mimeinfo.cache` in both locations is added to the watcher. No hardcoded paths (e.g., `/usr/share/applications` literal string) appear in `MimeService` constructor or initialization code.

---

### REQ-F-009: No Hardcoded File Paths in MimeService

**EARS Sentence:** `MimeService` shall not contain hardcoded file system paths; all watched directories and files shall be injected, derived from scanner state, or constructed from standard XDG environment variables at runtime.

**Acceptance Criterion:** A code review of `MimeService` constructor and watch initialization finds no string literals matching `/usr/share/applications`, `~/.local/share/applications`, `~/.config/mimeapps.list`, or `mimeinfo.cache` except where explicitly joined from injected or environment-derived base paths. Example acceptable pattern: `QString::fromStdString(std::getenv("HOME")) + "/.config/mimeapps.list"`.

---

### REQ-F-010: Debounce Timer Resets on Repeated File Changes

**EARS Sentence:** If a watched file is modified multiple times within the 500ms debounce window, the debounce timer shall reset with each change, and `refreshAllRoles()` shall be called only once after all changes have ceased for 500ms.

**Acceptance Criterion:** A test script writes to `~/.config/mimeapps.list` 5 times in rapid succession (e.g., 10ms apart). The debounce timer is reset on each write. `refreshAllRoles()` is called exactly once, approximately 500ms after the final write. If the writes are spaced >500ms apart, `refreshAllRoles()` is called multiple times (once per stable interval).

---

## Non-Functional Requirements

### REQ-NF-001: Debounce Interval is 500ms

**EARS Sentence:** The debounce timer used by `MimeService` for file watch events shall have a fixed interval of 500 milliseconds.

**Acceptance Criterion:** The debounce timer is initialized with a 500ms interval (e.g., `debounce_timer_.setInterval(500)`). This interval is consistent with `LauncherService`'s debounce behavior and documented in code comments or commit messages.

---

### REQ-NF-002: File Watcher Uses QFileSystemWatcher

**EARS Sentence:** `MimeService` shall use Qt's `QFileSystemWatcher` class for monitoring file system events, configured and managed identically to the existing `LauncherService` watcher.

**Acceptance Criterion:** The implementation uses `QFileSystemWatcher fs_watcher_` and connects its `fileChanged(const QString&)` and `directoryChanged(const QString&)` signals to debounce logic. Error handling follows the same pattern as `LauncherService` (e.g., graceful handling of permission denied, removed directories).

---

### REQ-NF-003: Launcher Behavior Unaffected

**EARS Sentence:** The addition of file watching and signal emission shall not alter the performance, correctness, or behavior of existing launcher functionality (app search, browsing, recent apps, app launch).

**Acceptance Criterion:** All existing launcher tests and manual smoke tests pass without modification. App launch latency and search responsiveness are not measurably degraded. The `LauncherService` scan/validate cycle completes in the same time as before (the `entriesUpdated` signal is a synchronous emission, not an async operation).

---

### REQ-NF-004: Initial Watch List is Populated at Construction

**EARS Sentence:** `MimeService` shall add all mimeapps.list and mimeinfo.cache watch paths to the `QFileSystemWatcher` during construction, before any external signals or MIME role queries are issued.

**Acceptance Criterion:** After `MimeService` is constructed, the internal `QFileSystemWatcher` reports at least 2 paths (for `~/.config/mimeapps.list` and `~/.local/share/applications/mimeapps.list`) and N paths for `mimeinfo.cache` (where N ≥ 1). A log statement or unit test confirms that watch setup completed.

---

## Constraints

### REQ-C-001: No Watching Non-Existent Directories at Startup

**EARS Sentence:** `MimeService` shall not attempt to watch XDG applications directories or mimeapps.list files that do not exist at shell startup; if a directory is created after startup, it shall not be automatically added to the watch list.

**Acceptance Criterion:** If `~/.local/share/applications/` does not exist when `MimeService` is constructed, the code skips adding `mimeinfo.cache` from that path (no attempt to watch a non-existent file). Directories created after shell startup are not added to the watcher. (User restart is the mechanism to pick up new XDG dirs.)

---

### REQ-C-002: Only 6 MIME Roles Supported

**EARS Sentence:** `MimeService` shall continue to support exactly 6 MIME roles (browser, terminal, file manager, image viewer, text editor, video player); no new roles shall be added as a result of this feature.

**Acceptance Criterion:** The implementation calls `xdg-mime query default` for the same 6 MIME types as before. No additional roles are discovered or watched. The `refreshAllRoles()` method signature and logic remain unchanged.

---

### REQ-C-003: No KDE ksycoca or DE-Specific Files Watched

**EARS Sentence:** `MimeService` shall not attempt to watch KDE-specific files (e.g., `ksycoca`, `/var/tmp/kdecache-*`), GNOME-specific files (e.g., `gnome-mimeapps.list`), or Plasma-specific files (e.g., `plasma-mimeapps.list`).

**Acceptance Criterion:** A grep of `MimeService` source files for `ksycoca`, `kdecache`, `gnome-mimeapps`, or `plasma-mimeapps` yields no matches. The implementation assumes a standard XDG environment.

---

### REQ-C-004: No /etc/xdg/mimeapps.list Watching

**EARS Sentence:** `MimeService` shall not watch or parse `/etc/xdg/mimeapps.list` or other system-wide DE-independent fallback MIME files.

**Acceptance Criterion:** The watch list includes only `~/.config/mimeapps.list` and `~/.local/share/applications/mimeapps.list`, not `/etc/xdg/mimeapps.list`. The `xdg-mime query default` command (which respects `/etc/xdg/` fallbacks internally) handles system defaults via existing `refreshAllRoles()` logic.

---

## Non-Goals

- **Watching parent directories for not-yet-created XDG app dirs**: If `~/.local/share/applications/` does not exist at shell startup, the shell will not monitor for its creation. Users must restart the shell after creating new XDG application directories.
- **KDE Plasma or GNOME-specific hybrid session support**: The feature assumes a standard XDG environment. DE-specific mechanisms (ksycoca, Plasma's file association cache) are not integrated.
- **Expanding the 6 standard MIME roles**: This feature does not add support for additional MIME types or application categories beyond the current browser, terminal, file manager, image viewer, text editor, and video player.
- **Runtime reconfiguration of watch paths**: Once `MimeService` is constructed, the set of watched directories is fixed for the shell's lifetime.

---

## Implementation Notes

- `LauncherService::entriesUpdated` should be emitted **after** the model is updated, not before, so MimeService's call to `refreshAllRoles()` (which queries the launcher's entries via `entriesForMimeTypes()` and `entriesForCategory()`) sees current data.
- `MimeService::refreshAllRoles()` re-queries `xdg-mime query default` for all 6 roles. Ensure the call is synchronous and completes within the 500ms debounce window so DefaultAppRow sees the updated values immediately.
- The `QFileSystemWatcher` may emit spurious events (e.g., multiple `fileChanged` signals for a single write). The 500ms debounce naturally coalesces these into a single `refreshAllRoles()` call.
- Test injection: both `LauncherService` and `MimeService` should accept optional `QFileSystemWatcher` and `DesktopEntryScanner` pointers in their constructors for unit testing (allow tests to substitute mock watchers or scanner results).


# Launcher Redesign – EARS Requirements Specification

**Document Version:** 1.0  
**Last Updated:** 2026-06-18  
**Status:** Approved for Architecture Design

---

## Executive Summary

This specification defines the behavior of a redesigned application launcher for holonight-shell. The launcher operates in two modes: **Browse mode** (empty query) displays all installed applications in alphabetical order with category filtering; **Search mode** (non-empty query) presents ranked results segmented by type (Best Match, Applications, Actions). Recent app tracking, XDG category mapping, and desktop action support are core features. All requirements use EARS (Easy Approach to Requirements Syntax) templates to ensure clarity and traceability.

---

## 1. Browse Mode – Left Panel (All Apps, Alphabetical, Category Filter)

### REQ-F-001 (Ubiquitous)
The launcher shall display all installed desktop applications in the left panel, sorted alphabetically by application name (case-insensitive, ignoring leading articles like "The").

**Acceptance Criteria:**
- Launching the launcher in Browse mode populates the left panel with ≥3 system applications in alphabetical order.
- Application names are sorted case-insensitively (e.g., "App A" appears before "app b").
- Leading articles ("The", "A", "An") are stripped before sorting but still displayed in the UI.

### REQ-F-002 (Event-driven)
When the user clicks a category in the right panel CATEGORIES section, the launcher shall filter the left panel to show only applications belonging to that category.

**Acceptance Criteria:**
- Clicking "Development" filters the left list to only applications with a matching mapped category.
- Clicking "All" resets the filter and shows all applications.
- The filter persists until a different category is clicked or the launcher closes.
- The filter is **not** a mode switch — the launcher remains in Browse mode.

### REQ-F-003 (State-driven)
While a category filter is active, the left panel shall display only the subset of applications matching that category, in alphabetical order.

**Acceptance Criteria:**
- Filtering to "Games" shows only game applications, sorted alphabetically within that filter.
- The application count displayed next to the category name in the right panel matches the count of visible applications in the left panel.
- Filtering a category with zero matching applications displays an empty state in the left panel.

### REQ-F-004 (Ubiquitous)
The launcher shall support clicking on any application in the left panel to select it (highlight/focus).

**Acceptance Criteria:**
- Clicking an application in the left list visually highlights it (e.g., background color change).
- Keyboard navigation (Up/Down) also selects an application.
- Only one application is selected at a time.

---

## 2. Browse Mode – Right Panel (Recent Section, Categories Section)

### REQ-F-005 (Ubiquitous)
The right panel shall display a RECENT section at the top, showing the 5 most recently launched applications (ordered newest to oldest).

**Acceptance Criteria:**
- After launching an application, it appears at the top of the RECENT list.
- Previously launched applications shift down; the oldest recent app is removed after 5 new launches.
- Each recent app displays its name and icon.
- If no applications have been launched, the RECENT section is empty or shows a placeholder.

### REQ-F-006 (Ubiquitous)
The right panel shall display a CATEGORIES section below RECENT, listing all available XDG categories with mapped display names.

**Acceptance Criteria:**
- At least 10 standard categories are listed (Multimedia, Development, Education, Games, Graphics, Internet, Office, Science, Settings, System, Other).
- "All" is always the first category.
- Each category displays a count of applications (e.g., "Games (5)").
- Categories with zero matching applications are still listed, showing a count of 0.

### REQ-F-007 (Event-driven)
When the launcher is opened in Browse mode, the launcher shall default to showing all applications (no category filter active).

**Acceptance Criteria:**
- Opening the launcher displays the full alphabetical list of all applications.
- No category is visually pre-highlighted or actively filtered.

### REQ-F-008 (Ubiquitous)
The right panel RECENT section shall only show applications, never desktop actions.

**Acceptance Criteria:**
- Even if a desktop action has been invoked, the recent history records the parent application.
- A user who launches only desktop actions (never the app itself) sees the app in RECENT after the action fires.

---

## 3. Search Mode – Left Panel (BEST MATCH, APPLICATIONS, ACTIONS Sections)

### REQ-F-009 (Event-driven)
When the user types any non-empty character sequence in the launcher's search field, the launcher shall transition from Browse mode to Search mode and display a ranked results list in the left panel.

**Acceptance Criteria:**
- Typing a single character triggers the transition immediately.
- The left panel clears Browse mode content and displays Search mode sections.
- The transition takes ≤50 ms from keystroke to display update (including ranking overhead).
- Deleting all characters returns to Browse mode.

### REQ-F-010 (Ubiquitous)
In Search mode, the left panel shall display results in three sections: **BEST MATCH**, **APPLICATIONS**, and **ACTIONS**, in that order from top to bottom.

**Acceptance Criteria:**
- BEST MATCH contains exactly 1 result (the highest-ranked application).
- APPLICATIONS contains all remaining application matches (0 or more).
- ACTIONS contains all desktop action matches (0 or more).
- Each section header is visually distinct and labeled.

### REQ-F-011 (Ubiquitous)
The BEST MATCH section shall display the highest-ranked application match in a visually accented row (larger, distinctive background or style).

**Acceptance Criteria:**
- The BEST MATCH application is visually larger or has a distinct background color relative to APPLICATIONS rows.
- BEST MATCH row is automatically selected on entry to Search mode.
- Clicking BEST MATCH launches the application immediately.

### REQ-F-012 (Ubiquitous)
The APPLICATIONS section shall display all non-best-match applications matching the search query, ranked by relevance and sorted alphabetically within each relevance tier.

**Acceptance Criteria:**
- An application matching the search in its name/comment/keywords appears in this section.
- Applications are ranked with name prefix matches higher than substring/keyword matches.
- Within each tier, applications are sorted alphabetically.
- Clicking any application in this section selects it and populates the context panel.

### REQ-F-013 (Ubiquitous)
The ACTIONS section shall display desktop actions (from `.desktop` `[Desktop Action *]` sections) whose parent applications or action names match the search query.

**Acceptance Criteria:**
- An action "Open File" from a parent app "Firefox" appears in ACTIONS if the search is "file" or "firefox".
- Each action displays its name and parent app name.
- Clicking an action launches it and records the parent app in Recent history.
- If no desktop actions match, the ACTIONS section is empty (or hidden).

### REQ-F-014 (State-driven)
While the search query is non-empty, the left panel shall update in real time as the query is edited.

**Acceptance Criteria:**
- Typing one more character re-ranks and refreshes results within ≤50 ms.
- Deleting a character re-ranks results immediately.
- Removing all characters returns to Browse mode without latency.

---

## 4. Search Mode – Right Panel (FILTERS Section, SELECTED ITEM Context Panel)

### REQ-F-015 (Ubiquitous)
In Search mode, the right panel top shall display a FILTERS section showing the count of matching applications and actions.

**Acceptance Criteria:**
- FILTERS displays "Applications (N)" and "Actions (M)" where N and M are the counts in their respective sections.
- Counts reflect the current search query and update as the query changes.
- Both counts are visible simultaneously.

### REQ-F-016 (Event-driven)
When the user clicks a filter button (e.g., "Applications (N)") in the FILTERS section, the launcher shall hide the non-selected result types.

**Acceptance Criteria:**
- Clicking "Applications (N)" hides the ACTIONS section; BEST MATCH and APPLICATIONS remain.
- Clicking "Actions (M)" hides BEST MATCH and APPLICATIONS; only ACTIONS is shown.
- Clicking the active filter again (e.g., clicking "Applications" when Applications is already the only visible section) removes the filter and re-shows all sections.
- The active filter is visually highlighted (e.g., different background or underline).

### REQ-F-017 (Ubiquitous)
In Search mode, the right panel bottom shall display a SELECTED ITEM context panel showing details of the currently highlighted application or action.

**Acceptance Criteria:**
- When an application is highlighted in the left panel, the context panel displays the app's name, icon, last-used date (or blank if never launched), and a list of its desktop actions.
- When a desktop action is highlighted, the context panel displays the action's parent app name, the action's display name, and the parent app's icon.
- The context panel updates as the user navigates with Up/Down keys.
- The context panel is empty or shows a placeholder if no item is selected.

### REQ-F-018 (Event-driven)
When the user moves selection to an application using keyboard navigation (Up/Down keys), the launcher shall update the SELECTED ITEM context panel to show that application's details.

**Acceptance Criteria:**
- Pressing Down once from BEST MATCH highlights the first APPLICATIONS entry and updates the context panel.
- The context panel displays the new application's last-used date (from Recent history, or blank).
- The context panel lists all desktop actions for that application.
- Navigation is instant (≤10 ms panel update).

### REQ-F-019 (Ubiquitous)
The SELECTED ITEM context panel shall display the last-used date and time of an application (from Recent history) in a human-readable format.

**Acceptance Criteria:**
- An application launched today displays "Today, 2:30 PM" or similar.
- An application launched yesterday displays "Yesterday, 10:15 AM" or similar.
- An application never launched displays blank or "Never" in the last-used field.
- The date format is locale-aware (using the system's date/time locale).

### REQ-F-020 (Ubiquitous)
The SELECTED ITEM context panel shall list all desktop actions for the selected application, regardless of whether the launcher is in Browse or Search mode.

**Acceptance Criteria:**
- Clicking an application in Browse mode filters the view, but clicking its name or icon in the context panel still lists its actions.
- Each action displays its display name (from the `Name` field in `[Desktop Action *]`).
- Clicking an action in the context panel launches it.
- If an application has no desktop actions, the action list is empty or shows a placeholder.

---

## 5. Recent Tracking (Storage, Max Entries, Last-Used Lookup)

### REQ-F-021 (Event-driven)
When an application is launched (via Enter, or click on the selected app), the launcher shall record the application's desktop file path and the launch timestamp in the Recent history.

**Acceptance Criteria:**
- The desktop file name and an ISO8601 timestamp are written to the history immediately after launch.
- If the application was already in Recent history, it is moved to the top and marked with the new timestamp.
- The history is persisted to disk.

### REQ-F-022 (Ubiquitous)
The Recent history shall be stored in a JSON file at `$XDG_CACHE_HOME/holonight-shell/launch-history.json`.

**Acceptance Criteria:**
- The directory `$XDG_CACHE_HOME/holonight-shell/` exists after the first launch is recorded.
- The JSON file is human-readable and contains an array of objects with `desktopFile` and `lastUsed` keys.
- The file persists across shell restarts; launching the launcher again after a restart shows the same Recent applications.

### REQ-F-023 (Ubiquitous)
The Recent history shall retain a maximum of 20 unique desktop file entries.

**Acceptance Criteria:**
- After 20 applications have been launched, launching a 21st application removes the oldest entry from the history.
- The oldest entry is identified by the earliest `lastUsed` timestamp.
- The JSON file never contains more than 20 entries.

### REQ-F-024 (Ubiquitous)
When a desktop action is launched, the launcher shall record the **parent application** (not the action) in the Recent history.

**Acceptance Criteria:**
- Launching the "Open URL" action from Firefox records Firefox's desktop file in Recent, not the action.
- The timestamp reflects the moment the action was invoked.
- The Recent list shows Firefox in the correct chronological position.

### REQ-F-025 (Ubiquitous)
The launcher shall provide a function to retrieve the last-used date and time of an application by its desktop file path.

**Acceptance Criteria:**
- Calling `lastUsedFor(desktopFile)` returns a `QDateTime` representing the most recent launch timestamp.
- If the application is not in Recent history, the function returns a null/invalid `QDateTime`.
- The function is efficient (sub-millisecond) for typical history sizes (≤20 entries).

### REQ-F-026 (State-driven)
While the launcher is open, the Recent section shall display only the 5 most recently launched applications (from the stored history of up to 20).

**Acceptance Criteria:**
- The RECENT section shows the top 5 entries from the JSON file, ordered newest-first.
- If fewer than 5 applications have been launched, the RECENT section shows all available entries.
- Launching an application during a launcher session immediately reflects the new app in the RECENT section on next Browse mode view.

---

## 6. Desktop Actions Parsing and Launching

### REQ-F-027 (Ubiquitous)
The launcher shall parse `[Desktop Action ActionName]` sections from `.desktop` files and extract the `Name` (display string) and `Exec` (command) fields.

**Acceptance Criteria:**
- A `.desktop` file with `[Desktop Action LaunchBrowser]` and `Name=Open Website` is parsed correctly.
- The `Exec` field is extracted and available for launching.
- Invalid or malformed sections are silently skipped (no error dialogs).

### REQ-F-028 (Ubiquitous)
The launcher shall support field code substitution in desktop action `Exec` fields, following the XDG Desktop Entry Specification (stripping unsupported codes like `%i`, `%c`).

**Acceptance Criteria:**
- An action with `Exec=firefox %U` is launched with URLs from the placeholder when applicable.
- Field codes like `%F` and `%U` are expanded or safely removed.
- Unsupported codes are stripped, leaving the rest of the command intact.
- The command is executed in a shell (e.g., via `QProcess` or equivalent) so pipes and environment variables work.

### REQ-F-029 (Event-driven)
When the user launches a desktop action (by clicking it in the left panel or context panel), the launcher shall execute the action's `Exec` command in the launcher's environment.

**Acceptance Criteria:**
- Clicking "Open Website" on Firefox launches the associated command.
- The launcher closes ≤200 ms after the action is invoked (does not block waiting for the action to complete).
- The action is launched with the standard input/output/error redirected appropriately (not attached to the launcher's stdio).

### REQ-F-030 (Ubiquitous)
The launcher's results shall list desktop actions separately from applications, with each action displaying its parent application's name.

**Acceptance Criteria:**
- In the ACTIONS section, each entry shows "Action Name (Parent App)" or similar formatting.
- A user can distinguish between launching an app directly and invoking its action.
- Clicking an action navigates to that action in the ACTIONS section (not the parent app).

---

## 7. Category Mapping (XDG → Curated Display Name)

### REQ-F-031 (Ubiquitous)
The launcher shall map XDG `Categories` field values to curated display names according to the following table:

| XDG Categories | Mapped Display Name |
|---|---|
| AudioVideo, Audio, Video | Multimedia |
| Development | Development |
| Education | Education |
| Game | Games |
| Graphics | Graphics |
| Network, Internet | Internet |
| Office | Office |
| Science | Science |
| Settings | Settings |
| System, Utility, Utilities | System |
| (all others) | Other |

**Acceptance Criteria:**
- An application with XDG `Categories=AudioVideo;Graphics` maps to the first matching category in the table (Multimedia, since AudioVideo is checked first).
- An application with `Categories=UnknownCategory;AnotherUnknown` maps to "Other".
- An application with an empty or missing `Categories` field maps to "Other".

### REQ-F-032 (Ubiquitous)
Each application shall appear in exactly one mapped category (the first matching mapped category in priority order).

**Acceptance Criteria:**
- An application is not shown in multiple category buckets simultaneously.
- The mapping is deterministic based on the order of XDG values and the priority table above.
- A category count in the CATEGORIES section reflects the number of applications mapped to that category.

### REQ-F-033 (Ubiquitous)
The "All" category shall always be the first category in the CATEGORIES list and shall display all installed applications (no filter).

**Acceptance Criteria:**
- "All" is listed at the top of the CATEGORIES section.
- Clicking "All" shows all applications in the left panel, regardless of any prior filter.
- The "All" count equals the total number of installed applications on the system.

### REQ-F-034 (Ubiquitous)
The launcher shall include categories with zero matching applications in the CATEGORIES list, displaying their count as 0.

**Acceptance Criteria:**
- Even if no applications are in the "Games" category, "Games (0)" is displayed.
- Clicking a zero-count category displays an empty left panel.
- Clicking back to "All" repopulates the list normally.

---

## 8. Keyboard Navigation

### REQ-F-035 (Ubiquitous)
The launcher shall support the following keyboard shortcuts:
- **Esc**: Close the launcher.
- **Enter**: Launch the selected application.
- **Ctrl+Enter**: Launch the selected application in a terminal.
- **Up/Down**: Navigate the left panel (Browse or Search mode).
- **Tab**: Navigate to the next item (left panel, then right panel, then wrap to left).

**Acceptance Criteria:**
- Pressing Esc closes the launcher and returns focus to the previously focused window.
- Pressing Enter on a selected application launches it and closes the launcher.
- Pressing Ctrl+Enter launches the application with a terminal prefix (if supported by the Exec field).
- Up/Down keys move selection one item at a time; pressing Up on the first item wraps to the last item.
- Tab moves focus between the left and right panels in a predictable order.

### REQ-F-036 (State-driven)
While in Browse mode with a category filter active, pressing Up/Down shall navigate within the filtered list only.

**Acceptance Criteria:**
- If the "Games" category is selected (filtering to 8 games), Up/Down navigates only these 8 applications.
- The first game is immediately accessible from the last game via Up wraparound.
- Pressing Up on the first filtered item wraps to the last filtered item.

### REQ-F-037 (State-driven)
While in Search mode, Up/Down navigation shall move between the BEST MATCH, APPLICATIONS, and ACTIONS sections in order.

**Acceptance Criteria:**
- Pressing Down from BEST MATCH moves to the first APPLICATIONS result.
- Pressing Down from the last APPLICATIONS result moves to the first ACTIONS result.
- Pressing Down from the last ACTIONS result wraps to BEST MATCH.
- Up/Down navigation reflects the current filter state (if only APPLICATIONS are shown, navigation stays within APPLICATIONS).

### REQ-F-038 (Event-driven)
When the user types characters in the search field, the launcher shall clear the current selection and automatically select the BEST MATCH result.

**Acceptance Criteria:**
- After typing "fire", the BEST MATCH (e.g., Firefox if it's the top result) is automatically selected.
- The context panel updates to show the BEST MATCH details.
- No manual selection is required to launch the top result.

---

## 9. Non-Functional Requirements

### REQ-NF-001 (Ubiquitous)
The launcher shall load all installed desktop applications from the system's `.desktop` file paths (e.g., `/usr/share/applications/`, `~/.local/share/applications/`) at startup.

**Acceptance Criteria:**
- The launcher populates the application list within ≤500 ms of opening.
- All standard system applications (50+) are discovered.
- Custom `.desktop` files in `~/.local/share/applications/` are included.

### REQ-NF-002 (Ubiquitous)
The launcher's search ranking algorithm shall prioritize exact name prefix matches, then substring matches, then keyword matches.

**Acceptance Criteria:**
- Searching "fire" ranks "Firefox" (prefix) above "Firewall Manager" (prefix) above "Settings for Firewalls" (substring).
- Results within each tier are sorted alphabetically.
- The ranking is consistent across repeated searches with identical queries.

### REQ-NF-003 (Ubiquitous)
The launcher shall use no hardcoded hex color values in QML; all colors shall come from the HoloNight palette (`HoloniightPalette` tokens).

**Acceptance Criteria:**
- All QML color properties reference `HoloniightPalette.<token>`, not `#rrggbb` or `rgb(r, g, b)`.
- The launcher respects the system theme and adapts to theme changes without recompilation.

### REQ-NF-004 (Ubiquitous)
The launcher shall use only theme-aware icons from the system icon theme or bundled SVG/PNG assets; no hardcoded icon paths.

**Acceptance Criteria:**
- Application icons are loaded from the system theme or bundled assets via QML's `image://icon/` provider.
- Theme changes reflect updated icons without launcher restart.

### REQ-NF-005 (Ubiquitous)
JSON persistence of Recent history shall use UTF-8 encoding and standard JSON formatting (no compression).

**Acceptance Criteria:**
- The history file is human-readable and editable in a text editor.
- ISO8601 timestamps are used consistently (e.g., `"2026-06-18T14:30:45Z"`).
- No binary or compressed data appears in the file.

### REQ-NF-006 (Ubiquitous)
The launcher shall not attempt to index files, directories, or bookmarks; search shall be limited to application names, descriptions, keywords, and desktop actions.

**Acceptance Criteria:**
- Searching "Documents" does not return file system results.
- Only installed applications and their actions appear in results.
- This is a hard constraint; file indexing is explicitly out of scope.

### REQ-NF-007 (Ubiquitous)
The launcher's Recent history shall be independent of system wallpaper, desktop environment session data, or other shell settings; corruption of the Recent JSON shall not prevent the launcher from functioning.

**Acceptance Criteria:**
- If the history file is deleted, the launcher still opens and functions (with an empty Recent section).
- If the history file is corrupted (invalid JSON), the launcher logs a warning and continues with an empty history.
- The launcher never crashes due to history file issues.

### REQ-NF-008 (Ubiquitous)
All C++ classes exposing Recent history to QML shall be registered as QML singletons with appropriate type names (e.g., `RecentAppsTracker`).

**Acceptance Criteria:**
- QML can import and access the Recent tracker without instantiation boilerplate.
- Type names are consistent across all QML files that reference the tracker.

---

## 10. Constraints

### REQ-C-001 (Ubiquitous)
The launcher's open/close animations shall remain unchanged from the current implementation (150 ms scale+fade transitions).

**Acceptance Criteria:**
- Opening the launcher uses the existing 150 ms scale+fade animation.
- Closing the launcher uses the existing 150 ms scale+fade animation.
- No new animation timings or easing functions are introduced.

### REQ-C-002 (Ubiquitous)
The launcher shall preserve all existing keyboard shortcuts and not introduce conflicts with shell-wide keybindings.

**Acceptance Criteria:**
- Esc, Enter, Ctrl+Enter, Up, Down, Tab all work as described and do not interfere with other shell commands.
- No new keybindings are added without explicit user configuration.

### REQ-C-003 (Ubiquitous)
The launcher shall not allocate persistent resources (threads, timers, file handles) while closed; all state shall be reset on launcher close and reinitialized on next open.

**Acceptance Criteria:**
- Closing the launcher releases all file handles to the Recent history JSON.
- No background processes or timers run after the launcher closes.
- Opening the launcher again reloads the history freshly from disk.

### REQ-C-004 (Ubiquitous)
The launcher shall run within the existing holonight-shell memory and performance constraints; no feature shall increase baseline memory usage by more than 10 MB (excluding application index cache).

**Acceptance Criteria:**
- The launcher's resident memory (RSS) stays below 50 MB after opening and populating the application list.
- The Recent history JSON (max 20 entries) adds ≤100 KB to persistent storage.
- Search result ranking completes within ≤50 ms for typical searches (20-100 matching results).

### REQ-C-005 (Ubiquitous)
The launcher shall not modify system configuration files, user home directory structure, or shell state outside of writing the Recent history JSON.

**Acceptance Criteria:**
- Only `$XDG_CACHE_HOME/holonight-shell/launch-history.json` is created/modified.
- No `.desktop` files, shell configs, or environment variables are written.
- The launcher is fully reversible (removing it leaves no artifacts).

### REQ-C-006 (Ubiquitous)
All XDG Desktop Entry field code expansions and command execution shall follow the XDG Desktop Entry Specification (freedesktop.org) without deviation.

**Acceptance Criteria:**
- Field codes are expanded identically to other XDG-compliant launchers (e.g., GNOME Activities, KDE Kickoff).
- Unsupported codes are removed cleanly without breaking the command.
- No shell injection vulnerabilities are introduced; commands are executed safely via `QProcess` or equivalent.

---

## Glossary

| Term | Definition |
|---|---|
| **Browse Mode** | Launcher state when the search field is empty; displays all applications alphabetically with category filtering. |
| **Search Mode** | Launcher state when the search field contains ≥1 character; displays ranked results in BEST MATCH, APPLICATIONS, and ACTIONS sections. |
| **Recent History** | JSON file tracking the 20 most recently launched applications with timestamps. |
| **Desktop Action** | An application action defined in a `.desktop` file `[Desktop Action *]` section, allowing the launcher to invoke secondary commands. |
| **XDG Categories** | The `Categories` field in a `.desktop` file, containing semicolon-separated tokens mapping to application types. |
| **Mapped Category** | A curated display name assigned to one or more XDG categories (e.g., "AudioVideo" → "Multimedia"). |
| **BEST MATCH** | The highest-ranked search result, displayed in a visually accented row. |
| **Field Code** | Placeholder tokens in the `Exec` field (e.g., `%U`, `%F`) substituted with values at launch time per the XDG spec. |
| **Acceptance Criterion** | An independently verifiable statement confirming that a requirement is met. |

---

## Traceability & Sign-Off

This specification establishes 30 functional requirements (REQ-F-001 through REQ-F-030), 8 non-functional requirements (REQ-NF-001 through REQ-NF-008), and 6 constraints (REQ-C-001 through REQ-C-006), totaling **44 requirements**. Each requirement includes at least one acceptance criterion to enable objective verification.

**Specification Author:** Claude Code  
**Date:** 2026-06-18  
**Status:** Ready for Architecture Design Phase

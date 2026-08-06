# Specification: Notification History

## Overview

The notification-history feature adds persistent bounded storage for desktop notifications in holonight-shell, along with a topbar icon showing unread count. When notifications expire via timeout or are closed by the application, they are moved to a JSON-backed history file. Users can open the history via the notification icon to review past notifications; dismissed notifications (user-initiated X or swipe) do not enter history.

## Requirements

### History Population

**REQ-F-001 | Ubiquitous | Move expired notifications to history**
- **Sentence:** The notification service shall move a notification to history when its timeout interval expires.
- **Acceptance Criterion:** When a notification auto-dismiss timeout fires, the corresponding history entry exists in memory with `closedReason=1` (Expired) before persistence.

**REQ-F-002 | Ubiquitous | Move closed notifications to history**
- **Sentence:** The notification service shall move a notification to history when the application sends a D-Bus CloseNotification request.
- **Acceptance Criterion:** When CloseNotification is invoked with a notification ID, the corresponding history entry exists in memory with `closedReason=3` (Closed) before persistence.

**REQ-F-003 | Unwanted behaviour | Do not store dismissed notifications**
- **Sentence:** If a user dismisses a notification (clicks the close button or swipes), then the notification service shall not add it to history.
- **Acceptance Criterion:** After a user-initiated dismiss action, a history query returns zero entries for that notification ID across all persistence operations.

### Persistence & Storage

**REQ-F-004 | Ubiquitous | Persist history entries to disk asynchronously**
- **Sentence:** The notification store shall write history to a JSON file asynchronously using QtConcurrent, never blocking the notification service main thread.
- **Acceptance Criterion:** A blocking operation on the main thread is not observed (measured by debug logs showing write completion on a worker thread) during normal notification flow.

**REQ-F-005 | Ubiquitous | Serialize concurrent writes to the history file**
- **Sentence:** The notification store shall serialize all writes to the history file to prevent data corruption from overlapping operations.
- **Acceptance Criterion:** When multiple notifications are received in rapid succession and each triggers a write, the final JSON file is valid and contains all expected entries.

**REQ-F-006 | Ubiquitous | Store history entries with required fields**
- **Sentence:** The notification store shall persist each history entry with id, appName, summary, appIcon, urgency, timestamp, closedReason, read, and appClass fields.
- **Acceptance Criterion:** After a notification is written and the JSON file is loaded, all nine fields are present and correctly typed (id as uint32, timestamp as milliseconds-since-epoch, read as boolean).

**REQ-F-007 | Conditional | Omit body field when persist_body is disabled**
- **Sentence:** Where the `persist_body` configuration is false, the notification store shall not include the body field in the persisted JSON.
- **Acceptance Criterion:** When `persist_body=false` and a notification is persisted, the body field does not appear in the JSON object for that entry.

**REQ-F-008 | Ubiquitous | Store actions for restored entries**
- **Sentence:** The notification store shall preserve the actions array from the original notification in the persisted JSON so that future UI implementations can render them.
- **Acceptance Criterion:** The JSON object for a history entry contains an `actions` array with the original key/label pairs.
- **Note:** Rendering action buttons as disabled is out of scope for this feature. This requirement exists to ensure the data is available; the visual treatment is deferred to a future sidebar Notifications tab implementation.

### Configuration

**REQ-NF-001 | Ubiquitous | Support notification history configuration section**
- **Sentence:** The configuration service shall parse a `[notifications.history]` TOML section with enabled, max_items, max_age_days, and persist_body keys.
- **Acceptance Criterion:** After reading a config file with a `[notifications.history]` section, ConfigService exposes a NotificationHistoryConfig struct with all four fields populated.

**REQ-NF-002 | Ubiquitous | Apply default values for missing config keys**
- **Sentence:** Where config keys are missing, the configuration service shall use defaults: enabled=true, max_items=100, max_age_days=14, persist_body=true.
- **Acceptance Criterion:** When the config file lacks a `[notifications.history]` section or is empty, the NotificationHistoryConfig object contains the specified default values.

**REQ-F-009 | Conditional | Skip history recording when disabled**
- **Sentence:** Where `enabled=false` in the configuration, the notification service shall not record or persist any history entries.
- **Acceptance Criterion:** When enabled=false, notifications do not appear in the history file or in-memory history list, even after expiry or CloseNotification.

### Unread / Read Semantics

**REQ-F-010 | Ubiquitous | Mark new history entries as unread**
- **Sentence:** The notification service shall set the read flag to false when a notification is first moved to history.
- **Acceptance Criterion:** A newly created history entry has `read=false` before and immediately after persistence.

**REQ-F-011 | Event-driven | Mark all history entries as read when sidebar opens**
- **Sentence:** When the sidebar is opened by any means (including the notification icon click), the notification service shall mark all unread history entries as read.
- **Acceptance Criterion:** After SidebarManager.toggle() is invoked, all entries in the history list have `read=true` and a subsequent sidebar query reports unreadCount=0.

**REQ-F-012 | Ubiquitous | Pre-read history entries from disk**
- **Sentence:** The notification service shall load all history entries from the history file at startup with the read flag set to true.
- **Acceptance Criterion:** On application startup, history entries loaded from disk have `read=true` and do not contribute to the unread count badge.

### Topbar Icon & Badge

**REQ-F-013 | Ubiquitous | Display notification icon in topbar**
- **Sentence:** The notification service shall display a NotificationsWidget in the topbar StatusesSection, positioned between BatteryWidget and KeyboardLayoutWidget.
- **Acceptance Criterion:** When holonight-shell is running, a notification icon is visible in the topbar at the specified position.

**REQ-F-014 | Event-driven | Toggle sidebar when notification icon is clicked**
- **Sentence:** When the notification icon is clicked, the notification widget shall invoke SidebarManager.toggle() with the current bar monitor name.
- **Acceptance Criterion:** Clicking the notification icon in the topbar opens the sidebar if closed, or closes it if open.

**REQ-F-015 | State-driven | Adjust icon opacity based on unread count**
- **Sentence:** While the unread count is zero, the notification icon shall display with reduced opacity (~0.55); while the unread count is greater than zero, it shall display at full opacity.
- **Acceptance Criterion:** When unreadCount=0, the icon opacity is ≤0.55; when unreadCount>0, the icon opacity is ≥0.95.

**REQ-F-016 | State-driven | Display badge dot when unread entries exist**
- **Sentence:** While the unread count is greater than zero, the notification icon shall display a 6×6px violet pulsing badge dot at the bottom-right corner.
- **Acceptance Criterion:** When unreadCount>0, a violet glow dot appears at the bottom-right of the icon; when unreadCount=0, the dot is not visible.

**REQ-NF-003 | Ubiquitous | Pulse badge with standard animation**
- **Sentence:** The badge dot shall animate with a glow effect pulsing between opacity 0.25 and 0.9 at 900ms using InOutSine easing, matching the TrayItem badge pattern.
- **Acceptance Criterion:** The badge animation cycle is 900ms, easing is InOutSine, and opacity oscillates between 0.25 and 0.9.

### Tooltip

**REQ-F-017 | Ubiquitous | Display tooltip on notification icon hover**
- **Sentence:** The notification widget shall display a BarTooltipArea tooltip showing unread count and app names.
- **Acceptance Criterion:** Hovering over the notification icon reveals a tooltip with the current unread count and the deduplicated list of app names that have unread entries.

**REQ-F-018 | Ubiquitous | Show unread count in tooltip first line**
- **Sentence:** The tooltip shall display the unread entry count as the first line (e.g., "5 unread").
- **Acceptance Criterion:** The tooltip first line shows the current unread count in human-readable format.

**REQ-F-019 | Ubiquitous | List app names in tooltip second line**
- **Sentence:** The tooltip shall display a deduplicated, comma-separated list of application names from unread history entries on the second line.
- **Acceptance Criterion:** The tooltip second line contains unique app names, sorted, and separated by ", " with no duplicates.

**REQ-F-020 | Ubiquitous | Elide tooltip text when too long**
- **Sentence:** The tooltip shall elide the app name list with Text.ElideRight if it exceeds the available width.
- **Acceptance Criterion:** When app name text would exceed the tooltip width, the rightmost characters are replaced with ellipsis.

### Startup Behaviour

**REQ-F-021 | Event-driven | Load history file on application startup**
- **Sentence:** When the notification service initializes, it shall synchronously load the history file from `~/.local/state/holonight/notifications/history.json` if it exists.
- **Acceptance Criterion:** On first run after writing test history entries, the notification service loads those entries and reports the correct count without blocking for >100ms.

**REQ-F-022 | Ubiquitous | Run age-based eviction on startup**
- **Sentence:** The notification service shall apply age-based eviction (removing entries older than max_age_days) immediately after loading the history file.
- **Acceptance Criterion:** When history is loaded and an entry has a timestamp older than (now - max_age_days * 86400000ms), that entry is not present in the in-memory history list.

**REQ-F-023 | Ubiquitous | Create history directory if missing**
- **Sentence:** The notification store shall create the history directory (`~/.local/state/holonight/notifications/`) if it does not exist before attempting to persist a history entry.
- **Acceptance Criterion:** On the first history write, the directory is created automatically; subsequent writes do not fail due to missing parent directories.

### Eviction & Capacity

**REQ-F-024 | Event-driven | Evict oldest entries when max_items is exceeded**
- **Sentence:** After a history entry is added, if the total history count exceeds max_items, the notification store shall evict the oldest entries by timestamp until the count is exactly max_items.
- **Acceptance Criterion:** When 101 entries exist and max_items=100, the entry with the lowest timestamp is removed after eviction completes.

**REQ-F-025 | Event-driven | Run eviction after each write**
- **Sentence:** After each successful write to the history file, the notification store shall apply both age-based and capacity-based eviction.
- **Acceptance Criterion:** A single history write operation triggers eviction; inspecting the in-memory list shows no entries exceed max_items and all remaining entries are within max_age_days.

### Non-Functional Requirements

**REQ-NF-004 | Ubiquitous | Parse and validate history JSON format**
- **Sentence:** The notification store shall use a robust JSON parser that validates structure and type constraints, logging errors for malformed entries without crashing.
- **Acceptance Criterion:** A corrupted history.json file with missing required fields results in a logged error and the service continues without loading invalid entries.

**REQ-NF-005 | Ubiquitous | Limit memory footprint of in-memory history**
- **Sentence:** The in-memory history list shall not exceed max_items entries plus a small fixed overhead for metadata.
- **Acceptance Criterion:** With max_items=100, the in-memory QList or QVector footprint for history does not exceed approximately 10MB.

**REQ-NF-006 | Ubiquitous | Use efficient Qt containers for history storage**
- **Sentence:** The notification history shall be stored in a Qt container (QList or QVector) suitable for sequential access and eviction by index.
- **Acceptance Criterion:** History entries support O(1) random access and O(1) removal from the front/back without full list scans.

### Constraints

**REQ-C-001 | Ubiquitous | Do not persist notification body for privacy**
- **Sentence:** Where the configuration option `persist_body=false`, the notification body field shall not be written to disk to preserve user privacy.
- **Acceptance Criterion:** The persist_body option is documented in the configuration file and is tested to exclude the body field when set to false.

**REQ-C-002 | Ubiquitous | Respect only Expired and Closed close reasons**
- **Sentence:** The notification store shall only persist notifications with closedReason values of 1 (Expired) or 3 (Closed), rejecting all other reasons including 2 (Dismissed).
- **Acceptance Criterion:** A notification with closedReason=2 (Dismissed) is never added to history, even if triggered from the UI.

**REQ-C-003 | Ubiquitous | Use UTC timestamps for history entries**
- **Sentence:** All history entry timestamps shall be stored as milliseconds since the Unix epoch (1970-01-01 00:00:00 UTC) in 64-bit integer format.
- **Acceptance Criterion:** A persisted timestamp can be converted back to a human-readable date using standard epoch conversion without loss of precision.

**REQ-C-004 | Ubiquitous | Store history file in XDG_STATE_HOME**
- **Sentence:** The history file shall be located at `~/.local/state/holonight/notifications/history.json`, respecting the XDG Base Directory specification.
- **Acceptance Criterion:** The path is constructed from `XDG_STATE_HOME` (defaulting to `~/.local/state`) and the history file is always stored at this location.

**REQ-C-005 | Ubiquitous | Limit history to configured bounds**
- **Sentence:** The notification store shall never exceed max_items entries or retain entries older than max_age_days, enforced by eviction on every write and at startup.
- **Acceptance Criterion:** After a full load-write-evict cycle, the history list is within bounds: count ≤ max_items and all timestamps are within max_age_days.

## Out of Scope

- Sidebar Notifications tab UI rendering
- Keyboard navigation of history entries
- Full-text search of history
- History export/import
- Per-app notification filtering or archival

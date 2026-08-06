# SDD Tasks — notification-history

- [x] T-001: Add NotificationHistoryItem and NotificationHistoryConfig structs to NotificationTypes.h
  - REQs: REQ-F-006, REQ-NF-001, REQ-NF-002
  - Check: After building, both structs are defined in src/services/notifications/NotificationTypes.h with all required fields (id, appName, summary, body, appIcon, appClass, urgency, timestampMs, closedReason, read, actions for item; enabled, max_items, max_age_days, persist_body for config).

- [x] T-002: Add NotificationHistoryConfig to ConfigService.h and implement parsing in ConfigService.cpp
  - REQs: REQ-NF-001, REQ-NF-002, REQ-F-009
  - Check: After adding a [notifications.history] section to the test config with mixed keys, ConfigService parses it correctly, applies defaults for missing keys (enabled=true, max_items=100, max_age_days=14, persist_body=true), and emits notificationHistoryChanged() when reloaded.

- [x] T-003: Create NotificationStore.h with load() and persist() method signatures
  - REQs: REQ-F-004, REQ-F-005, REQ-F-021, REQ-F-023
  - Check: After building, NotificationStore class is defined with load(), persist(), historyFilePath() methods, writeCompleted() signal, and private fields for write_in_flight_, write_dirty_, pending_snapshot_, watcher_.

- [x] T-004: Implement NotificationStore::load() to read and parse history.json synchronously
  - REQs: REQ-F-021, REQ-F-006, REQ-NF-004, REQ-C-004, REQ-C-003
  - Check: Calling load() on a test history.json with valid and malformed entries returns a QList of NotificationHistoryItem with only valid entries parsed; malformed entries are skipped with qCWarning and do not crash.

- [x] T-005: Implement NotificationStore::persist() and async write serialization with dirty-flag pattern
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-008, REQ-C-001, REQ-C-003, REQ-C-004
  - Check: After calling persist() twice in rapid succession with different snapshots, the JSON file on disk contains only the second snapshot's entries; body field is omitted when persist_body=false; writeCompleted() signal fires on the main thread after each write finishes.

- [x] T-006: Add ensureDirectoryExists() to NotificationStore to create ~/.local/state/holonight/notifications/
  - REQs: REQ-F-023
  - Check: On the first persist() call with a missing parent directory, the directory is created automatically and the write succeeds without filesystem errors.

- [x] T-007: Add history_ QList, unread_count_ field, store_ ownership, and history_config_ to NotificationService.h
  - REQs: REQ-F-010, REQ-F-011, REQ-F-012
  - Check: After building, NotificationService declares private fields history_ (QList<NotificationHistoryItem>), unread_count_ (int), store_ (NotificationStore*), and history_config_ (NotificationHistoryConfig).

- [x] T-008: Add unreadCount Q_PROPERTY and unreadCountChanged signal to NotificationService.h
  - REQs: REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-015, REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-019
  - Check: After building, NotificationService exposes Q_PROPERTY int unreadCount with READ unreadCount, NOTIFY unreadCountChanged and a public getter returning unread_count_.

- [x] T-009: Add onSidebarOpened(const QString& monitor_name) private slot to NotificationService.h
  - REQs: REQ-F-011
  - Check: Header declares the private slot signature; implementation marks all history entries as read, resets unread_count_ to 0, and emits unreadCountChanged().

- [x] T-010: Wire NotificationService constructor to instantiate NotificationStore, call load(), and run age eviction at startup
  - REQs: REQ-F-021, REQ-F-022, REQ-F-012
  - Check: After starting the notification service with a pre-populated history.json file containing old entries, age-based eviction runs synchronously; entries older than max_age_days are not present in memory; all loaded entries are pre-marked read=true and unread_count_=0.

- [x] T-011: Connect NotificationService to ConfigService notificationHistoryChanged signal and update history_config_ on reload
  - REQs: REQ-NF-001, REQ-NF-002
  - Check: After reloading the config with a different [notifications.history] section, NotificationService.history_config_ reflects the new values.

- [x] T-012: Connect NotificationService to SidebarManager sidebarOpened signal and wire onSidebarOpened() slot
  - REQs: REQ-F-011
  - Check: After clicking the notification icon (which calls SidebarManager.toggle()), the onSidebarOpened() signal fires and all history entries are marked read; unreadCount becomes 0.

- [x] T-013: Connect NotificationStore writeCompleted signal to NotificationService eviction and unreadCountChanged handler
  - REQs: REQ-F-024, REQ-F-025
  - Check: After a write completes, age and capacity eviction run on the main thread; unreadCountChanged() is emitted only if unread_count_ changed due to eviction; history size never exceeds max_items and all entries are within max_age_days.

- [x] T-014: Extend NotificationService::closeNotification() to capture NotificationData before erase and build NotificationHistoryItem
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-C-002
  - Check: When a notification expires (closedReason=Expired) or is closed by app (closedReason=Closed), it is moved to history; when dismissed by user (closedReason=Dismissed), it is not recorded; appClass is extracted from hints["desktop-entry"]; timestampMs is set to QDateTime::currentMSecsSinceEpoch(); read is set to false.

- [x] T-015: Extend closeNotification() to append history item, increment unread_count_, and call store_->persist() when enabled
  - REQs: REQ-F-001, REQ-F-002, REQ-F-009, REQ-F-010
  - Check: After a notification expires and history is enabled, the item is appended to history_; unread_count_ increments; unreadCountChanged() is emitted; store_->persist(history_) is called with the full snapshot.

- [x] T-016: Implement eviction helper in NotificationService to remove aged and excess entries and recompute unread_count_
  - REQs: REQ-F-024, REQ-F-025, REQ-C-005
  - Check: Calling the eviction method removes all entries older than (now - max_age_days * 86400000ms) and excess entries beyond max_items; unread_count_ is recomputed via std::ranges::count_if after eviction.

- [x] T-017: Create NotificationsWidget.qml with icon, opacity binding, and hover/tap handlers
  - REQs: REQ-F-013, REQ-F-014, REQ-F-015
  - Check: After building and running the shell, a notification icon appears in the topbar StatusesSection between BatteryWidget and KeyboardLayoutWidget; icon opacity is 0.55 when unreadCount=0 and animates to 1.0 when unreadCount>0; clicking the icon calls SidebarManager.toggle().

- [x] T-018: Implement NotificationsWidget badge dot with MultiEffect glow and pulsing animation
  - REQs: REQ-F-016, REQ-NF-003
  - Check: When unreadCount>0, a 6×6px violet dot appears at the bottom-right of the icon with MultiEffect glow (declared before dot per z-order rule); SequentialAnimation pulses opacity between 0.25 and 0.9 at 900ms with InOutSine easing; when unreadCount=0, the dot is invisible and animation stops.

- [x] T-019: Implement NotificationsWidget BarTooltipArea with unreadCount label and deduplicated app names list
  - REQs: REQ-F-017, REQ-F-018, REQ-F-019, REQ-F-020
  - Check: Hovering over the notification icon shows a tooltip with first line as "N unread" and second line as comma-separated unique app names from unread history entries, sorted alphabetically; app names list is elided with Text.ElideRight if exceeding available width.

- [x] T-020: Insert NotificationsWidget into StatusesSection.qml between BatteryWidget and KeyboardLayoutWidget
  - REQs: REQ-F-013
  - Check: After editing StatusesSection.qml and building, the notification widget appears at the correct position in the topbar with correct barMonitorName propagation.

- [x] T-021: Register NotificationStore.h/.cpp and NotificationsWidget.qml in CMakeLists.txt
  - REQs: REQ-F-004, REQ-F-013, REQ-NF-006
  - Check: After running task configure and task build, no CMake errors occur; holonight-shell links successfully with NotificationStore and NotificationsWidget included.

- [x] T-022: Build verification — ensure all components compile and link without errors
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009, REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-015, REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-019, REQ-F-020, REQ-F-021, REQ-F-022, REQ-F-023, REQ-F-024, REQ-F-025, REQ-NF-001, REQ-NF-002, REQ-NF-003, REQ-NF-004, REQ-NF-005, REQ-NF-006, REQ-C-001, REQ-C-002, REQ-C-003, REQ-C-004, REQ-C-005
  - Check: After running task build, the binary holonight-shell is produced without compiler or linker errors; no clang-tidy warnings are introduced by new code.

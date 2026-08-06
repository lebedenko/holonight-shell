# SDD Tasks — notification-hardening

- [x] T-001: Create NotificationFilter.h/.cpp with enum and structs
  - REQs: REQ-C-NH02, REQ-NF-NH01
  - Check: `src/services/notifications/NotificationFilter.h` declares `UrgencyFilter` enum (None/Low/Normal/LowAndNormal), `AppNotificationRule` struct (app_name/enabled/urgency_filter), `FilterDecision` enum (Allow/Suppress), and `evaluateFilter(const NotificationData&, bool, const QList<AppNotificationRule>&) -> FilterDecision` free function; .cpp contains implementation

- [x] T-002: Create NotificationRuleModel.h/.cpp
  - REQs: REQ-F-NH06, REQ-F-NH07, REQ-F-NH08, REQ-F-NH11, REQ-F-NH12, REQ-C-NH04
  - Check: `src/services/notifications/NotificationRuleModel.h` declares `NotificationRuleModel` class inheriting `QAbstractListModel` with `QML_ELEMENT` and `QML_SINGLETON`; declares `ensureApp(const QString&)`, `setEnabled(int, bool)`, `setUrgencyFilter(int, int)` as public slots/invokables; declares `AppNameRole`, `EnabledRole`, `UrgencyFilterRole` enum; .cpp implements all overrides and methods; file ends with `#include "NotificationRuleModel.moc"`

- [x] T-003: Wire NotificationRuleModel into ShellApplication
  - REQs: REQ-F-NH06, REQ-C-NH04
  - Check: `ShellApplication` header declares `NotificationRuleModel* notification_rule_model_` member; `ShellApplication` constructor constructs member before `NotificationService`; member is accessed without null-check in later tasks

- [x] T-004: Add dndEnabled Q_PROPERTY to NotificationService
  - REQs: REQ-F-NH01, REQ-F-NH02, REQ-F-NH04, REQ-F-NH05
  - Check: `NotificationService` header declares `Q_PROPERTY(bool dndEnabled READ dndEnabled WRITE setDndEnabled NOTIFY dndEnabledChanged FINAL)`; default value is `false`; `dndEnabledChanged()` signal declared; implementation stores backing field and emits signal on change

- [x] T-005: Add daemonConflict and daemonConflictOwner CONSTANT properties to NotificationService
  - REQs: REQ-F-NH18, REQ-F-NH20
  - Check: `NotificationService` header declares `Q_PROPERTY(bool daemonConflict READ daemonConflict CONSTANT FINAL)` and `Q_PROPERTY(QString daemonConflictOwner READ daemonConflictOwner CONSTANT FINAL)`; declares `setDaemonConflict(const QString&)` setter; properties read backing fields `daemon_conflict_` and `daemon_conflict_owner_`

- [x] T-006: Inject NotificationRuleModel* into NotificationService constructor
  - REQs: REQ-F-NH06
  - Check: `NotificationService` constructor signature includes `NotificationRuleModel* rule_model` parameter; pointer stored as member `rule_model_`; all construction call-sites pass the injected pointer from `ShellApplication`

- [x] T-007: Add daemon detection probe in NotificationServer::start()
  - REQs: REQ-C-NH05, REQ-F-NH18
  - Check: `NotificationServer::start()` calls `bus.interface()->serviceOwner("org.freedesktop.Notifications")` before `registerService()`; if owner exists, sets `conflict_detected_ = true` and `conflict_owner_ = owner_reply.value()`; skips `registerService()` on conflict; declares accessor methods `conflictDetected()` and `conflictOwner()`

- [x] T-008: Wire daemon conflict detection in ShellApplication::startServices()
  - REQs: REQ-F-NH18, REQ-F-NH19
  - Check: `ShellApplication::startServices()` calls `notification_server_->start()`; checks `if (notification_server_->conflictDetected())` and calls `notification_service_->setDaemonConflict(notification_server_->conflictOwner())` with owner name

- [x] T-009: Implement filter evaluation in NotificationService::addOrReplace()
  - REQs: REQ-F-NH02, REQ-F-NH06, REQ-F-NH07, REQ-F-NH08, REQ-F-NH09, REQ-F-NH10, REQ-F-NH16
  - Check: `NotificationService::addOrReplace(const NotificationData&)` calls `rule_model_->ensureApp(data.app_name)` on entry; calls `evaluateFilter(data, dnd_enabled_, rule_model_->rules())` before insertion; returns `0` (suppressed) when filter decision is `Suppress`; suppressed notifications are never inserted into model, never armed with timer, never written to history

- [x] T-010: Register UrgencyFilter enum with Qt meta-object system
  - REQs: REQ-F-NH08
  - Check: `NotificationFilter.h` declares `Q_ENUM_NS(UrgencyFilter)` or equivalent; `NotificationService.h` includes meta-object registration so QML can reference `NotificationService.UrgencyFilter.Low`, `UrgencyFilter.Normal`, etc.

- [x] T-011: Add NotificationFilter and NotificationRuleModel to CMakeLists.txt
  - REQs: (build requirement)
  - Check: `CMakeLists.txt` lists `NotificationFilter.cpp` and `NotificationRuleModel.cpp` in `HOLONIGHT_SERVICE_SOURCES` or equivalent notification service sources; `task configure` and `task build` succeed without compilation errors

- [x] T-012: Replace SidebarNotifications.qml stub with ColumnLayout skeleton
  - REQs: REQ-F-NH01, REQ-F-NH06, REQ-F-NH11
  - Check: `src/qml/RightSidebar/SidebarNotifications.qml` root is `ColumnLayout`; contains a `Text` item with "Notifications" or similar section header; contains placeholder text "No apps seen yet" shown when rules list is empty; sets `preferredHeight` dynamically from `contentColumn.implicitHeight`

- [x] T-013: Add DND toggle row to SidebarNotifications.qml
  - REQs: REQ-F-NH01, REQ-F-NH02, REQ-F-NH05
  - Check: DND row placed below section header; contains disc-button or switch bound to `NotificationService.dndEnabled`; active state renders with `HoloniightPalette.error` accent color; row label reads "Do Not Disturb"; toggling button in QML console immediately flips `dndEnabled` property

- [x] T-014: Add daemon conflict diagnostic row to SidebarNotifications.qml
  - REQs: REQ-F-NH18, REQ-F-NH19, REQ-F-NH20
  - Check: Diagnostic row placed below DND row; shown only when `NotificationService.daemonConflict === true`; displays warning glyph (⚠) in error color; displays text "Notification daemon conflict: <owner>\nStop this service to enable HoloNight notifications." where `<owner>` is `NotificationService.daemonConflictOwner` or "unknown" if empty

- [x] T-015: Add per-app rules Repeater to SidebarNotifications.qml
  - REQs: REQ-F-NH06, REQ-F-NH07, REQ-F-NH08, REQ-F-NH11, REQ-C-NH03
  - Check: `Repeater { model: NotificationRuleModel }` placed below daemon conflict row; each delegate displays: app name label (bound to `model.appName`), enable toggle (bound to `model.enabled`, calls `NotificationRuleModel.setEnabled(index, checked)`), urgency filter ComboBox (bound to `model.urgencyFilter`, calls `NotificationRuleModel.setUrgencyFilter(index, currentIndex)` with values 0–3); ComboBox delegate uses explicit `id` reference, not `parent.*`; placeholder text hidden when rules.length > 0

- [x] T-016: Add GTest unit test for evaluateFilter()
  - REQs: REQ-F-NH02, REQ-F-NH08, REQ-F-NH10, REQ-F-NH16
  - Check: `tests/test_notification_filter.cpp` compiles and links; contains `TEST` cases covering: (1) critical urgency (=2) always returns Allow regardless of DND or per-app rules; (2) DND=true suppresses non-critical notifications; (3) per-app rule with `enabled=false` suppresses non-critical; (4) per-app rule with matching `urgency_filter` suppresses that urgency level; (5) critical notification bypasses per-app disabled rule; `ctest -R test_notification_filter` passes all cases

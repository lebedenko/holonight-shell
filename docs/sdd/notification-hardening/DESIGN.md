# Design — notification-hardening

## Overview

This feature extends the existing `org.freedesktop.Notifications` D-Bus server in HoloNight shell with four orthogonal capabilities:

1. **Do Not Disturb (DND)** — a runtime-only boolean toggle on `NotificationService` that suppresses all non-critical incoming notifications.
2. **Per-app rules** — a session-scoped in-memory `QAbstractListModel` (`NotificationRuleModel`) that auto-populates from seen `app_name` values and allows per-app enable/urgency-filter control.
3. **Critical notification hardening** — urgency=2 notifications bypass all suppression rules, are styled with the error accent colour, and never auto-dismiss. The visual layer for this is already implemented in `ToastItem.qml` and `effectiveTimeoutMs()`.
4. **Daemon conflict detection** — `NotificationServer::start()` probes `org.freedesktop.Notifications` ownership before attempting to claim it; the result is surfaced as a pair of CONSTANT properties on `NotificationService` for QML diagnostic display.

All filtering is applied in a single gateway (`NotificationService::addOrReplace()`) so suppressed notifications never enter the model, never arm timers, and are never written to history.

---

## Spec Conflict Resolution

**REQ-F-NH10** states: a per-app rule set to "off" suppresses ALL notifications from that app regardless of urgency.

**REQ-F-NH16** states: a notification with critical urgency (`urgency=2`) displays even if the app rule is "off".

These requirements are contradictory at urgency=2. The tie-breaking rule established by this design:

> **Stage 0 ground truth: `urgency==2` always wins.** Critical urgency bypasses both DND and per-app rules unconditionally.

REQ-F-NH10 is superseded by REQ-F-NH16 for `urgency=2`. The filter pipeline evaluates critical urgency as the first and final check — if it passes, nothing else is consulted.

Rationale: critical notifications exist specifically for conditions that must reach the user regardless of preference settings. Allowing a per-app rule to silently swallow a critical alert would be a safety regression; REQ-F-NH16 is the stronger, more specific requirement.

---

## Components

### 1. `AppNotificationRule` and `UrgencyFilter` — value types in `NotificationFilter.h`

**File:** `src/services/notifications/NotificationFilter.h` (+ `.cpp`)

`AppNotificationRule` is a plain data struct — no `QObject`, no Qt event-loop dependency — that holds the per-app rule state:

```
struct AppNotificationRule {
    QString    app_name;
    bool       enabled         = true;
    UrgencyFilter urgency_filter = UrgencyFilter::None;
};
```

`UrgencyFilter` is a scoped enum controlling which urgency levels are suppressed by the rule (independently of the `enabled` flag):

```
enum class UrgencyFilter : uint8_t {
    None         = 0,  // suppress nothing (pass all non-disabled)
    Low          = 1,  // suppress urgency=0
    Normal       = 2,  // suppress urgency=1
    LowAndNormal = 3,  // suppress urgency=0 and urgency=1
};
```

The `enabled` flag and `urgency_filter` are orthogonal: `enabled=false` means suppress all (below critical); a specific `UrgencyFilter` suppresses only matching urgency levels regardless of `enabled`. The filter function unifies both into a single pass/suppress decision.

`AppNotificationRule` needs `Q_GADGET` and `QML_VALUE_TYPE` or simply integer roles in `NotificationRuleModel` — prefer roles only (no gadget exposure) to keep the struct free of Qt meta-object overhead.

---

### 2. `NotificationFilter` — free function in `NotificationFilter.h`

**File:** `src/services/notifications/NotificationFilter.h` (+ `.cpp`)

A single pure free function with no `QObject` dependency:

```
enum class FilterDecision { Allow, Suppress };

FilterDecision evaluateFilter(
    const NotificationData& data,
    bool dnd_enabled,
    const QList<AppNotificationRule>& rules);
```

**Pipeline:**
1. `data.urgency == NotifUrgency::Critical` → return `Allow` (stage 0 gate; no further checks).
2. `dnd_enabled` → return `Suppress`.
3. Find rule with `rule.app_name == data.app_name` (linear scan; expected list size is O(10s) of apps):
   - Rule found, `!rule.enabled` → return `Suppress`.
   - Rule found, `rule.urgency_filter` matches `data.urgency` → return `Suppress`.
4. return `Allow`.

Keeping this as a free function mirrors the existing `NotificationPolicy.h` pattern (pure logic, no Qt event loop) and makes it independently unit-testable with `QCoreApplication` only.

`NotificationService::addOrReplace()` calls `evaluateFilter()` after calling `rule_model_->ensureApp(data.app_name)` (auto-populate first, then filter) and before inserting into the model.

---

### 3. `NotificationRuleModel` — new QAbstractListModel singleton

**Files:** `src/services/notifications/NotificationRuleModel.h`, `NotificationRuleModel.cpp`

```cpp
class NotificationRuleModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

 public:
  enum Roles { AppNameRole = Qt::UserRole + 1, EnabledRole, UrgencyFilterRole };
  Q_ENUM(Roles)

  explicit NotificationRuleModel(QObject* parent = nullptr);

  // Called by NotificationService on every Notify() arrival with a non-empty app_name.
  // No-op if app_name is already in the list.
  void ensureApp(const QString& app_name);

  // Reference used by NotificationService to pass rules to evaluateFilter().
  [[nodiscard]] const QList<AppNotificationRule>& rules() const { return rules_; }

  // QML-callable rule mutations — act on row index from Repeater/ListView.
  Q_INVOKABLE void setEnabled(int row, bool enabled);
  Q_INVOKABLE void setUrgencyFilter(int row, int filter);  // filter: int(UrgencyFilter)

  // QAbstractListModel overrides
  int rowCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

 private:
  QList<AppNotificationRule> rules_;
};
```

**Roles exposed to QML:**
- `appName` (QString) — displayed in the per-app rules list
- `enabled` (bool) — bound to a toggle switch
- `urgencyFilter` (int) — bound to a segment/combo selector (0–3 maps to `UrgencyFilter`)

**Ownership and registration:** `NotificationRuleModel` is created in `ShellApplication`'s constructor as a raw pointer member (consistent with all other services). `ShellApplication::registerQmlTypes()` registers it as `"NotificationRuleModel"` using the same `reg()` lambda pattern already in use.

`NotificationService` receives a `NotificationRuleModel*` pointer injected via its constructor (new parameter), following the `ConfigService*` / `ActiveWindowService*` injection pattern already present.

**In-memory only:** no file, no SQLite. `QList<AppNotificationRule>` lives as long as the process. Rules are cleared on restart (REQ-F-NH12, REQ-C-NH04).

---

### 4. DND State — new Q_PROPERTY on `NotificationService`

**File:** `src/services/notifications/NotificationService.h` / `.cpp`

DND state is a writable runtime boolean on `NotificationService`:

```cpp
Q_PROPERTY(bool dndEnabled READ dndEnabled WRITE setDndEnabled NOTIFY dndEnabledChanged FINAL)
```

Backed by `bool dnd_enabled_{false}` (default off; no persistence — REQ-F-NH04).

`setDndEnabled(bool)` flips the field and emits `dndEnabledChanged()`. No cascading action on existing live notifications — REQ-F-NH23 states rules apply to incoming notifications only; the historical and currently-displayed set is untouched.

QML writes it directly:

```qml
NotificationService.dndEnabled = !NotificationService.dndEnabled
```

This matches the pattern `IdleService.idleInhibited = !IdleService.idleInhibited` in `KeepAwakeAction.qml`.

**Rationale for placing DND on NotificationService rather than a new class:** `NotificationService` is already the QML-facing singleton for all notification behaviour and is the sole entry point for new notifications via `addOrReplace()`. Adding DND here costs one property and one field; splitting it into a new class would add a dependency edge with no architectural benefit.

---

### 5. Daemon Detection — probe in `NotificationServer`, exposed via `NotificationService`

**Files:** `src/services/notifications/NotificationServer.h/.cpp`, `NotificationService.h/.cpp`, `ShellApplication.cpp`

**Detection (in NotificationServer::start()):**

Before calling `bus.registerService()`, probe ownership of `org.freedesktop.Notifications`:

```cpp
const QDBusReply<QString> owner_reply =
    bus.interface()->serviceOwner(QString::fromUtf8(kServiceName));
if (owner_reply.isValid()) {
    // Another process holds the name — record it and skip registration.
    conflict_detected_ = true;
    conflict_owner_    = owner_reply.value();
    qCWarning(lcNotificationServer) << "daemon conflict:" << conflict_owner_;
    return;
}
// Name is unclaimed — proceed with registerService() + registerObject() as before.
```

This satisfies REQ-C-NH05 (check before advertising) and REQ-F-NH20 (owner name captured for diagnostic).

New members on `NotificationServer`:
```cpp
bool    conflict_detected_{false};
QString conflict_owner_;
// Accessors (non-Q_PROPERTY; not a QML type):
[[nodiscard]] bool    conflictDetected() const;
[[nodiscard]] QString conflictOwner() const;
```

**Wiring (in ShellApplication::startServices()):**

```cpp
notification_server_->start();
if (notification_server_->conflictDetected()) {
    notification_service_->setDaemonConflict(notification_server_->conflictOwner());
}
```

**Exposure on NotificationService (QML-facing):**

```cpp
Q_PROPERTY(bool    daemonConflict      READ daemonConflict      CONSTANT FINAL)
Q_PROPERTY(QString daemonConflictOwner READ daemonConflictOwner CONSTANT FINAL)

void setDaemonConflict(const QString& owner);
```

`CONSTANT` is correct: the conflict state is determined once at startup and never changes. This mirrors `idleDaemonDetected CONSTANT` on `IdleService`.

QML reads `NotificationService.daemonConflict` to conditionally show the diagnostic row, and `NotificationService.daemonConflictOwner` to build the actionable message (REQ-F-NH20: "Stop `<owner>` to enable HoloNight notifications").

---

### 6. Critical Notification QML Styling — already implemented

**Files:** `src/qml/Notifications/ToastItem.qml`, `src/services/notifications/NotificationTypes.cpp`

The critical-urgency visual treatment is already in place and requires **no new code**:

| Requirement | Where implemented | Status |
|---|---|---|
| REQ-F-NH14 — error accent colour | `ToastItem.qml`: `accentColor` returns `HoloniightPalette.error` when `accentKind === "critical"` | Done |
| REQ-F-NH15 — no auto-dismiss timeout | `NotificationTypes.cpp`: `effectiveTimeoutMs()` returns `-1` for `urgency == Critical` | Done |
| REQ-F-NH17 — persistent close button | `ToastItem.qml`: close button (`closeButton` item, lines 283–327) is always rendered | Done |
| Accent routing | `NotificationTypes.cpp`: `accentForData()` sets `NotifAccentKind::Critical` for urgency=2 | Done |

The design confirms no QML changes are needed for critical styling. The filter pipeline (section 2) guarantees critical notifications always reach `addOrReplace()` regardless of DND or per-app rules.

---

### 7. DND Toggle UI — in `SidebarNotifications.qml`

**File:** `src/qml/RightSidebar/SidebarNotifications.qml`

The current file is a stub (placeholder label only). Replace the body with a `ColumnLayout`-based section following the `SidebarSystem.qml` / `SidebarQuickSettings.qml` structural pattern.

**DND toggle row:** a circular disc action button (visual pattern: `KeepAwakeAction.qml`) bound to `NotificationService.dndEnabled`. Use `HoloniightPalette.error` as the active-state accent (red = "blocking"), `HoloniightPalette.borderPassive` for inactive. Label: "Do Not Disturb". A brief subtitle or icon state change visually indicates active DND (REQ-F-NH05).

**Daemon conflict diagnostic row:** shown only when `NotificationService.daemonConflict` is `true`. Matches the `kdeCompatRow` pattern in `SidebarSystem.qml`: warning glyph (`"⚠"`, `HoloniightPalette.error`), descriptive text, no action button (the user must stop the conflicting daemon externally). Text template:

```
Notification daemon conflict: <owner>
Stop this service to enable HoloNight notifications.
```

Where `<owner>` is `NotificationService.daemonConflictOwner` (or "unknown" if empty).

---

### 8. Per-App Rules UI — in `SidebarNotifications.qml`

**File:** `src/qml/RightSidebar/SidebarNotifications.qml`

Below the DND row, a section header ("App Notifications") followed by a `Repeater { model: NotificationRuleModel }`.

Each delegate row shows:
- **App name label** bound to `model.appName`.
- **Enable/disable toggle** — a `Controls.Switch` or small disc button whose `checked` state is `model.enabled`; `onToggled` calls `NotificationRuleModel.setEnabled(index, checked)`.
- **Urgency filter selector** — a compact `Controls.ComboBox` or three-segment control with options: "All", "Block Low", "Block Normal", "Block Low+Normal", bound to `model.urgencyFilter`. `onActivated` calls `NotificationRuleModel.setUrgencyFilter(index, currentIndex)`.

When the list is empty (no notifications received yet in this session), display a subtle placeholder text ("No apps seen yet").

**Scrollability:** `SidebarNotifications.qml` sets `preferredHeight` dynamically from `contentColumn.implicitHeight + 32`, matching `SidebarSystem.qml`. The outer `ScrollView` in `SidebarContent.qml` handles overflow; no inner scroll needed.

**Per-app rule mutations from QML** use `Q_INVOKABLE` methods on `NotificationRuleModel` by row index (from the `Repeater`'s `index`). This matches how `MimeService.setDefaultBrowser(desktopFile)` is called in `SidebarSystem.qml` — invokable methods, no property-binding write-back.

Note from CLAUDE.md / `feedback_holonight_combobox_parent.md`: delegate `contentItem` and `background` in a `Controls.ComboBox` (or any `T.Control` subclass) must reference the control via an explicit `id`, not `parent.*`, to avoid resolving to `QQuickItem`. Declare `id: appRuleDelegate` on each delegate root and use `appRuleDelegate.text` / `appRuleDelegate.highlighted` inside `contentItem`/`background`.

---

## Data Flow

```
org.freedesktop.Notifications.Notify() D-Bus call
  → NotificationServer::Notify()
      Builds NotificationData (urgency, app_name, hints decoded)
  → NotificationService::addOrReplace(data)
      1. rule_model_->ensureApp(data.app_name)          // auto-populate, even if later suppressed
      2. evaluateFilter(data, dnd_enabled_, rule_model_->rules())
            urgency==2  → Allow  (critical always passes; REQ-F-NH16 > REQ-F-NH10)
            dnd_enabled → Suppress
            rule found, !enabled → Suppress
            rule found, urgency_filter matches → Suppress
            else → Allow
      3. If Suppress: return 0 to caller (notification silently dropped)
                      NOT added to model, NOT timed, NOT added to history
      4. If Allow:
            allocate ID, resolve monitor, insert into model
            place() → visible or queued
            armTimeout() (no-op for critical: effectiveTimeoutMs==-1)
            emit notificationAdded(id, monitor)
  → NotificationManager reacts to notificationAdded
  → ToastStack (per monitor) shows ToastItem with accentKind from model
  → User dismisses → NotificationService::dismiss() → closeNotification(Dismissed)
      history NOT written for Dismissed (existing policy unchanged)
  → Auto-timeout expires → closeNotification(Expired)
      (never fires for critical: timer never armed)
      history written for Expired
```

**Return value when suppressed:** `addOrReplace()` returns `0`. The freedesktop spec requires IDs to be `> 0`, but returning `0` for suppressed notifications is acceptable in a single-seat shell server — clients use the returned ID only for `replaces_id` in a follow-up `Notify()` call or for `CloseNotification()`, neither of which applies to a notification that was never displayed.

---

## Key Decisions with Rationale

### DND state lives on NotificationService

`NotificationService` is already the QML-facing singleton and the sole insertion gateway for new notifications. Placing `dndEnabled` here costs one property and one field. A separate `NotificationFilterService` or `NotificationSettingsService` would add a dependency edge through `ShellApplication` with no architectural gain. Precedent: `IdleService.idleInhibited` is a writable property on the same service that implements the inhibition logic.

### NotificationFilter is a separate free function, not inline logic

Extracting `evaluateFilter()` into a standalone free function in `NotificationFilter.h/.cpp` mirrors the existing `NotificationPolicy.h` pattern and allows the filter pipeline to be unit-tested independently with GTest + `QCoreApplication` only (no QObject, no Qt event loop, no mock model). Inline logic in `addOrReplace()` would make the filter branch invisible to unit tests without constructing a full `NotificationService`.

### Filter applied in NotificationService::addOrReplace(), not in NotificationServer::Notify()

`NotificationServer` is a thin protocol adapter: it decodes D-Bus wire arguments into `NotificationData` and delegates all policy to `NotificationService`. Injecting filter logic into `NotificationServer` would couple the D-Bus layer to business rules and violate the adapter boundary already documented in `NotificationServer.h`. `NotificationService::addOrReplace()` is the correct single gateway for all notification lifecycle decisions.

### In-memory per-app rules, no persistence

Explicitly required by REQ-C-NH04 and REQ-F-NH12. In-memory-only storage avoids stale rules surviving app renames or reinstalls (the primary known limitation of `app_name`-key matching). A `QList<AppNotificationRule>` covers all expected session sizes (O(10s) of apps) with trivial cost.

### Daemon conflict result communicated through NotificationService, not NotificationServer

`NotificationServer` has no QML exposure and should remain a pure protocol adapter. Routing the conflict result through `NotificationService` (via `ShellApplication::startServices()` wiring) keeps all QML-facing state on a single well-known singleton. Pattern precedent: `IdleService` similarly exposes `idleDaemonDetected CONSTANT` rather than surfacing the daemon-scan result from `IdleBackend` directly.

### Critical bypass evaluated before DND (stage 0 gate)

REQ-F-NH16 establishes critical urgency as unconditional. Checking urgency=2 as the first step in `evaluateFilter()` ensures that no future suppression rule addition (DND schedule, content filter, etc.) can accidentally block a critical notification. The gate is visually first in the code and documented explicitly.

---

## Alternatives Considered

### 1. Put DND + rules in a new `NotificationSettingsService`

Rejected. Splitting settings state into a new service would require injecting it into `NotificationService` anyway (since `addOrReplace()` must read them), producing a three-node dependency triangle (`ShellApplication` → `NotificationSettingsService` → `NotificationService`) with no separation benefit. The equivalent in the existing codebase is `IdleService` owning both the idle detection logic and the user-controllable inhibit state — a single cohesive service.

### 2. Filter in NotificationServer::Notify() instead of addOrReplace()

Rejected for two reasons. First, it violates the adapter boundary: `NotificationServer` is documented as a protocol-only layer with "no application state". Second, the `replaces_id` path in `addOrReplace()` also needs filter evaluation (a replace of an existing visible notification by a suppressed-urgency repeat should still be suppressed); placing the check only in `Notify()` would miss that path.

### 3. Persist per-app rules to SQLite (like LauncherService)

Rejected by spec (REQ-C-NH04, REQ-F-NH12). Additionally, `app_name` string instability makes persisted rules a liability — a user's carefully configured rule silently stops working after an app update that changes its reported name. Session-only storage makes the failure mode obvious: users reconfigure after restart and immediately notice if the app name changed.

### 4. Expose `NotificationRuleModel` as a child property of `NotificationService` rather than a separate QML singleton

Rejected because `QML_ELEMENT`/`QML_SINGLETON` registration is the established pattern for all services in this codebase, and QML Repeater/ListView models work cleanly as singletons. A child `Q_PROPERTY` on `NotificationService` would require `QML_ELEMENT` on `NotificationRuleModel` anyway for type-safe use in QML, and would add a `Q_PROPERTY` slot on `NotificationService` for no gain.

### 5. Use a QML Switch in the DND toggle row instead of a custom disc button

Valid alternative, not rejected — the UX choice is implementation-defined. A custom disc button (matching `KeepAwakeAction.qml`) is recommended for visual consistency with the sidebar's existing action affordances. A `Controls.Switch` is acceptable if the QML author prefers a standard control. Either approach satisfies REQ-F-NH01 and REQ-F-NH05; the design does not mandate one over the other.

---

## Known Risks

### app_name string key instability (existing known limitation)

Per-app rules key on the free-form `app_name` D-Bus hint. Applications may change this string across versions, locales, or rebuilds. A user's rule for `"ExampleApp"` stops applying if the app begins reporting `"Example App v2.0"`. Acknowledged in the spec's Known Limitations. No mitigation in scope.

### Empty app_name handling (REQ-C-NH01)

If `app_name` is empty or absent, `ensureApp()` is a no-op (no rule is created) and the notification is evaluated against DND state only. The spec permits grouping under a special "Unlabeled" bucket as an alternative — this design chooses the simpler no-op. If the "Unlabeled" bucket is later desired, `ensureApp()` can be changed to use a sentinel key (e.g., `""`) with a display label "Unknown App". No structural change required.

### QML model update thread safety

`NotificationService::addOrReplace()` is called from `NotificationServer::Notify()`, which is dispatched on the GUI thread by Qt D-Bus. `NotificationRuleModel::ensureApp()` therefore also runs on the GUI thread. No cross-thread model mutation risk exists in the current architecture. If a background notification source is added later, the `ensureApp()` call must be explicitly routed through a queued connection or `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.

### NotificationServer::start() order vs QML registration

`ShellApplication::startServices()` calls `notification_server_->start()` then conditionally calls `notification_service_->setDaemonConflict()`. QML properties are read lazily after `registerQmlTypes()` runs (which precedes `startServices()`). Since `daemonConflict` and `daemonConflictOwner` are `CONSTANT`, they are read only once by QML bindings after the sidebar is first opened — well after `startServices()` completes. No race condition.

### serviceOwner() reply before registration attempt

The daemon detection probe calls `bus.interface()->serviceOwner()` synchronously before `registerService()`. On a healthy session bus this call returns within microseconds. If the bus is degraded at startup (extremely rare), the call returns an error reply and `conflict_detected_` stays false (false negative: no diagnostic shown, but registration attempt proceeds normally). This is the correct safe-failure direction.

---

## Integration Points

The following existing files require changes. New files are listed separately.

| File | Change |
|---|---|
| `src/services/notifications/NotificationServer.h` | Add `conflict_detected_`, `conflict_owner_` fields; add `conflictDetected()` / `conflictOwner()` accessors |
| `src/services/notifications/NotificationServer.cpp` | In `start()`: probe `serviceOwner()` before `registerService()`; record conflict if already owned |
| `src/services/notifications/NotificationService.h` | Add `dndEnabled` read/write property; add `daemonConflict` / `daemonConflictOwner` CONSTANT properties; add `setDaemonConflict()` setter; inject `NotificationRuleModel*` parameter in constructor |
| `src/services/notifications/NotificationService.cpp` | Call `rule_model_->ensureApp()` and `evaluateFilter()` at the top of `addOrReplace()`; return 0 early on Suppress; add DND field and setter impl |
| `src/app/ShellApplication.h` | Add `NotificationRuleModel* notification_rule_model_` member |
| `src/app/ShellApplication.cpp` | Construct `NotificationRuleModel` before `NotificationService`; pass pointer to `NotificationService` constructor; register as `"NotificationRuleModel"` in `registerQmlTypes()`; wire daemon conflict in `startServices()` |
| `src/qml/RightSidebar/SidebarNotifications.qml` | Replace stub with full ColumnLayout: DND toggle row, daemon conflict diagnostic row (conditional), per-app rules Repeater |
| `CMakeLists.txt` | Add `NotificationFilter.cpp`, `NotificationRuleModel.cpp` to the service sources; add `SidebarNotifications.qml` is already listed (verify it remains in `HOLONIGHT_QML_FILES`) |

**New files to create:**

| File | Purpose |
|---|---|
| `src/services/notifications/NotificationFilter.h` | `AppNotificationRule` struct, `UrgencyFilter` enum, `FilterDecision` enum, `evaluateFilter()` declaration |
| `src/services/notifications/NotificationFilter.cpp` | `evaluateFilter()` implementation |
| `src/services/notifications/NotificationRuleModel.h` | `NotificationRuleModel` class declaration (QAbstractListModel + QML_SINGLETON) |
| `src/services/notifications/NotificationRuleModel.cpp` | `NotificationRuleModel` implementation; `#include "NotificationRuleModel.moc"` at end |

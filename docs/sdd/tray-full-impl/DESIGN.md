# DESIGN — tray-full-impl: Complete System Tray Implementation

**Date**: 2026-05-25
**Project**: holonight-shell (C++23/Qt6 Wayland shell)
**Scope**: Full SNI + DBusMenu implementation on top of the existing tray skeleton
**Depends on**: `docs/sdd/topbar-tray/DESIGN.md` (existing skeleton)

---

## 1. Component Overview

### 1.1 Component map

```
Session D-Bus
  │
  ├─ org.kde.StatusNotifierWatcher  (owned by TrayWatcher — existing)
  │    RegisterStatusNotifierItem / RegisterStatusNotifierHost
  │    PropertiesChanged ──────────────────────────────────────────────┐
  │    NewIcon / NewAttentionIcon / NewStatus / NewTitle / NewToolTip ─┤
  │                                                                    │
  ├─ com.canonical.dbusmenu  (per item, on item's bus)                 │
  │    GetLayout / AboutToShow / Event                                 │
  │                                                                    ▼
  ▼                                                        ItemSignalWatcher (new)
TrayWatcher (existing — modified)                           routes SNI signals back
  │  fetchItemProperties / updateItemProperties                with item key
  │  +fetchToolTip (new)                                            │
  │  +subscribeItemSignals (new)                                    │
  ▼                                                                │
TrayItem struct (modified)                     ◄─────────────────┘
  +tooltip_title / +tooltip_description / +tooltip_icon_name

TrayModel (existing — modified)
  +roles: tooltipTitle / tooltipDescription / tooltipIconName
  +Q_INVOKABLE activate(key,x,y)          ← passes screen coords
  +Q_INVOKABLE secondaryActivate(key,x,y)
  +Q_INVOKABLE scroll(key,delta,orientation)
  +Q_INVOKABLE openContextMenu(key,x,y)
  │
  ├─ TrayImageProvider (existing — unchanged)
  │
  └─ DbusMenuClient (new, 1 per open menu, short-lived)
       GetLayout ──► DbusMenuModel (new)
       AboutToShow / Event
       │
       ▼
TrayMenuSurface (new C++ — dedicated layer-shell menu window)
  │  show(screen, x, y, menuModel, client)
  │  hide() / close()
  ▼
TrayMenuPopup.qml (new)
  ListView ──► TrayMenuItem.qml (new)
                  submenus: nested TrayMenuPopup

QML layer
  TraySection.qml (modified)
    slotAssignment[] — computed urgent-priority ordering (see §4)
    Repeater { model: slotAssignment }
      TrayItem.qml (modified)
        violet badge dot + MultiEffect glow  (replaces red border)
        right/middle click + scroll handlers
        BarTooltipArea — roles-driven fallback chain
```

### 1.2 File inventory

| File | Status | Purpose |
|------|--------|---------|
| `src/TrayItem.h/.cpp` | **Modified** | Add `SniToolTip` struct; extend `TrayItem` with tooltip fields |
| `src/TrayItemProperties.h/.cpp` | **Modified** | Parse `ToolTip` property from `QVariantMap` |
| `src/TrayWatcher.h/.cpp` | **Modified** | Add `ItemSignalWatcher` inner class; add `subscribeItemSignals()`; add `fetchToolTip()`; add `fetchSingleProperty()` |
| `src/TrayModel.h/.cpp` | **Modified** | Add tooltip roles; add `activate(key,x,y)`, `secondaryActivate`, `scroll`, `openContextMenu` invokables |
| `src/DbusMenuItem.h` | **New** | `DbusMenuItem` value struct; `DbusMenuModel` QAbstractListModel |
| `src/DbusMenuItem.cpp` | **New** | `DbusMenuModel` implementation |
| `src/DbusMenuClient.h/.cpp` | **New** | Async `GetLayout`/`AboutToShow`/`Event` manager for one item's menu |
| `src/TrayMenuSurface.h/.cpp` | **New** | Second layer-shell `QQuickView` for context menu popup |
| `src/qml/Tray/TraySection.qml` | **Modified** | `maxCollapsedItems: 3`; slot-priority computed array; slide-in animation |
| `src/qml/Tray/TrayItem.qml` | **Modified** | Remove red border; add violet badge + glow; right/middle/scroll handlers; tooltip role binding |
| `src/qml/Tray/TrayMenuPopup.qml` | **New** | Popup content: ListView bound to `DbusMenuModel` |
| `src/qml/Tray/TrayMenuItem.qml` | **New** | Single menu row: icon + label + submenu arrow + separator variant |

---

## 2. Data Flow

### 2.1 SNI direct signal path (new)

```
SNI item emits NewStatus("NeedsAttention")
  └─ ItemSignalWatcher::onNewStatus(status)
       └─ TrayWatcher::onItemDirectSignal(key, "Status", QVariant(status))
            └─ TrayWatcher::updateItemProperties(key, {{"Status", status}})
                 └─ TrayModel::updateItem(key, mergedItem)
                      └─ dataChanged(row, row)  ──► QML re-renders slot priority

SNI item emits NewIcon
  └─ ItemSignalWatcher::onNewIcon()
       └─ TrayWatcher::fetchSingleProperty(key, "IconName")
            └─ QDBusPendingCallWatcher → Get("IconName") completes
                 └─ updateItemProperties(key, {{"IconName", value}})

SNI item emits NewToolTip
  └─ ItemSignalWatcher::onNewToolTip()
       └─ TrayWatcher::fetchToolTip(key, service, path)
            └─ Get("ToolTip") completes → parseSniToolTip()
                 └─ updateItemProperties(key, {{"ToolTip", struct}})
                      └─ TrayModel::updateItem → dataChanged (tooltipTitle etc.)
```

### 2.2 Tooltip display path

```
User hovers TrayItem delegate
  └─ BarTooltipArea (450 ms delay)
       └─ TooltipSurface.show(barMonitorName, x, width,
              tooltipTitle || title,
              tooltipDescription || status,
              tooltipIconName || iconName)
```

### 2.3 Right-click / context menu path

```
User right-clicks TrayItem
  └─ TrayItem.qml MouseArea: TrayModel.openContextMenu(itemKey, globalX, globalY)
       └─ TrayModel looks up item service/path
       └─ Creates DbusMenuClient(service, menuPath, TrayMenuSurface)
            └─ Get("Menu") on SNI item → DBus object path of menu
            └─ DbusMenuClient::open(service, menuPath, x, y)
                 └─ GetLayout(0, -1, []) → QDBusPendingCallWatcher
                 └─ callback: parseLayout(dbusArg) → DbusMenuModel
                 └─ AboutToShow calls for all root items (batched, 1 s timeout)
                 └─ emit menuReady(model)
                      └─ TrayMenuSurface::show(screenName, x, y, model)
                           └─ QQuickView loads TrayMenuPopup.qml
                           └─ sets model property on root item

User clicks menu item
  └─ TrayMenuPopup calls DbusMenuClient.activateItem(id)
       └─ Event(id, "clicked", QDBusVariant(QString{}), timestamp)
       └─ wait for Event reply → DbusMenuClient::close() → TrayMenuSurface::hide()

User clicks outside / Escape
  └─ TrayMenuPopup close request → TrayMenuSurface::close()
       └─ DbusMenuClient::close() → TrayMenuSurface::hide() (sends no Event)
```

### 2.4 Mouse action path (left / middle / scroll)

```
Left click:
  TrayItem.qml → TrayModel.activate(key, globalX, globalY)
    └─ Activate(x, y) async D-Bus call on SNI item

Middle click:
  TrayItem.qml → TrayModel.secondaryActivate(key, globalX, globalY)
    └─ SecondaryActivate(x, y) async D-Bus call

Scroll:
  TrayItem.qml WheelHandler →
    TrayModel.scroll(key, delta, "vertical" | "horizontal")
      └─ Scroll(delta, orientation) async D-Bus call
```

---

## 3. Interfaces and APIs

### 3.1 `SniToolTip` struct and `TrayItem` additions

```cpp
// TrayItem.h

struct SniToolTip {
  QString icon_name;
  SniIconPixmapList icon_pixmap;
  QString title;
  QString description;
};

// New streaming operators + qDBusRegisterMetaType<SniToolTip>() in registerTrayMetaTypes()

struct TrayItem {
  // ... existing fields unchanged ...
  SniToolTip tooltip;  // NEW: fetched from ToolTip property
};
```

`SniToolTip` D-Bus type is `(sa(iiay)ss)` — a struct of (icon_name, pixmap_array, title, description). Streaming operators follow the same pattern as `SniIconPixel`.

### 3.2 `TrayModel` new invokables and roles

```cpp
// Additional Roles
enum Roles : uint16_t {
  // ... existing roles ...
  TooltipTitleRole,        // QString
  TooltipDescriptionRole,  // QString
  TooltipIconNameRole,     // QString
};

// New Q_INVOKABLE methods
Q_INVOKABLE void activate(const QString& key, int x, int y);
Q_INVOKABLE void secondaryActivate(const QString& key, int x, int y);
Q_INVOKABLE void scroll(const QString& key, int delta, const QString& orientation);
Q_INVOKABLE void openContextMenu(const QString& key, int x, int y);
```

Old `activate(key)` is replaced by `activate(key, x, y)`. Screen coordinates are passed from QML via `mapToGlobal(0, 0)`.

`openContextMenu` creates (or reuses) a `DbusMenuClient`, starts the async fetch, and connects `menuReady` to `TrayMenuSurface::show`.

### 3.3 `ItemSignalWatcher` (inner class in `TrayWatcher.cpp`)

```cpp
class ItemSignalWatcher : public QObject {
  Q_OBJECT
public:
  ItemSignalWatcher(QString key, TrayWatcher* owner, QObject* parent);

public Q_SLOTS:
  void onNewIcon();
  void onNewAttentionIcon();
  void onNewStatus(const QString& status);
  void onNewTitle();
  void onNewToolTip();
};
```

Constructed alongside `ItemPropWatcher` in `TrayWatcher::fetchItemProperties`. Connected to SNI item signals via `QDBusConnection::connect` with the specific signal names. Each slot calls the appropriate `TrayWatcher` method with the stored `key_`.

### 3.4 `TrayWatcher` new private methods

```cpp
private:
  void subscribeItemSignals(const QString& key, const QString& service, const QString& path);
  void fetchSingleProperty(const QString& key, const QString& service, const QString& path,
                           const QString& prop_name);
  void fetchToolTip(const QString& key, const QString& service, const QString& path);

  // Called by ItemSignalWatcher:
  void onItemDirectSignal(const QString& key, const QString& prop_name, const QVariant& value);
  void onItemNeedsPropertyFetch(const QString& key, const QString& prop_name);
  void onItemNeedsToolTipFetch(const QString& key);

  QHash<QString, ItemSignalWatcher*> signal_watchers_;  // NEW
```

### 3.5 `DbusMenuItem` struct

```cpp
// src/DbusMenuItem.h

struct DbusMenuItem {
  int id{0};
  QString label;
  QString type;             // "standard" | "separator"
  QString icon_name;
  bool enabled{true};
  bool visible{true};
  QString toggle_type;      // "checkmark" | "radio" | ""
  int toggle_state{-1};     // -1=indeterminate, 0=off, 1=on
  QList<DbusMenuItem> children;  // direct children (one level deep)
};
```

The full menu is a tree. `DbusMenuModel` exposes one depth level as a flat `QAbstractListModel` and holds child models lazily.

### 3.6 `DbusMenuModel` (QAbstractListModel)

```cpp
class DbusMenuModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT

public:
  enum Roles : uint16_t {
    IdRole = Qt::UserRole + 1,
    LabelRole,
    TypeRole,          // "standard" | "separator"
    IconNameRole,
    EnabledRole,
    VisibleRole,
    ToggleTypeRole,
    ToggleStateRole,
    HasSubmenuRole,    // bool — true when children non-empty
  };
  Q_ENUM(Roles)

  explicit DbusMenuModel(QList<DbusMenuItem> items, QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  // Returns child model for item at row (creates on first access).
  Q_INVOKABLE DbusMenuModel* submenuAt(int row);

private:
  QList<DbusMenuItem> items_;
  QHash<int, DbusMenuModel*> submenu_cache_;  // row → child model
};
```

`DbusMenuModel` is a plain `QAbstractListModel` — not a singleton. `DbusMenuClient` creates one per `GetLayout` response and hands ownership to `TrayMenuSurface` (which sets it as a property on the QML root).

### 3.7 `DbusMenuClient`

```cpp
class DbusMenuClient : public QObject {
  Q_OBJECT

public:
  explicit DbusMenuClient(QObject* parent = nullptr);

  // Starts async GetLayout, then AboutToShow, then emits menuReady.
  void open(const QString& service, const QString& object_path, int x, int y);
  void close();

  // Called from TrayMenuPopup to fire the Event call.
  Q_INVOKABLE void activateItem(int item_id);

Q_SIGNALS:
  void menuReady(DbusMenuModel* model, int x, int y);
  void menuFailed();
  void menuClosed();

private:
  void doGetLayout();
  void doAboutToShow(const QList<int>& root_ids, DbusMenuModel* model, int x, int y);
  static DbusMenuItem parseItem(const QDBusArgument& arg, int depth = 0);

  static constexpr int kMaxDepth = 5;
  static constexpr int kAboutToShowTimeoutMs = 1000;

  QDBusConnection bus_;
  QString service_;
  QString path_;
  int pending_x_{0};
  int pending_y_{0};
  bool menu_open_{false};
};
```

`DbusMenuClient` is created fresh for each right-click (re-fetch on every open, per §6.3 rationale). `TrayModel::openContextMenu` owns it and destroys it when the next right-click occurs. `close()` emits `menuClosed`, which lets `TrayMenuSurface` hide and release the active model.

### 3.8 `TrayMenuSurface`

```cpp
class TrayMenuSurface : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool menuVisible READ isMenuVisible NOTIFY menuVisibleChanged)

public:
  explicit TrayMenuSurface(QObject* parent = nullptr);
  ~TrayMenuSurface() override;

  [[nodiscard]] bool isMenuVisible() const { return menu_visible_; }

  // x, y: screen coords of the tray item's top-left corner.
  // model: owned by the surface while displayed; client remains owned by TrayModel.
  Q_INVOKABLE void show(const QString& screen_name, int x, int y, DbusMenuModel* model,
                        DbusMenuClient* client);
  Q_INVOKABLE void hide();
  Q_INVOKABLE void activateItem(int item_id);
  Q_INVOKABLE void close();

Q_SIGNALS:
  void menuVisibleChanged();

private:
  [[nodiscard]] bool ensureSurface(const QString& screen_name, int x, int y,
                                   DbusMenuModel* model);
  void destroySurface();

  LayerShell shell_;
  QQuickView* view_{nullptr};
  LayerSurface* surface_{nullptr};
  DbusMenuClient* active_client_{nullptr};
  DbusMenuModel* active_model_{nullptr};
  bool menu_visible_{false};
};
```

`TrayMenuSurface` follows the shell layer-surface pattern but accepts an `x, y` coordinate for positioning and a `DbusMenuModel*` that it injects into the loaded QML as the root item's `menuModel` property. It clamps the menu to the screen bounds, constrains height when the menu would overflow the screen, and forwards QML item activation to the currently active `DbusMenuClient`.

### 3.9 QML component properties

**`TraySection.qml`**

```
required property string barMonitorName
readonly property int maxCollapsedItems: 3     // was 4
readonly property var slotItems                 // computed var[], see §4
```

**`TrayItem.qml`** — new / changed properties

```
required property string tooltipTitle
required property string tooltipDescription
required property string tooltipIconName
```

All existing required properties remain unchanged.

**`TrayMenuPopup.qml`**

```
required property var menuModel          // DbusMenuModel*
required property var menuClient         // DbusMenuClient* for activateItem
property int depth: 0                    // 0 = root, 1 = first submenu, …
```

**`TrayMenuItem.qml`**

```
required property int itemId
required property string label
required property string type            // "standard" | "separator"
required property string iconName
required property bool enabled
required property bool hasSubmenu
required property string toggleType
required property int toggleState
```

---

## 4. Slot Priority Algorithm

The algorithm determines which items occupy the 3 visible slots and which are in overflow.

### 4.1 Definitions

- `allItems`: all items returned by `TrayModel` (the raw flat list).
- `visibleItems`: items where `status != "Passive"`.
- `urgentItems`: items where `status == "NeedsAttention"` (subset of `visibleItems`).
- `nonUrgentItems`: `visibleItems` minus `urgentItems`, in model order.
- `K = 3` (maxCollapsedItems).

### 4.2 Slot assignment (pure computation, runs in QML or C++ proxy)

```
function computeSlots(visibleItems, K):
  urgent = visibleItems.filter(i => i.status === "NeedsAttention")
  nonUrgent = visibleItems.filter(i => i.status !== "NeedsAttention")

  // Urgent items claim the leftmost slots first, in model order (first-come).
  visibleSlots = urgent.slice(0, K)                // up to K urgent items visible
  remaining = K - visibleSlots.length              // free slots after urgent
  visibleSlots = visibleSlots.concat(nonUrgent.slice(0, remaining))

  // Overflow queue: urgent extras first, then non-urgent extras, in model order.
  overflowItems = urgent.slice(K)
                  .concat(nonUrgent.slice(remaining))

  return { visibleSlots, overflowItems }
```

This is deterministic and side-effect free — no queued state is maintained across renders. The sort is recomputed every time `TrayModel` emits `dataChanged` or `rowsInserted`/`rowsRemoved`. QML's `Repeater` handles the diff.

### 4.3 Implementation location: QML `slotItems` computed property

```qml
// TraySection.qml
readonly property var slotItems: {
    var vis = []
    var urg = []
    var non = []
    for (var row = 0; row < TrayModel.rowCount(); row++) {
        var idx = TrayModel.index(row, 0)
        var status = TrayModel.data(idx, TrayModel.StatusRole)
        var key = TrayModel.data(idx, TrayModel.ItemKeyRole)
        if (status === "Passive") continue
        var entry = { key: key, row: row }
        if (status === "NeedsAttention") urg.push(entry)
        else non.push(entry)
    }
    var slots = urg.slice(0, root.maxCollapsedItems)
    var free = root.maxCollapsedItems - slots.length
    slots = slots.concat(non.slice(0, free))
    return slots
}
```

The Repeater binds to `slotItems` (a JS Array of `{key, row}` objects). Each delegate looks up its data from `TrayModel` by `row`. `TrayModel.dataChanged` triggers `slotItems` recalculation via `Connections`.

### 4.4 Overflow count

```qml
readonly property int overflowCount:
    TrayModel.rowCount() - passiveCount - slotItems.length
```

where `passiveCount` is tracked via the same scan.

### 4.5 Slide-in animation

When `slotItems` gains a new entry at the front (urgent preemption) or any position (non-urgent promotion), the `Repeater` creates the delegate. The delegate's `x` starts offset to the left:

```qml
// TrayItem.qml
property real slideOffset: 0.0
x: slideOffset
Component.onCompleted: {
    slideOffset = -root.size - itemSpacing
    slideAnim.start()
}
NumberAnimation on slideOffset {
    id: slideAnim
    to: 0
    duration: 100
    easing.type: Easing.OutCubic
}
```

This produces a left-to-right slide matching REQ-F-003 / REQ-F-008.

---

## 5. DBusMenu Parsing

### 5.1 `GetLayout` wire format

`com.canonical.dbusmenu.GetLayout(parentId, recursionDepth, propertyNames)` returns:

```
(u, (iia{sv}av))
  revision:uint32
  root_item:(id:int, properties:map, children:array_of_variant)
```

Each child in `children` is a `QDBusVariant` wrapping another `(iia{sv}av)` tuple — the same recursive structure. Qt unmarshals the outer tuple, but the `av` children require manual iteration of the `QDBusArgument`.

### 5.2 `DbusMenuClient::parseItem`

```cpp
DbusMenuItem DbusMenuClient::parseItem(const QDBusArgument& arg, int depth) {
  DbusMenuItem item;
  arg.beginStructure();
  arg >> item.id;

  // Properties map
  QVariantMap props;
  arg >> props;
  item.label       = props.value("label").toString();
  item.type        = props.value("type", "standard").toString();  // "separator" when set
  item.icon_name   = props.value("icon-name").toString();
  item.enabled     = props.value("enabled", true).toBool();
  item.visible     = props.value("visible", true).toBool();
  item.toggle_type  = props.value("toggle-type").toString();
  item.toggle_state = props.value("toggle-state", -1).toInt();

  // Children (recursive)
  if (depth < kMaxDepth) {
    arg.beginArray();
    while (!arg.atEnd()) {
      QVariant child_variant;
      arg >> child_variant;
      if (child_variant.canConvert<QDBusArgument>()) {
        item.children.append(parseItem(child_variant.value<QDBusArgument>(), depth + 1));
      }
    }
    arg.endArray();
  } else {
    // Consume but discard children beyond max depth
    QVariant ignored;
    arg >> ignored;
  }

  arg.endStructure();
  return item;
}
```

### 5.3 `AboutToShow` coordination

After parsing the root items list:

```cpp
// Collect IDs of root-level items
QList<int> root_ids;
for (const DbusMenuItem& child : root.children) {
  root_ids.append(child.id);
}

// Send AboutToShow for each in parallel; wait for all or 1-second timeout.
QSharedPointer<int> pending = QSharedPointer<int>::create(root_ids.size());
QTimer* timeout = new QTimer(this);
timeout->setSingleShot(true);
timeout->setInterval(kAboutToShowTimeoutMs);

auto finish = [this, model, x, y, timeout, pending]() {
  timeout->stop();
  timeout->deleteLater();
  emit menuReady(model, x, y);
};

connect(timeout, &QTimer::timeout, this, finish);
timeout->start();

for (int id : root_ids) {
  auto msg = QDBusMessage::createMethodCall(service_, path_,
      "com.canonical.dbusmenu", "AboutToShow");
  msg << id;
  auto* watcher = new QDBusPendingCallWatcher(bus_.asyncCall(msg, 5000), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
      [this, pending, finish](QDBusPendingCallWatcher* ptr) {
    ptr->deleteLater();
    if (--(*pending) == 0) finish();
  });
}

if (root_ids.isEmpty()) {
  finish();
}
```

### 5.4 Event dispatch

```cpp
void DbusMenuClient::activateItem(int item_id) {
  auto msg = QDBusMessage::createMethodCall(service_, path_,
      "com.canonical.dbusmenu", "Event");
  msg << item_id
      << QString("clicked")
      << QVariant::fromValue(QDBusVariant(QString{}))
      << static_cast<uint32_t>(QDateTime::currentSecsSinceEpoch());
  auto* pending = new QDBusPendingCallWatcher(bus_.asyncCall(msg, 2000), this);
  connect(pending, &QDBusPendingCallWatcher::finished, this,
      [this](QDBusPendingCallWatcher* ptr) {
    ptr->deleteLater();
    close();
  });
}
```

### 5.5 Property name mapping (`com.canonical.dbusmenu` → `DbusMenuItem`)

| D-Bus property key | `DbusMenuItem` field | Notes |
|---|---|---|
| `label` | `label` | Plain text; `_` prefix = mnemonic (strip for display) |
| `type` | `type` | `"separator"` → separator; absent/`"standard"` → normal item |
| `icon-name` | `icon_name` | XDG icon name |
| `enabled` | `enabled` | Default true when absent |
| `visible` | `visible` | Default true when absent |
| `toggle-type` | `toggle_type` | `"checkmark"`, `"radio"`, or empty |
| `toggle-state` | `toggle_state` | 0/1; -1 when absent |
| `children-display` | — | `"submenu"` = item has children (inferred from `children` array being non-empty) |

---

## 6. Key Decisions with Rationale

### 6.1 Slot priority in QML vs `QSortFilterProxyModel`

**Decision**: Implement slot priority as a computed `slotItems` JS array in QML (`TraySection.qml`), not as a `QSortFilterProxyModel` in C++.

**Rationale**: The slot count is small (≤ 3 visible), the model changes infrequently, and the ordering rule requires access to `status` which is a model role already exposed to QML. A QML computed property is simpler to test visually, does not require a new C++ class, and avoids the proxy model's lifecycle complexity (creating/destroying it, wiring it to the singleton `TrayModel`). The tradeoff is that QML JS arrays are re-scanned on every model change, but at ≤ ~20 tray items this is negligible.

**Alternatives considered**: `QSortFilterProxyModel` subclass with a custom `lessThan()` that places urgent items first — rejected because it requires a new C++ file, complicates the singleton/multi-engine wiring, and cannot easily drive the slide-in animation which needs to know whether a specific item *entered* the visible window (proxy models don't expose that directly).

### 6.2 Re-fetch DBusMenu on every right-click

**Decision**: `DbusMenuClient` calls `GetLayout` fresh on each right-click, discarding any previous result.

**Rationale**: DBusMenu items can change state between opens (enabled/disabled toggles, checked state, dynamic labels). Caching and diffing the tree requires tracking revision numbers (`com.canonical.dbusmenu` has a `Revision` field and `LayoutUpdated` signal) — that complexity is not worth it for an infrequently opened popup. Apps that want live updates typically call `LayoutUpdated` anyway; a fresh `GetLayout` is always correct.

**Alternatives considered**: Subscribe to `LayoutUpdated` signal and re-parse only on changes, keeping the `DbusMenuClient` alive. Rejected for initial implementation due to lifecycle complexity (when to subscribe, when to unsubscribe, how to handle rapid status changes).

### 6.3 `TrayMenuSurface` vs reusing `PopupSurface`

**Decision**: Create a new `TrayMenuSurface` class rather than extending `PopupSurface`.

**Rationale**: `PopupSurface` is hardcoded to a fixed size (`kPopupWidth=160, kPopupHeight=150`), loads `SessionPopup.qml`, anchors top-right, and has no mechanism to pass a data model. A context menu needs: dynamic sizing, bottom-of-item positioning (x/y from screen coords), and model injection. Extending `PopupSurface` would require changing its interface in ways that break the session popup. A sibling class with the same `LayerShell` pattern is cleaner.

**Alternatives considered**: Make `PopupSurface` generic with a QML source URL and a data property. Rejected because the positioning and sizing logic is fundamentally different between session menu (always top-right corner) and tray context menu (below the clicked item, width matches content).

### 6.4 `DbusMenuModel` as `QAbstractListModel` per depth level

**Decision**: `DbusMenuModel` models one level of the menu tree. Submenus are exposed via `submenuAt(row)` which returns a child `DbusMenuModel*`.

**Rationale**: QML `ListView` binds to flat models. A tree model (`QAbstractItemModel` with parent indexes) requires `TreeView` (Qt 6.3+, available) but is harder to style precisely. Flattening to one model per level and nesting `TrayMenuPopup` components keeps the QML straightforward and lets each submenu use the same `TrayMenuPopup.qml` recursively.

**Alternatives considered**: A single flat model with depth/indent fields (one QAbstractListModel for the whole tree). Rejected because submenus need independent show/hide animations and hit-testing that is awkward with a single flat list.

### 6.5 `ItemSignalWatcher` as a second inner class alongside `ItemPropWatcher`

**Decision**: Create `ItemSignalWatcher` as a second inner class in `TrayWatcher.cpp` rather than merging signal subscription into `ItemPropWatcher`.

**Rationale**: `ItemPropWatcher` handles `PropertiesChanged` (one generic slot). Direct SNI signals (`NewIcon`, `NewStatus`, etc.) require separate named slots. Merging them would give `ItemPropWatcher` a large interface unrelated to property changes. Two focused inner classes with single responsibilities are easier to read and test.

### 6.6 Screen coordinates via QML `mapToGlobal`

**Decision**: Pass screen coordinates for `Activate`, `SecondaryActivate`, and `openContextMenu` from QML using `mapToGlobal(0, 0)` on the tray item delegate.

**Rationale**: C++ has no direct access to the item's screen position without threading through model row → QQuickItem lookup, which is fragile. QML already knows the item's position in the visual hierarchy and `mapToGlobal` gives the correct Wayland surface-local coordinates translated to screen space. The invokables accept plain `int x, int y` so QML can call them naturally.

**Implementation note**: `mapToGlobal` returns coordinates in the shell's screen coordinate space. The QML delegate passes the tray item's top-left coordinate; `TrayMenuSurface` adds a small menu gap, converts the point into the target screen's local coordinates, clamps the x offset to keep the fixed-width menu on-screen, and clamps the height when the menu would extend beyond the bottom edge.

---

## 7. Alternatives Considered

### 7.1 `QSortFilterProxyModel` for urgent priority sorting

See §6.1. Rejected in favour of QML-side computed array. The proxy model approach would work correctly but adds a C++ class, requires singleton→proxy wiring in QML, and makes animation triggering harder.

### 7.2 Subscribe to `LayoutUpdated` for live DBusMenu

See §6.2. Rejected for initial implementation in favour of per-open re-fetch. Can be added later to handle menu state changes while a menu is open.

### 7.3 Reuse `PopupSurface` for context menu

See §6.3. Rejected because `PopupSurface` assumptions (fixed size, fixed anchor, hardcoded QML source) are incompatible with context menu requirements.

### 7.4 A single flat `DbusMenuModel` with depth column

See §6.4. Rejected in favour of per-level models. Flat model with depth would work for display but makes independent submenu hide/show logic awkward.

### 7.5 `QML Menu` (Qt Quick Controls) for context menu

Using `Menu` / `MenuItem` from `QtQuick.Controls` was considered. Rejected because: `Menu` uses native window positioning which is incompatible with the layer-shell surface model; it cannot be placed on a specific `wl_output` via `zwlr_layer_shell_v1`; and it has no mechanism to bind to a `DbusMenuModel`. A custom `TrayMenuPopup.qml` on a `TrayMenuSurface` layer window gives full styling control consistent with the rest of the bar.

### 7.6 `libdbusmenu-qt` third-party library

Rejected for the same reason as the original `topbar-tray` design: no external library dependencies beyond Qt. The `com.canonical.dbusmenu` protocol is well-specified and straightforward to implement directly.

### 7.7 Hyprland workspace urgency as supplement to SNI urgency

The ext-workspace protocol on Hyprland v0.55.2 does not send the urgent bit. Using workspace urgency as a supplement to SNI `NeedsAttention` was considered and rejected — the limitation is documented and SNI status alone is the correct signal per REQ-C-004.

---

## 8. Known Risks

### 8.1 Screen coordinate translation for layer-shell popup positioning

`TrayMenuSurface` must position the popup below the tray item. Layer-shell `set_margin` offsets from the anchor edge. The x coordinate from `mapToGlobal` can vary per monitor. **Risk**: the popup may appear at a slightly incorrect x position on multi-monitor setups or on monitors where the bar is not at x=0. **Mitigation**: derive the left margin from `globalX - screenOffsetX` where `screenOffsetX` is obtained from `QScreen::geometry().x()`, and clamp the result to the screen width minus the menu width and edge margin.

### 8.2 Apps that do not expose a `Menu` property

Some SNI items implement the visual icon/status but not the `Menu` D-Bus property. `Get("Menu")` on such items returns an error or an empty object path. **Risk**: right-click on such items silently fails or produces an error log. **Mitigation**: in `TrayModel::openContextMenu`, check that the returned object path is non-empty before constructing `DbusMenuClient`. If empty, fall back to calling `ContextMenu(x, y)` on the SNI item (a legacy method that tells the app to open its own menu natively). Log a `qCInfo` if neither is available.

### 8.3 Apps that ignore `AboutToShow`

The `AboutToShow` call is specified to let the application update the menu contents before display. Some apps do not implement it (the call returns immediately or errors). **Risk**: menu may display slightly stale contents. **Mitigation**: the 1-second timeout in `DbusMenuClient` ensures the menu always opens. Apps that don't implement `AboutToShow` will just have their timeout fire immediately on error reply.

### 8.4 DBusMenu `LayoutUpdated` signal while menu is open

If an app sends `LayoutUpdated` while the context menu is open, the current `DbusMenuModel` is stale. **Risk**: menu shows outdated items. **Mitigation**: not handled in initial implementation. A future follow-up can subscribe to `LayoutUpdated` on `DbusMenuClient` and call `GetLayout` again, then update `DbusMenuModel` in place.

### 8.5 Animation jank with rapid status changes

If an item rapidly toggles between `NeedsAttention` and `Active` (some apps flash urgency), the slot assignment recomputes on every change, potentially cancelling in-progress slide animations. **Risk**: visual flickering. **Mitigation**: add a 50 ms debounce on `slotItems` recomputation in QML using a `Timer { interval: 50 }`. This smooths rapid toggles at the cost of 50 ms latency on legitimate urgency changes.

### 8.6 `DbusMenuModel*` lifetime across QML/C++ boundary

`DbusMenuClient::menuReady` emits a raw `DbusMenuModel*`. `TrayMenuSurface` receives it and passes it to the QML root item. QML does not take ownership of raw C++ pointers. **Risk**: if the client is destroyed while QML is still rendering with the model, QML could read a dangling model. **Mitigation**: when shown, `TrayMenuSurface` reparents the model to itself and owns it until the surface is destroyed. `TrayModel` hides the surface before replacing or deleting the active `DbusMenuClient`.

### 8.7 `SniToolTip` D-Bus argument structure variability

The SNI spec defines `ToolTip` as `(sa(iiay)ss)` but some implementations omit the pixmap array or use a simplified string-only variant. **Risk**: argument unmarshalling throws a `QDBusError`. **Mitigation**: wrap `ToolTip` parsing in a try-block equivalent using `QDBusArgument`'s type checking (`arg.currentType()`) before each field, and fall back to empty struct on any parse failure.

### 8.8 `TrayImageProvider` thread safety (inherited risk)

Described in `topbar-tray/DESIGN.md §10.4`. New tooltip icon pixmaps go through the same `TrayImageProvider` path. The risk is unchanged; the same COW-on-assignment mitigation applies.

---

## 9. Final Validation

Final validation for this SDD ran on 2026-05-25:

- `task build`
- `task qml-lint`
- `QT_QPA_PLATFORM=offscreen build/tests/holonight_tests --gtest_filter='Tray*:DbusMenu*:QmlSmoke.*' --gtest_also_run_disabled_tests` — 48 passed
- Manual Wayland session validation: Bluetooth tray DBusMenu opens on-screen, follows shell styling, activates menu items without the null `QDBusVariant` marshalling error, and sits close to the top bar.

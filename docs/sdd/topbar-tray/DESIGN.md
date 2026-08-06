# DESIGN — topbar-tray: System Tray Widget

Feature: `topbar-tray` — StatusNotifierWatcher/Host D-Bus → TrayModel (C++) → TraySection (QML).

---

## 1. Component Overview

```
Session D-Bus
  │
  ├─ org.kde.StatusNotifierWatcher (registered by TrayWatcher)
  │    ├─ ItemRegistered signal → adds item
  │    └─ ItemUnregistered signal → removes item
  │
  │  (fallback: read existing watcher's RegisteredStatusNotifierItems)
  │
  ▼
TrayWatcher (C++ QObject)          — Registers as SNI Watcher + Host; owns item set
  │  addItem() / removeItem()
  ▼
TrayModel (C++ QAbstractListModel) — Thread-safe model of TrayItem records for QML Repeater
  │  itemsChanged NOTIFY
  ▼
TraySection.qml / TrayItem.qml
  Row of 22×22px icons; glow pulse on NeedsAttention items; 100ms fade in/out
```

### C++ files

| File | Purpose |
|------|---------|
| `src/TrayWatcher.h/.cpp` | Registers on D-Bus as `org.kde.StatusNotifierWatcher` + `org.kde.StatusNotifierHost-holonight-<pid>`. Tracks live items, discovers new items via signals or fallback read, handles service-disappearance cleanup. |
| `src/TrayItem.h/.cpp` | Value type + D-Bus property cache for one SNI item. Holds service name, object path, icon pixmap cache, status string. |
| `src/TrayModel.h/.cpp` | `QAbstractListModel` subclass. Exposes the list of `TrayItem` records to QML via named roles. |

### QML files

| File | Purpose |
|------|---------|
| `src/qml/Tray/TraySection.qml` | Top-level `BarSection` wrapper; contains the `Row` of icons. |
| `src/qml/Tray/TrayItem.qml` | Delegate for one tray icon: `Image` + `MultiEffect` glow + opacity `Behavior`. |

No new Wayland protocol XML is needed. SNI is pure session D-Bus.

---

## 2. C++ Class Designs

### 2.1 `TrayItem`

A plain value struct / lightweight QObject that holds all data for one SNI item. It is **not** exposed to QML directly — `TrayModel` maps its fields to roles.

```cpp
// src/TrayItem.h
#pragma once

#include <QDBusObjectPath>
#include <QImage>
#include <QString>

struct SniIconPixel {
  int width{0};
  int height{0};
  QByteArray data; // ARGB32, already byte-swapped for host endianness
};

// D-Bus type a(iiay) — one element in the IconPixmap array.
// Custom streaming operators (<<, >>) must be registered with qDBusRegisterMetaType.
using SniIconPixmapList = QList<SniIconPixel>;

struct TrayItem {
  // Identity
  QString service;           // e.g. "org.kde.kdeconnect"
  QString object_path;       // e.g. "/org/kde/kdeconnect/daemon"

  // Visual (from SNI Properties)
  QString icon_name;
  QString attention_icon_name;
  QString status;            // "Passive" | "Active" | "NeedsAttention"
  QString title;

  // Pixmap cache — decoded once from the wire ARGB32 data
  QImage icon_pixmap_cache;
  QImage attention_pixmap_cache;

  // Helpers
  [[nodiscard]] bool isPassive() const    { return status == QLatin1String("Passive"); }
  [[nodiscard]] bool needsAttention() const { return status == QLatin1String("NeedsAttention"); }
};
```

**Key detail — `SniIconPixel` D-Bus streaming:**

The SNI `IconPixmap` property has D-Bus type `a(iiay)`. Qt's D-Bus machinery cannot unmarshal this automatically; custom `operator<<` and `operator>>` for `QDBusArgument` must be provided and the type registered with `qDBusRegisterMetaType<SniIconPixmapList>()` (called once at program startup, e.g. in `TrayWatcher` constructor).

```cpp
// In TrayItem.cpp

QDBusArgument& operator<<(QDBusArgument& arg, const SniIconPixel& pix) {
  arg.beginStructure();
  arg << pix.width << pix.height << pix.data;
  arg.endStructure();
  return arg;
}

const QDBusArgument& operator>>(const QDBusArgument& arg, SniIconPixel& pix) {
  arg.beginStructure();
  arg >> pix.width >> pix.height >> pix.data;
  arg.endStructure();
  return arg;
}

// Register in TrayWatcher constructor:
//   qDBusRegisterMetaType<SniIconPixel>();
//   qDBusRegisterMetaType<SniIconPixmapList>();
```

---

### 2.2 `TrayWatcher`

**Responsibilities:**

- Register `org.kde.StatusNotifierHost-holonight-<pid>` on the session bus.
- Attempt to register as `org.kde.StatusNotifierWatcher`; fall back to reading an existing watcher's `RegisteredStatusNotifierItems`.
- When acting as watcher: expose the SNI Watcher D-Bus interface, handle `RegisterStatusNotifierItem` calls, emit `ItemRegistered`/`ItemUnregistered`, maintain `RegisteredStatusNotifierItems` property.
- For each live item: fetch SNI properties, build a `TrayItem`, push to `TrayModel`.
- Watch each item's service name via `QDBusServiceWatcher`; call `removeItem()` on disappearance.
- Subscribe to `org.freedesktop.DBus.Properties.PropertiesChanged` on each item's object path; call `updateItemProperties()` on receipt.

**Key fields:**

```cpp
private:
  QDBusConnection bus_;                        // sessionBus()
  TrayModel* model_;                           // not owned — injected
  QMap<QString, TrayItem> items_;              // key: service + ":" + path
  QDBusServiceWatcher* service_watcher_;       // watches all tracked services
  bool is_watcher_{false};                     // true if we own the watcher name
  QStringList registered_items_;              // mirrors SNI Watcher property
```

**Key methods:**

```cpp
public:
  explicit TrayWatcher(TrayModel* model, QObject* parent = nullptr);
  ~TrayWatcher() override;

private:
  void registerHost();
  void tryRegisterWatcher();
  void fallbackReadItems(const QString& existing_watcher_service);
  void addItem(const QString& service_and_path);
  void removeItem(const QString& service_and_path);
  void fetchItemProperties(const QString& key, const QString& service, const QString& path);
  void updateItemProperties(const QString& key, const QVariantMap& changed);
  void watchService(const QString& service);
  static QString makeItemKey(const QString& service, const QString& path);

private Q_SLOTS:
  // Slot for QDBusServiceWatcher::serviceUnregistered
  void onServiceUnregistered(const QString& service);
  // Slot for org.kde.StatusNotifierWatcher ItemRegistered (when we are watcher)
  void onItemRegistered(const QString& service_and_path);
  // Slot for org.kde.StatusNotifierWatcher ItemUnregistered
  void onItemUnregistered(const QString& service_and_path);
  // Slot for PropertiesChanged on an SNI item
  void onItemPropertiesChanged(const QString& interface,
                               const QVariantMap& changed,
                               const QStringList& invalidated);
```

**D-Bus Watcher service registration:**

```cpp
void TrayWatcher::tryRegisterWatcher() {
  auto reply = bus_.interface()->registerService(
      QStringLiteral("org.kde.StatusNotifierWatcher"),
      QDBusConnectionInterface::ReplaceExistingService,
      QDBusConnectionInterface::AllowReplacement);

  if (reply.value() == QDBusConnectionInterface::ServiceRegistered) {
    is_watcher_ = true;
    // Register the watcher object at the canonical path
    bus_.registerObject(QStringLiteral("/StatusNotifierWatcher"), this,
                        QDBusConnection::ExportAllSlots |
                        QDBusConnection::ExportAllSignals |
                        QDBusConnection::ExportAllProperties);
    qCInfo(trayLog) << "Registered as org.kde.StatusNotifierWatcher";
  } else {
    qCInfo(trayLog) << "Watcher already running; falling back to read";
    fallbackReadItems(reply.value() == QDBusConnectionInterface::ServiceNotRegistered
                      ? QString{} : "org.kde.StatusNotifierWatcher");
  }
}
```

**Watcher D-Bus interface — methods TrayWatcher must export as Q_INVOKABLE slots:**

When `is_watcher_ == true`, the following slots are exported as D-Bus methods on `/StatusNotifierWatcher`:

```cpp
public Q_SLOTS:  // exported via QDBusConnection::ExportAllSlots
  void RegisterStatusNotifierItem(const QString& service_or_path);
  void RegisterStatusNotifierHost(const QString& service);

Q_SIGNALS:       // exported via QDBusConnection::ExportAllSignals
  void ItemRegistered(const QString& service_and_path);
  void ItemUnregistered(const QString& service_and_path);
  void StatusNotifierHostRegistered();
  void StatusNotifierItemRegistered(const QString& service_and_path); // alias used by some clients

Q_PROPERTY exported:
  QStringList RegisteredStatusNotifierItems READ registeredItems
  bool        IsStatusNotifierHostRegistered READ isHostRegistered
  int         ProtocolVersion READ protocolVersion
```

**Fallback when watcher already exists:**

```cpp
void TrayWatcher::fallbackReadItems(const QString& watcher_service) {
  if (watcher_service.isEmpty()) return;
  QDBusInterface iface(watcher_service,
                       QStringLiteral("/StatusNotifierWatcher"),
                       QStringLiteral("org.kde.StatusNotifierWatcher"),
                       bus_);
  // Read existing items
  QVariant v = iface.property("RegisteredStatusNotifierItems");
  for (const QString& entry : v.toStringList())
    addItem(entry);
  // Subscribe to future registrations
  bus_.connect(watcher_service,
               QStringLiteral("/StatusNotifierWatcher"),
               QStringLiteral("org.kde.StatusNotifierWatcher"),
               QStringLiteral("ItemRegistered"),
               this, SLOT(onItemRegistered(QString)));
  bus_.connect(watcher_service,
               QStringLiteral("/StatusNotifierWatcher"),
               QStringLiteral("org.kde.StatusNotifierWatcher"),
               QStringLiteral("ItemUnregistered"),
               this, SLOT(onItemUnregistered(QString)));
}
```

---

### 2.3 `TrayModel`

**Responsibilities:**

- `QAbstractListModel` subclass that holds a `QList<TrayItem>`.
- Exposes roles to QML: `service`, `objectPath`, `iconName`, `attentionIconName`, `status`, `iconPixmapUrl` (a synthetic `image://tray/<key>` URL), `title`.
- Provides `addItem(TrayItem)`, `removeItem(QString key)`, `updateItem(QString key, TrayItem updated)` methods called by `TrayWatcher`.
- Implements a custom `QQuickImageProvider` subclass (`TrayImageProvider`) that serves the cached `QImage` from model items via the `image://tray/<key>` URL scheme.

```cpp
// src/TrayModel.h
#pragma once

#include "TrayItem.h"

#include <QAbstractListModel>
#include <QQuickImageProvider>
#include <QtQml/qqml.h>

class TrayModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  enum Roles : uint16_t {
    ServiceRole      = Qt::UserRole + 1,
    ObjectPathRole,
    IconNameRole,
    AttentionIconNameRole,
    StatusRole,
    IconPixmapUrlRole,   // "image://tray/<key>" or empty string
    TitleRole,
    ItemKeyRole,         // service + ":" + path, used as QML model identifier
  };
  Q_ENUM(Roles)

  explicit TrayModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  void addItem(const TrayItem& item, const QString& key);
  void removeItem(const QString& key);
  void updateItem(const QString& key, const TrayItem& updated);

  // Called by TrayImageProvider
  [[nodiscard]] QImage imageForKey(const QString& key) const;

private:
  QList<QPair<QString, TrayItem>> rows_; // (key, item) ordered list
  QHash<QString, int> index_by_key_;     // key → row index for O(1) lookup
};

class TrayImageProvider : public QQuickImageProvider {
public:
  explicit TrayImageProvider(TrayModel* model);
  QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
  TrayModel* model_; // not owned
};
```

The `TrayImageProvider` is registered with the QML engine in `main.cpp`:

```cpp
engine->addImageProvider(QStringLiteral("tray"), new TrayImageProvider(model));
```

Because there are multiple `QQmlEngine` instances (one per monitor), the provider must be added to each engine. `LayerShellManager` creates engines — see §7 for CMake / main.cpp wiring.

---

## 3. Data Flow

### 3.1 Startup

```
main.cpp
  └─ new TrayModel(...)
  └─ new TrayWatcher(model, ...)
       └─ qDBusRegisterMetaType<SniIconPixel>()
       └─ qDBusRegisterMetaType<SniIconPixmapList>()
       └─ registerHost()
            └─ bus_.registerService("org.kde.StatusNotifierHost-holonight-<pid>")
       └─ tryRegisterWatcher()
            ├─ SUCCESS → is_watcher_=true, register DBus object
            └─ FALLBACK → fallbackReadItems()
                 └─ for each entry: addItem(entry)
```

### 3.2 New item arrives (watcher path)

```
SNI client → D-Bus call: RegisterStatusNotifierItem("org.example.app")
  └─ TrayWatcher::RegisterStatusNotifierItem(service_or_path)
       └─ normalise to "service:/path" form
       └─ append to registered_items_
       └─ emit ItemRegistered(key)
       └─ addItem(key)
```

### 3.3 `addItem(key)` pipeline

```
addItem(key)
  └─ parse service + path from key
  └─ watchService(service)   ← QDBusServiceWatcher
  └─ fetchItemProperties(key, service, path)
       └─ QDBusInterface iface(service, path, "org.kde.StatusNotifierItem", bus_)
       └─ Async GetAll("org.kde.StatusNotifierItem") — 5s timeout
            └─ callback: build TrayItem from QVariantMap
                 ├─ status = props["Status"].toString()
                 ├─ icon_name = props["IconName"].toString()
                 ├─ attention_icon_name = props["AttentionIconName"].toString()
                 ├─ icon_pixmap_cache = decodePixmap(props["IconPixmap"])
                 ├─ attention_pixmap_cache = decodePixmap(props["AttentionIconPixmap"])
                 └─ model_->addItem(item, key)
  └─ subscribe PropertiesChanged on item path
```

### 3.4 QML rendering

```
TrayModel (QAbstractListModel)
  └─ TraySection.qml  Repeater { model: TrayModel }
       └─ TrayItem.qml  delegate
            ├─ status == "Passive"         → visible: false
            ├─ status == "Active"           → Image{ source: iconUrl }
            └─ status == "NeedsAttention"  → Image + MultiEffect glow pulse
```

The QML `Repeater` model is bound to `TrayModel` directly (the singleton). Because `TrayModel` is a `QAbstractListModel`, QML's Repeater responds automatically to `rowsInserted`, `rowsRemoved`, and `dataChanged` signals — no manual refresh signal needed.

### 3.5 Item removal

```
QDBusServiceWatcher::serviceUnregistered(service)
  └─ onServiceUnregistered(service)
       └─ find all keys with matching service prefix
       └─ for each key:
            └─ model_->removeItem(key)
            └─ items_.remove(key)
       └─ service_watcher_->removeWatchedService(service)
```

---

## 4. D-Bus Interface Details

### 4.1 `org.kde.StatusNotifierWatcher` — interface we implement (when watcher)

**Methods exposed as D-Bus slots:**

| Method | Signature | Behaviour |
|--------|-----------|-----------|
| `RegisterStatusNotifierItem` | `(s) → void` | Normalise argument (service-only or service:path), append to `registered_items_`, emit `ItemRegistered`, call `addItem`. |
| `RegisterStatusNotifierHost` | `(s) → void` | Record the host service; emit `StatusNotifierHostRegistered`. |

**Properties:**

| Property | D-Bus type | Notes |
|----------|-----------|-------|
| `RegisteredStatusNotifierItems` | `as` | Live list of `"service:/path"` strings. |
| `IsStatusNotifierHostRegistered` | `b` | `true` once we've called `registerHost()`. |
| `ProtocolVersion` | `i` | Return `0`. |

**Signals:**

| Signal | Signature |
|--------|-----------|
| `ItemRegistered` | `(s)` |
| `ItemUnregistered` | `(s)` |
| `StatusNotifierHostRegistered` | `()` |

### 4.2 `org.kde.StatusNotifierItem` — interface we call (per item)

**Properties we read (via `GetAll` at startup and `PropertiesChanged` incremental):**

| Property | D-Bus type | Notes |
|----------|-----------|-------|
| `Status` | `s` | `"Passive"` / `"Active"` / `"NeedsAttention"` |
| `IconName` | `s` | XDG icon name; empty string if none. |
| `IconPixmap` | `a(iiay)` | ARGB32 pixmap list. Requires custom marshalling. |
| `AttentionIconName` | `s` | Icon name for NeedsAttention state. |
| `AttentionIconPixmap` | `a(iiay)` | Pixmap for NeedsAttention. |
| `Title` | `s` | Tooltip / accessible name. |

**Methods we call:**

| Method | Signature | Notes |
|--------|-----------|-------|
| `Activate` | `(ii) → void` | Called with `(0, 0)` on left-click. Fire-and-forget async call. |

**Signals we subscribe to (per item path):**

| Signal | Interface | Notes |
|--------|-----------|-------|
| `PropertiesChanged` | `org.freedesktop.DBus.Properties` | Standard interface; carries changed property map. |

### 4.3 Service name normalisation

An SNI client may call `RegisterStatusNotifierItem` with:
- `"org.example.app"` — service name only; object path defaults to `/StatusNotifierItem`.
- `"org.example.app:/custom/path"` — colon-separated service:path pair.

```cpp
static QString normaliseKey(const QString& raw) {
  if (raw.contains(QLatin1Char('/')))
    return raw; // already "service:/path"
  return raw + QStringLiteral(":/StatusNotifierItem");
}
```

---

## 5. Icon Rendering Pipeline

### 5.1 Decision logic (per `TrayItem`)

```
renderIcon(item):
  if status == NeedsAttention:
    1. if attention_icon_name non-empty:
         try QIcon::fromTheme(attention_icon_name) → use if valid
    2. if attention_pixmap_cache non-null:
         use attention_pixmap_cache
  // Fall through to regular icon
  if icon_name non-empty:
    1. try QIcon::fromTheme(icon_name) → use if valid
  if icon_pixmap_cache non-null:
    use icon_pixmap_cache
  // No icon: item renders as blank (no visible slot)
```

In QML, the image source is either:
- `"image://icon/<icon_name>"` — uses Qt's built-in icon image provider (XDG theme lookup).
- `"image://tray/<key>"` — served by `TrayImageProvider` from the cached `QImage`.

The model role `IconPixmapUrlRole` contains the `image://tray/<key>` URL if a pixmap was cached, otherwise an empty string. QML logic picks the right source:

```qml
source: {
  if (model.status === "NeedsAttention" && model.attentionIconName !== "")
    return "image://icon/" + model.attentionIconName
  if (model.status === "NeedsAttention" && model.iconPixmapUrl !== "")
    return model.iconPixmapUrl   // attention pixmap stored under key+":attention"
  if (model.iconName !== "")
    return "image://icon/" + model.iconName
  return model.iconPixmapUrl     // regular pixmap
}
```

### 5.2 Pixmap decoding and byte-swap

The SNI wire format for `IconPixmap` is `a(iiay)` where each element is `(width, height, data)`. The `data` byte array is raw ARGB32 in **network (big-endian) byte order**. On little-endian hosts (x86/x86-64/ARM64) the channel order must be reversed per 4-byte pixel.

```cpp
// In TrayWatcher.cpp or a free function in TrayItem.cpp
static QImage decodePixmap(const SniIconPixmapList& pixmaps, int target_size = 22) {
  if (pixmaps.isEmpty()) return {};

  // Prefer the variant closest to target_size × target_size
  const SniIconPixel* best = &pixmaps.front();
  for (const auto& p : pixmaps) {
    if (std::abs(p.width - target_size) < std::abs(best->width - target_size))
      best = &p;
  }

  QImage img(best->width, best->height, QImage::Format_ARGB32);
  const int pixel_count = best->width * best->height;
  const uchar* src = reinterpret_cast<const uchar*>(best->data.constData());
  uint32_t* dst = reinterpret_cast<uint32_t*>(img.bits());

  for (int i = 0; i < pixel_count; ++i) {
    // Wire is big-endian ARGB: bytes [A, R, G, B]
    // QImage::Format_ARGB32 on LE is stored as 0xAARRGGBB in native uint32
    dst[i] = (uint32_t(src[0]) << 24)   // A
            | (uint32_t(src[1]) << 16)   // R
            | (uint32_t(src[2]) <<  8)   // G
            |  uint32_t(src[3]);         // B
    src += 4;
  }

  return img.scaled(target_size, target_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
```

### 5.3 Caching

- Decoded `QImage` is stored in `TrayItem::icon_pixmap_cache` / `attention_pixmap_cache`.
- Decoding runs once inside `fetchItemProperties()` callback.
- If `PropertiesChanged` reports `IconPixmap` or `AttentionIconPixmap` changed, `updateItemProperties()` re-decodes and stores the new image; `TrayModel::updateItem()` is called which emits `dataChanged` for the `IconPixmapUrlRole` row.
- `TrayImageProvider::requestImage(id, ...)` looks up the `QImage` by key from `TrayModel::imageForKey(id)`. It is called on the render thread — `TrayModel` must not hold a mutex here in practice (Qt's model/view framework guarantees main-thread model updates), but the `TrayImageProvider` must be made thread-safe if `TrayWatcher` ever updates from a non-main thread (it does not in this design).

---

## 6. QML Component Design

### 6.1 `TraySection.qml`

```qml
pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell

BarSection {
    id: root

    readonly property int itemSize: 22
    readonly property int itemSpacing: 6

    // Hide the entire section when no visible items exist
    implicitWidth: visibleRow.implicitWidth > 0
                   ? visibleRow.implicitWidth + 8
                   : 0

    Row {
        id: visibleRow
        anchors.verticalCenter: parent.verticalCenter
        x: 4
        spacing: root.itemSpacing

        Repeater {
            model: TrayModel
            delegate: TrayItem {
                required property string itemKey
                required property string iconName
                required property string attentionIconName
                required property string iconPixmapUrl
                required property string status
                required property string title

                size: root.itemSize
            }
        }
    }
}
```

The `implicitWidth` reacts to the `Row`'s implicit width, which sums visible delegate widths. Delegates with `visible: false` (Passive items) contribute `0` to the row's implicit width automatically because Qt sets an invisible item's implicit contribution to zero in a `Row`.

### 6.2 `TrayItem.qml`

```qml
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import Holonight

Item {
    id: root

    required property int    size
    required property string iconName
    required property string attentionIconName
    required property string iconPixmapUrl
    required property string status
    required property string title

    width:   root.size
    height:  root.size

    visible: root.status !== "Passive"

    opacity: 0
    Behavior on opacity {
        NumberAnimation { duration: 100; easing.type: Easing.OutCubic }
    }

    Component.onCompleted: opacity = (root.status !== "Passive") ? 1.0 : 0.0

    onStatusChanged: opacity = (root.status !== "Passive") ? 1.0 : 0.0

    // ── Glow (NeedsAttention) — declared BEFORE icon so icon renders on top ──
    MultiEffect {
        id: glowEffect
        source: iconImage
        anchors.fill: iconImage
        visible:          root.status === "NeedsAttention"
        autoPaddingEnabled: true
        shadowEnabled:    true
        shadowColor:      HoloniightPalette.cyan
        shadowBlur:       0.7
        shadowOpacity:    glowPulse.value
        shadowScale:      1.12
        shadowHorizontalOffset: 0
        shadowVerticalOffset:   0
    }

    // ── Icon image ──
    Image {
        id: iconImage
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        smooth: true

        source: {
            if (root.status === "NeedsAttention" && root.attentionIconName !== "")
                return "image://icon/" + root.attentionIconName
            if (root.iconName !== "")
                return "image://icon/" + root.iconName
            return root.iconPixmapUrl
        }
    }

    // ── Glow pulse animation ──
    SequentialAnimation {
        id: glowPulse
        running: root.status === "NeedsAttention"
        loops:   Animation.Infinite

        property real value: 0.0

        NumberAnimation { target: glowPulse; property: "value"; to: 0.9; duration: 750; easing.type: Easing.InOutSine }
        NumberAnimation { target: glowPulse; property: "value"; to: 0.2; duration: 750; easing.type: Easing.InOutSine }
    }

    // ── Click handler ──
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onClicked: TrayModel.activate(root.itemKey)
    }

    ToolTip.visible: hoverArea.containsMouse && root.title !== ""
    ToolTip.text:   root.title

    HoverHandler { id: hoverArea }
}
```

**MultiEffect z-order note:** `MultiEffect` is declared *before* `iconImage` in the file, so it renders behind the icon. This is consistent with the pattern documented in CLAUDE.md and used in `WorkspacePill.qml`.

**NeedsAttention attention pixmap:** When `status === "NeedsAttention"` and `attentionIconName` is empty but `iconPixmapUrl` contains an attention variant, the `source` binding needs the model to expose a dedicated role for it (`AttentionPixmapUrlRole`). A simpler approach: the model stores the attention image under key `"<key>:attention"` in the image provider, and the model role `AttentionIconPixmapUrl` returns `"image://tray/<key>:attention"` when available. The QML `source` binding becomes:

```qml
source: {
  if (root.status === "NeedsAttention") {
    if (root.attentionIconName !== "")    return "image://icon/" + root.attentionIconName
    if (root.attentionPixmapUrl !== "")  return root.attentionPixmapUrl
  }
  if (root.iconName !== "")              return "image://icon/" + root.iconName
  return root.iconPixmapUrl
}
```

This requires adding `AttentionPixmapUrlRole` to `TrayModel::Roles` and passing the attention image to `TrayImageProvider` under the `"<key>:attention"` id.

### 6.3 `TrayModel.activate()` Q_INVOKABLE

QML calls `TrayModel.activate(itemKey)` on click. The model delegates to `TrayWatcher` or holds a direct `QDBusInterface` per item. Recommended: `TrayModel` keeps a `QHash<QString, QPair<QString,QString>> service_and_path_by_key_` and calls `Activate(0, 0)` asynchronously:

```cpp
Q_INVOKABLE void activate(const QString& key) {
  auto it = service_and_path_by_key_.find(key);
  if (it == service_and_path_by_key_.end()) return;
  QDBusInterface iface(it->first, it->second,
                       QStringLiteral("org.kde.StatusNotifierItem"),
                       QDBusConnection::sessionBus());
  iface.asyncCall(QStringLiteral("Activate"), 0, 0);
}
```

---

## 7. CMake Wiring

No new Wayland protocol XML is needed.

### 7.1 New source files

Add to `qt6_add_executable(holonight-shell ...)`:

```cmake
src/TrayItem.h
src/TrayItem.cpp
src/TrayWatcher.h
src/TrayWatcher.cpp
src/TrayModel.h
src/TrayModel.cpp
```

### 7.2 New QML files

Add `set_source_files_properties` entries:

```cmake
set_source_files_properties(src/qml/Tray/TraySection.qml
    PROPERTIES QT_RESOURCE_ALIAS "Tray/TraySection.qml")
set_source_files_properties(src/qml/Tray/TrayItem.qml
    PROPERTIES QT_RESOURCE_ALIAS "Tray/TrayItem.qml")
```

Add to `qt6_add_qml_module(holonight-shell ... QML_FILES ...)`:

```cmake
src/qml/Tray/TraySection.qml
src/qml/Tray/TrayItem.qml
```

### 7.3 No new `target_link_libraries` entries

`Qt6::DBus` is already linked. No new dependencies are introduced.

### 7.4 `main.cpp` additions

```cpp
#include "TrayModel.h"
#include "TrayWatcher.h"

// In main():
auto* tray_model   = new TrayModel(&app);
auto* tray_watcher = new TrayWatcher(tray_model, &app);
Q_UNUSED(tray_watcher)

QQmlEngine::setObjectOwnership(tray_model, QQmlEngine::CppOwnership);
qmlRegisterSingletonType<TrayModel>("HolonightShell", 1, 0, "TrayModel",
    [tray_model](QQmlEngine*, QJSEngine*) -> QObject* { return tray_model; });
```

The `TrayImageProvider` must be added to every `QQmlEngine` instance. `LayerShellManager` constructs one engine per monitor. Add a hook in `LayerShellManager::createEngine()` (or wherever engines are initialised):

```cpp
engine->addImageProvider(QStringLiteral("tray"),
                         new TrayImageProvider(tray_model));
```

`TrayImageProvider` takes a raw pointer to `TrayModel`; the provider is owned by the engine and destroyed with it. Since `TrayModel` outlives all engines (it is parented to `&app`), this is safe.

Alternatively, pass `tray_model` to `LayerShellManager` and add the provider there. Exact wiring depends on `LayerShellManager`'s constructor signature — leave this as a detail for implementation.

### 7.5 `TopBar.qml` insertion

```qml
// Between BatterySection and StatusSection:
BatterySection {
    Layout.alignment: Qt.AlignVCenter
}

TraySection {
    Layout.alignment: Qt.AlignVCenter
}

StatusSection {
    Layout.alignment: Qt.AlignVCenter
}
```

---

## 8. Key Decisions with Rationale

### 8.1 `QDBusServiceWatcher` vs `NameOwnerChanged` signal

`QDBusServiceWatcher` is used for service-disappearance detection (REQ-F-006). It wraps `NameOwnerChanged` internally but filters by service name, avoiding the need to demultiplex a global signal. Each service is added with `addWatchedService()` when the item first appears and removed with `removeWatchedService()` when the item is gone. A single watcher instance is reused across all items.

### 8.2 `QAbstractListModel` + `Repeater` vs dynamic component creation

`QAbstractListModel` bound to a `Repeater` is the idiomatic Qt Quick approach. It avoids manual component lifecycle management, correctly handles bulk insertions/deletions, and integrates with Qt's model change signals. Dynamic `Component.createObject()` would require manual tracking and is error-prone during rapid item churn (e.g., an app crash while items are being added). `Repeater` is sufficient because there is no overflow cap for MVP (REQ-F-018) and the item count is expected to be small (<20).

### 8.3 `TrayImageProvider` (custom image provider) vs `QQuickPaintedItem`

A `QQuickImageProvider` decouples icon storage (C++ `QImage`) from rendering (`Image` QML element) without creating per-icon C++ objects visible in QML. It allows the same declarative `Image { source: "image://tray/..." }` pattern used everywhere else in the bar. A `QQuickPaintedItem` subclass would require per-item C++ objects registered as QML types, complicating the model/delegate boundary.

### 8.4 `GetAll` at item registration vs lazy property fetch

`GetAll` at registration time fetches all properties in one D-Bus roundtrip per item. Lazy per-property `Get` calls would multiply roundtrips with no benefit, since the icon and status are always needed immediately. `GetAll` also avoids a TOCTOU window where the item could be removed before individual `Get` calls complete.

### 8.5 Single `QDBusServiceWatcher` vs per-item watcher

A single `QDBusServiceWatcher` with `QDBusServiceWatcher::WatchForUnregistration` mode is used for all services. It is cheaper than creating one watcher per service and Qt's implementation batches the D-Bus monitoring calls internally. When a service disappears, the `serviceUnregistered(service)` signal carries the service name; `TrayWatcher` then removes all items whose key starts with `service + ":"`.

### 8.6 Watcher registration with `ReplaceExistingService`

Using `QDBusConnectionInterface::ReplaceExistingService` means we take over as watcher even if another watcher is running. This matches the behaviour of KDE's system tray and is expected by SNI clients. If this causes conflicts with a running DE tray daemon in testing, the flag can be changed to `QDBusConnectionInterface::DontAllowReplacement` with the fallback path always taken — but this would mean we never receive `RegisterStatusNotifierItem` calls from new apps.

### 8.7 Glow pulse implemented as `SequentialAnimation` on a property

The pulse animation drives a local `property real value` on the `SequentialAnimation` object, which `MultiEffect.shadowOpacity` binds to. This avoids a `SequentialAnimation` targeting a property on a different Item (which can cause binding loops) and keeps the animation logic self-contained within `TrayItem.qml`.

---

## 9. Alternatives Considered

### 9.1 `libdbusmenu-qt` or `libappindicator`

Using a third-party library would reduce boilerplate around the SNI protocol. Rejected because: (a) the project has no external library dependencies beyond Qt and libpulse; (b) these libraries pull in GTK/GLib transitive dependencies; (c) the SNI interface is small enough to implement directly.

### 9.2 KStatusNotifierItem / Plasma integration

Depending on `libksysguard` or Plasma's tray implementation was considered and rejected for the same reasons as 9.1 — unnecessary dependency weight for a standalone Wayland bar.

### 9.3 Polling instead of `PropertiesChanged` subscription

Some older SNI implementations do not emit `PropertiesChanged`. Polling every N seconds was considered as a fallback. Rejected for MVP — polling would add complexity and power cost. If a specific app is found not to update correctly, a targeted workaround (e.g., re-fetch on `NewIcon` signal if exposed) can be added incrementally.

### 9.4 `QML_ELEMENT` on `TrayItem` struct

Making `TrayItem` a `QML_ELEMENT` `QObject` and exposing items directly as QML objects was considered. Rejected: it couples the model representation to QML's object system, makes it harder to batch updates, and creates unnecessary QObject overhead for what is essentially a data record. The `QAbstractListModel` + roles pattern (same as `WorkspaceModel`) is better aligned with the existing codebase.

### 9.5 `image://icon/` for all icons, ignoring pixmap fallback

`QIcon::fromTheme()` is reliable for apps that ship proper XDG icons. However, many system-tray apps (particularly Electron-based or Java apps) only provide `IconPixmap` and leave `IconName` empty. Skipping pixmap rendering would make those apps invisible in the tray. The two-path approach in §5 is necessary for broad compatibility.

---

## 10. Known Risks

### 10.1 SNI watcher conflict with a running DE

If the user's desktop environment (e.g., KDE Plasma, GNOME with AppIndicator extension) already owns `org.kde.StatusNotifierWatcher`, the `tryRegisterWatcher` call will fail or acquire the name from Plasma's process (with `ReplaceExistingService`). Stealing the name from Plasma's watcher may cause Plasma's tray to stop working. **Mitigation:** during development, test on a bare Hyprland session without Plasma running. Consider adding a `--no-watcher` flag or config option to force fallback mode.

### 10.2 `PropertiesChanged` not emitted by all SNI implementations

The D-Bus spec requires `PropertiesChanged` on the `org.freedesktop.DBus.Properties` interface, but some legacy SNI implementations (e.g., older versions of Skype, Wine system tray icons) use SNI-specific signals (`NewIcon`, `NewStatus`) instead. REQ-F-019 covers `PropertiesChanged` only. **Mitigation:** subscribe to `NewIcon` and `NewStatus` signals as well in a follow-up, or accept that status updates from legacy clients require the service to disappear and reappear.

### 10.3 D-Bus type registration timing

`qDBusRegisterMetaType<SniIconPixel>()` and `qDBusRegisterMetaType<SniIconPixmapList>()` must be called before any D-Bus unmarshalling of these types. Placing the calls in `TrayWatcher`'s constructor (before any D-Bus work is done) is safe, but if another code path triggers SNI unmarshalling before `TrayWatcher` is constructed, a runtime warning will occur. **Mitigation:** call both `qDBusRegisterMetaType` lines in `main.cpp` before any Qt object construction.

### 10.4 `TrayImageProvider` thread safety

`TrayModel` is updated on the main thread. `TrayImageProvider::requestImage` is called from Qt's render thread. Reading `QImage` from `TrayModel::imageForKey()` while the model is being updated on the main thread is a data race. **Mitigation:** images are stored as `QImage` value types in `TrayItem`, and `QImage` is implicitly shared (COW). As long as the render thread only reads the image and the main thread only replaces the stored `QImage` atomically (which `QList` assignment does), the race window is narrow. For full safety, protect `imageForKey` and model update paths with a `QReadWriteLock`, or store images in a separate `QHash<QString, QImage>` with atomic pointer swaps. This is a known risk to be addressed in a follow-up if tearing is observed.

### 10.5 Icon flicker on `PropertiesChanged` icon update

When `IconPixmap` changes and the image provider serves a new `QImage`, the QML `Image` element must reload its source. Because `source` is a static URL (`"image://tray/<key>"`), changing the underlying image without changing the URL will not trigger a reload. **Mitigation:** append a monotonically increasing version counter to the URL: `"image://tray/<key>?v=<n>"`. Increment `n` in `TrayModel::updateItem()` when the pixmap changes. This forces QML to request the updated image.

### 10.6 No menu support

Many SNI items expose a `dbusmenu` context menu via the `Menu` property. This design does not implement right-click menus. **Risk:** some apps (e.g., network-manager-applet, Telegram) rely on the context menu as their primary interaction mechanism. Left-click `Activate` is the only interaction in MVP. Context menus can be added in a follow-up session using `com.canonical.dbusmenu`.

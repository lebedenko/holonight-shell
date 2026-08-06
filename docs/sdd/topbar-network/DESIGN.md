# Network Widget Design — `topbar-network`

Feature: `topbar-network` — NetworkManager D-Bus → NetworkService (C++) → NetworkSection (QML).

---

## 1. Component Overview

```
org.freedesktop.NetworkManager (system D-Bus)
  │
  │  StateChanged signal
  │  PropertiesChanged on AP path
  ▼
NetworkService  (C++ QObject singleton)
  Q_PROPERTY: available, online, type, ssid, strength, vpnActive
  │
  │  NOTIFY signals bound in QML
  ▼
NetworkSection.qml  (extends BarSection)
  Row: [vpnIcon] [primaryIcon] [ssidLabel] [signalIcon] [strengthLabel]
```

`NetworkService` owns all D-Bus interactions. QML is purely declarative — it reads
properties and emits no commands back to the service.

---

## 2. D-Bus Object Path Chain

| Step | Service | Path | Interface | Key property / signal |
|------|---------|------|-----------|-----------------------|
| 1 | `org.freedesktop.NetworkManager` | `/org/freedesktop/NetworkManager` | `org.freedesktop.NetworkManager` | `State` (uint), `PrimaryConnection` (o), `ActiveConnections` (ao) |
| 1a | same | same | same | `StateChanged(uint newState)` signal |
| 2 | same | `<PrimaryConnection path>` | `org.freedesktop.NetworkManager.Connection.Active` | `Type` (s: `"802-11-wireless"` / `"802-3-ethernet"`), `Devices` (ao) |
| 3 | same | `<device path>` | `org.freedesktop.NetworkManager.Device.Wireless` | `ActiveAccessPoint` (o) |
| 4 | same | `<AP path>` | `org.freedesktop.NetworkManager.AccessPoint` | `Ssid` (ay → QByteArray), `Strength` (y → uchar) |
| 4a | same | `<AP path>` | `org.freedesktop.DBus.Properties` | `PropertiesChanged` signal |
| VPN | same | each path in `ActiveConnections` | `org.freedesktop.NetworkManager.Connection.Active` | `Type == "vpn"` |

NM state value `70` = `NM_STATE_CONNECTED_GLOBAL` → `online = true`. All other
states → `online = false`.

---

## 3. Data Flow

### 3.1 Initial Startup

```
constructor
  └─ check NM on system bus
       ├─ not present → available=false, start poll timer (2s)
       └─ present     → available=true
                           └─ subscribe StateChanged
                           └─ queryNmState()
                                └─ queryPrimaryConnection()
                                     └─ queryConnectionType()  (→ sets type)
                                          ├─ Wired → scanVpn(); done
                                          └─ WiFi  → queryWifiDevice()
                                                       └─ queryAccessPoint()
                                                            └─ sets ssid, strength
                                                       └─ scanVpn()
```

### 3.2 StateChanged Signal

Any `StateChanged` fires `onNmStateChanged(uint newState)`:

```
onNmStateChanged(newState)
  ├─ setOnline(newState == 70)
  └─ queryPrimaryConnection()   ← full re-query cascade (same as startup)
```

Full re-query is chosen over incremental updates for simplicity: connection
changes always invalidate type, device, AP, and VPN state simultaneously.

### 3.3 AP PropertiesChanged (strength-only update)

```
onApPropertiesChanged(iface, changed, invalidated)
  └─ if "Strength" in changed
       └─ setStrength(clamp(changed["Strength"].value<uchar>(), 0, 100))
```

This subscription is set on the specific AP D-Bus path. When `queryAccessPoint`
resolves a new AP path it unsubscribes from the old path and subscribes to the
new one (see §5 for stale-call handling).

---

## 4. NetworkService Class Design

### 4.1 Header sketch

```cpp
class NetworkService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool available  READ available  NOTIFY availableChanged)
  Q_PROPERTY(bool online     READ online     NOTIFY onlineChanged)
  Q_PROPERTY(int  type       READ type       NOTIFY typeChanged)
  Q_PROPERTY(bool vpnActive  READ vpnActive  NOTIFY vpnActiveChanged)
  Q_PROPERTY(QString ssid    READ ssid       NOTIFY ssidChanged)
  Q_PROPERTY(int  strength   READ strength   NOTIFY strengthChanged)

 public:
  enum ConnectionType { None = 0, WiFi = 1, Wired = 2 };
  Q_ENUM(ConnectionType)

  explicit NetworkService(QObject* parent = nullptr);

 Q_SIGNALS:
  void availableChanged();
  void onlineChanged();
  void typeChanged();
  void vpnActiveChanged();
  void ssidChanged();
  void strengthChanged();

 private Q_SLOTS:
  void onNmStateChanged(uint newState);
  void onApPropertiesChanged(const QString& iface,
                             const QVariantMap& changed,
                             const QStringList& invalidated);
  void onPollTimer();

 private:
  // Query cascade
  void queryNmState();
  void queryPrimaryConnection();
  void queryConnectionType(const QString& activePath);
  void queryWifiDevice(const QString& devicePath);
  void queryAccessPoint(const QString& apPath);
  void scanVpn();

  // Setters (emit NOTIFY only on value change)
  void setAvailable(bool v);
  void setOnline(bool v);
  void setType(int v);
  void setVpnActive(bool v);
  void setSsid(const QString& v);
  void setStrength(int v);

  // D-Bus helpers
  QDBusPendingCallWatcher* asyncGet(const QString& service,
                                    const QString& path,
                                    const QString& iface,
                                    const QString& prop);

  // State
  bool available_{false};
  bool online_{false};
  int  type_{0};
  bool vpn_active_{false};
  QString ssid_;
  int  strength_{0};

  // Tracking — needed to detect stale async responses and to
  // unsubscribe PropertiesChanged from the old AP path.
  QString current_primary_path_;
  QString current_ap_path_;

  QTimer* poll_timer_{nullptr};   // used when available==false
};
```

### 4.2 Member variables rationale

| Variable | Purpose |
|----------|---------|
| `current_primary_path_` | Guard: discard async results from a previous connection that resolved after a new `StateChanged` arrived |
| `current_ap_path_` | Track which AP path we have subscribed `PropertiesChanged` on, so we can unsubscribe before switching |
| `poll_timer_` | 2 s retry when NM is absent; stopped as soon as `available` becomes `true` |

---

## 5. Async Call Pattern

All D-Bus reads use `QDBusAbstractInterface::asyncCall()` wrapped in
`QDBusPendingCallWatcher`. No synchronous `call()` is used (REQ-NF-NS-012).

### 5.1 Generic helper

```cpp
// Returns a watcher connected to finished(); caller connects its own lambda.
QDBusPendingCallWatcher* NetworkService::asyncGet(
    const QString& service, const QString& path,
    const QString& iface,   const QString& prop)
{
    QDBusInterface props(service, path,
                         "org.freedesktop.DBus.Properties",
                         QDBusConnection::systemBus());
    auto call    = props.asyncCall("Get", iface, prop);
    auto watcher = new QDBusPendingCallWatcher(call, this);
    return watcher;
}
```

### 5.2 Chained async calls

Each step of the query chain is initiated in the `finished` slot of the
previous watcher. Example (simplified):

```cpp
void NetworkService::queryPrimaryConnection() {
    // Capture snapshot of primary path at call time.
    const QString snapPrimary = current_primary_path_;

    auto* w = asyncGet(kNmService, kNmPath, kNmIface, "PrimaryConnection");
    connect(w, &QDBusPendingCallWatcher::finished, this,
            [this, snapPrimary](QDBusPendingCallWatcher* watcher) {
        watcher->deleteLater();
        QDBusPendingReply<QDBusVariant> reply = *watcher;
        if (!reply.isValid()) {
            qCWarning(lcNetwork) << "PrimaryConnection get failed:"
                                 << reply.error().message();
            return;
        }
        const QString path =
            reply.value().variant().value<QDBusObjectPath>().path();

        // Stale guard: another StateChanged may have arrived while we waited.
        if (path != current_primary_path_) return;

        queryConnectionType(path);
    });
}
```

### 5.3 Stale response mitigation

Before each async result is consumed, the callback compares the path it was
launched with against the *current* `current_primary_path_`. If they differ,
the result is discarded silently. This prevents a slow response to an old
connection from overwriting state set by a faster response to the new one.

For AP-level calls, `current_ap_path_` serves the same role.

### 5.4 AP subscription management

```cpp
void NetworkService::queryAccessPoint(const QString& newApPath) {
    if (newApPath == current_ap_path_) return;  // no change

    // Unsubscribe old AP
    if (!current_ap_path_.isEmpty()) {
        QDBusConnection::systemBus().disconnect(
            kNmService, current_ap_path_,
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            this, SLOT(onApPropertiesChanged(QString,QVariantMap,QStringList)));
    }

    current_ap_path_ = newApPath;

    // Subscribe new AP
    QDBusConnection::systemBus().connect(
        kNmService, current_ap_path_,
        "org.freedesktop.DBus.Properties", "PropertiesChanged",
        this, SLOT(onApPropertiesChanged(QString,QVariantMap,QStringList)));

    // Fetch initial Ssid + Strength
    // ... two async calls, same pattern as above
}
```

---

## 6. QML Layout — NetworkSection.qml

### 6.1 Element breakdown

```
BarSection  (id: root)
│
├─ readonly property string iconColor:
│    NetworkService.online ? HoloniightPalette.success
│                           : HoloniightPalette.error
│
├─ readonly property string wifiTier:        // "weak"|"ok"|"good"|"excellent"
│    NetworkService.strength <= 25 ? "weak"
│  : NetworkService.strength <= 50 ? "ok"
│  : NetworkService.strength <= 75 ? "good"
│                                  : "excellent"
│
├─ readonly property string primaryIconName:
│    !NetworkService.available            ? "network-offline-symbolic"
│  : NetworkService.type == 0            ? "network-offline-symbolic"
│  : NetworkService.type == 2            ? "network-wired-symbolic"
│  : "network-wireless-signal-" + wifiTier + "-symbolic"
│
├─ property int displayStrength: NetworkService.strength
│    Behavior on displayStrength {
│        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
│    }
│
└─ Row (id: row, spacing: 4)
     ├─ Image  vpnIcon    source: "image://icon/network-vpn-symbolic"
     │                    visible: NetworkService.vpnActive
     │
     ├─ Image  primaryIcon  source: "image://icon/" + primaryIconName
     │
     ├─ Text   ssidLabel  text: NetworkService.ssid
     │                    visible: NetworkService.type == 1
     │                          && NetworkService.ssid != ""
     │                    font.family: "JetBrains Mono"; font.pixelSize: 13
     │
     ├─ Image  signalIcon  source: "image://icon/" + primaryIconName
     │                     visible: NetworkService.type == 1
     │         (reuses same computed name; both icons are driven by wifiTier)
     │
     └─ Text   strengthLabel  text: displayStrength + "%"
                               visible: NetworkService.type == 1
                               font.family: "JetBrains Mono"; font.pixelSize: 13
```

> **Design note on `primaryIcon` vs `signalIcon`**: the spec calls for a
> separate signal-bars icon between SSID and percentage. Both `primaryIcon`
> and `signalIcon` resolve to the same `network-wireless-signal-*` name when
> WiFi is active — they are distinct `Image` elements so that `primaryIcon`
> also handles the wired/offline states while `signalIcon` is WiFi-only.

### 6.2 implicitWidth

```qml
implicitWidth: {
    let w = primaryIcon.width + 8  // always visible
    if (NetworkService.vpnActive)   w += vpnIcon.width  + row.spacing
    if (ssidLabel.visible)          w += ssidLabel.contentWidth + row.spacing
    if (NetworkService.type == 1) {
        w += signalIcon.width + row.spacing
        w += strengthLabel.contentWidth + row.spacing
    }
    return w
}
```

`TextMetrics` with `text: "100%"` can pin `strengthLabel.width` to a fixed
value (same pattern as `AudioSection`) to prevent layout jitter as the animated
number changes digit count.

### 6.3 Color application

```qml
readonly property color iconColor:
    (NetworkService.online && NetworkService.available)
        ? HoloniightPalette.success
        : HoloniightPalette.error

// applied to all Image / Text children:
Image { ... ColorOverlay { color: root.iconColor } }
Text  { color: root.iconColor }
```

---

## 7. Key Decisions with Rationale

| Decision | Rationale |
|----------|-----------|
| Full re-query on every `StateChanged` | NM state changes (roaming, WiFi→wired switch) simultaneously invalidate primary path, device, AP, and VPN list. Incremental tracking requires handling partial-state transitions; full re-query is simpler and the round-trip is under 100 ms on local socket. |
| `uchar` strength needs no scaling | NM exposes `Strength` as a byte 0–100 already. No multiply needed (contrast: UPower `Percentage` is a double 0.0–100.0 that must be passed through `qRound`). |
| SSID as `QByteArray` → `QString::fromUtf8()` | The `Ssid` property is a raw byte array (`ay`) in D-Bus; it is not null-terminated and may contain non-ASCII bytes. `fromUtf8()` handles multibyte characters; an empty or replacement-character result is normalised to `""`. |
| 2 s poll when `available == false` | NM may not be running (minimal containers, early boot). Polling avoids subscribing to service-appearance notifications (which require `AddMatch` on the bus daemon) and is consistent with the BatteryService recovery pattern. |
| `QDBusPendingCallWatcher` per-call | Keeps each async operation self-contained with its own lambda and lifetime; avoids shared reply-queue state. |
| Separate `current_primary_path_` guard | Multiple rapid `StateChanged` events (e.g. DHCP negotiation) can queue several in-flight async chains. The guard ensures only the chain matching the most recent primary path writes to properties. |

---

## 8. Alternatives Considered

### Qt Network (`QNetworkInformation`) vs D-Bus

`QNetworkInformation` (Qt 6.1+) provides online/type detection without writing
D-Bus code. It does **not** expose SSID, signal strength, or VPN status.
Rejected: too coarse for the widget's requirements.

### Synchronous D-Bus calls

`QDBusInterface::call()` is simpler to chain but blocks the Qt event loop for
the duration of the IPC round-trip (typically 1–10 ms, occasionally 100+ ms on
a busy system). Rejected per REQ-NF-NS-012.

### QML-level D-Bus binding (`QtDBus` QML module)

Qt's QML D-Bus bindings would let QML directly read NM properties without a
C++ service. Rejected because:
- The NM query requires a 4-step path-chaining cascade that is difficult to
  express declaratively.
- SSID sanitisation and strength clamping belong in C++.
- Consistent with the established pattern (`BatteryService`, `AudioService`).

### Separate worker thread for D-Bus

Not needed: `QDBusPendingCallWatcher` delivers results on the main thread via
the event loop without blocking. A dedicated thread would add synchronization
overhead for no gain.

---

## 9. Known Risks

### 9.1 Stale path from mid-query connection change

**Risk**: `queryPrimaryConnection` dispatches an async call. Before it returns,
a new `StateChanged` fires with a different `PrimaryConnection` path. The first
callback arrives late and writes stale type/device/AP into properties.

**Mitigation**: Each async lambda captures `current_primary_path_` at dispatch
time and discards its result if `current_primary_path_` has changed by the time
the callback fires (§5.3). Same pattern applied at the AP level with
`current_ap_path_`.

### 9.2 NM object disappears mid-chain

**Risk**: NM crashes between steps 2 and 3 of the query cascade. Later
`asyncGet` calls return an error reply.

**Mitigation**: Every `QDBusPendingCallWatcher::finished` slot checks
`reply.isValid()`; on failure it logs and returns without updating properties.
The 2 s poll timer re-establishes the subscription on NM restart.

### 9.3 SSID contains non-UTF-8 bytes

**Risk**: A maliciously named or legacy AP sends a byte sequence that is not
valid UTF-8. `QString::fromUtf8()` replaces invalid sequences with the Unicode
replacement character `U+FFFD`.

**Mitigation**: After `fromUtf8()`, check `ssid.contains(QChar::ReplacementCharacter)`
and replace with `""`, satisfying REQ-F-ERR-002.

### 9.4 Strength `uchar` overflow on unusual NM builds

**Risk**: NM documentation says 0–100 but a buggy NM patch or custom build
might return 255 as "unknown".

**Mitigation**: Clamp: `std::clamp(static_cast<int>(raw), 0, 100)` before
calling `setStrength()` (REQ-F-ERR-005).

### 9.5 VPN scan cost at every StateChanged

**Risk**: `scanVpn` iterates all `ActiveConnections` (each requires a D-Bus
property read). On a system with many active connections this might add latency.

**Mitigation**: VPN scan uses a single `GetAll` on each active connection path;
typical desktop systems have 1–3 entries. Acceptable at current scale; can be
optimised later by subscribing to `ActiveConnections` property changes instead.

---

## 10. CMake Integration

No new libraries. `Qt6::DBus` is already linked for `BatteryService`.

```cmake
# In the existing target_sources block:
src/NetworkService.h
src/NetworkService.cpp

# In the QML sources list with QT_RESOURCE_ALIAS:
set_source_files_properties(
    src/qml/Topbar/NetworkSection.qml
    PROPERTIES QT_RESOURCE_ALIAS "Topbar/NetworkSection.qml"
)
```

`NetworkService` is registered in `main.cpp` alongside the other singletons:

```cpp
auto* networkService = new NetworkService(&app);
qmlRegisterSingletonInstance("HolonightShell", 1, 0,
                              "NetworkService", networkService);
```

`TopBar.qml` insertion point: between `AudioSection` and `BatterySection`.

```qml
AudioSection   { Layout.alignment: Qt.AlignVCenter }
NetworkSection { Layout.alignment: Qt.AlignVCenter }  // ← new
BatterySection { Layout.alignment: Qt.AlignVCenter }
```

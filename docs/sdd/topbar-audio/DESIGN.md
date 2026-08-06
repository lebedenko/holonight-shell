# DESIGN — topbar-audio

## Components

### `AudioService` (C++)

`AudioService : QObject` is a singleton that owns the entire D-Bus lifecycle and exposes three QML-readable properties plus one invokable method.

**Header: `src/AudioService.h`**

```cpp
class AudioService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(int volume   READ volume   NOTIFY volumeChanged)
  Q_PROPERTY(bool muted   READ muted    NOTIFY mutedChanged)
  Q_PROPERTY(bool available READ available NOTIFY availableChanged)

 public:
  explicit AudioService(QObject* parent = nullptr);
  ~AudioService() override = default;
  // delete copy/move (same pattern as BatteryService)

  [[nodiscard]] int  volume()    const { return volume_; }
  [[nodiscard]] bool muted()     const { return muted_; }
  [[nodiscard]] bool available() const { return available_; }

  Q_INVOKABLE void setVolume(int percent);

 Q_SIGNALS:
  void volumeChanged();
  void mutedChanged();
  void availableChanged();

 private Q_SLOTS:
  void onFallbackSinkUpdated(const QDBusObjectPath& sinkPath);
  void onVolumeUpdated(const QList<uint>& volumes);
  void onMuteUpdated(bool muted);

 private:
  void connectToPulse();
  void readFallbackSink();
  void subscribeSink(const QDBusObjectPath& sinkPath);
  void unsubscribeSink();
  void readSinkProperties(const QDBusObjectPath& sinkPath);
  void setVolume_(int value);
  void setMuted_(bool value);
  void setAvailable_(bool value);

  QDBusConnection pa_conn_{"holonight-pulse"};  // second connection, initially invalid
  QDBusObjectPath current_sink_;
  int  volume_{0};
  bool muted_{false};
  bool available_{false};
};
```

**QML singleton registration in `src/main.cpp`** (identical pattern to `BatteryService`):

```cpp
qmlRegisterSingletonType<AudioService>(
    "HolonightShell", 1, 0, "AudioService",
    [](QQmlEngine*, QJSEngine*) -> QObject* {
        return new AudioService();
    });
```

### `AudioSection.qml` (QML)

Located at `src/qml/Topbar/AudioSection.qml`. Root element is `BarSection`. Displays a system icon and a fixed-width percentage label. Collapses to zero width when `AudioService.available == false`. Handles scroll-wheel events to call `AudioService.setVolume()`.

---

## Data Flow

```
PipeWire/PulseAudio daemon
        |
        | D-Bus (UNIX socket via Address)
        v
  AudioService (C++)
        |  volumeChanged / mutedChanged / availableChanged
        v
  AudioSection.qml
        |  WheelHandler -> setVolume(percent)
        v
  AudioService.setVolume()
        |  SetSinkVolumes D-Bus call
        v
  PipeWire/PulseAudio daemon
```

---

## D-Bus Connection Lifecycle

AudioService uses two sequential D-Bus connections.

### Step 1 — Server lookup (session bus)

In the `AudioService` constructor:

```cpp
static constexpr auto kLookupService = "org.PulseAudio.ServerLookup1";
static constexpr auto kLookupPath    = "/org/pulseaudio/server_lookup1";
static constexpr auto kLookupIface   = "org.PulseAudio.ServerLookup1";

QDBusConnection session = QDBusConnection::sessionBus();
QDBusInterface lookup(kLookupService, kLookupPath, kLookupIface, session);
QVariant addr = lookup.property("Address");   // e.g. "unix:path=/run/user/1000/pulse/dbus-socket"
```

If `lookup.lastError().isValid()` or `addr` is empty/invalid: call `setAvailable_(false)` and return. Do not attempt further D-Bus work.

### Step 2 — Core1 connection (dedicated socket)

```cpp
pa_conn_ = QDBusConnection::connectToBus(addr.toString(), "holonight-pulse");
```

`QDBusConnection::connectToBus` accepts a full D-Bus address string as returned by the `Address` property; no additional URL encoding is required for standard `unix:path=...` addresses.

If `!pa_conn_.isConnected()`: log warning, `setAvailable_(false)`, return.

On success: call `readFallbackSink()`, subscribe to `FallbackSinkUpdated`, then `setAvailable_(true)`.

**Failure modes and outcomes:**

| Condition | Outcome |
|---|---|
| Session bus unavailable | `available = false`, silent |
| `org.PulseAudio.ServerLookup1` not present | `available = false`, warning log |
| `Address` property empty | `available = false`, warning log |
| `connectToBus` fails | `available = false`, warning log |
| `FallbackSink` property empty | `available = false`, warning log |

The constructor never blocks or retries. If PulseAudio starts after the shell launches, the widget stays collapsed until the next application restart. (Reconnection logic is out of scope for this feature.)

---

## Sink Tracking Lifecycle

### Startup

1. `readFallbackSink()` calls `GetAll` (or `property("FallbackSink")`) on `org.PulseAudio.Core1` at path `/org/pulseaudio/core1` interface `org.PulseAudio.Core1`.
2. The returned value is a `QDBusObjectPath`, e.g. `/org/pulseaudio/core1/sink0`.
3. `subscribeSink(path)` is called immediately.

### subscribeSink / unsubscribeSink

```cpp
// D-Bus constants
static constexpr auto kCore1Service = "org.PulseAudio.Core1";
static constexpr auto kCore1Path    = "/org/pulseaudio/core1";
static constexpr auto kCore1Iface   = "org.PulseAudio.Core1";
static constexpr auto kSinkIface    = "org.PulseAudio.Core1.Sink";
```

`subscribeSink(path)`:
- Stores path in `current_sink_`.
- Calls `readSinkProperties(path)` — reads `Volume` (type `au`, i.e. `QList<uint>`) and `Mute` (type `b`) from interface `org.PulseAudio.Core1.Sink` via `org.freedesktop.DBus.Properties.GetAll`.
- Connects `pa_conn_.connect(kCore1Service, path.path(), kSinkIface, "VolumeUpdated", this, SLOT(onVolumeUpdated(QList<uint>)))`.
- Connects `pa_conn_.connect(kCore1Service, path.path(), kSinkIface, "MuteUpdated", this, SLOT(onMuteUpdated(bool)))`.

`unsubscribeSink()`:
- Calls `pa_conn_.disconnect(kCore1Service, current_sink_.path(), kSinkIface, "VolumeUpdated", this, SLOT(onVolumeUpdated(QList<uint>)))`.
- Calls `pa_conn_.disconnect(kCore1Service, current_sink_.path(), kSinkIface, "MuteUpdated", this, SLOT(onMuteUpdated(bool)))`.
- Clears `current_sink_`.

### FallbackSinkUpdated

Subscribed during startup:

```cpp
pa_conn_.connect(kCore1Service, kCore1Path, kCore1Iface,
                 "FallbackSinkUpdated", this,
                 SLOT(onFallbackSinkUpdated(QDBusObjectPath)));
```

`onFallbackSinkUpdated(const QDBusObjectPath& sinkPath)`:
1. `unsubscribeSink()` — disconnects signals from old sink.
2. `subscribeSink(sinkPath)` — connects to new sink, reads its properties.

### Property reads — volume conversion

`readSinkProperties` extracts the `Volume` property as `QList<uint>`. If the list is empty, logs a warning and returns. Otherwise:

```cpp
uint sum = 0;
for (uint v : volumes) { sum += v; }
uint mean = sum / volumes.size();
setVolume_(qRound(mean / 65536.0 * 100));
```

The `Mute` property is read as `bool` directly:

```cpp
setMuted_(map.value("Mute").toBool());
```

---

## Volume Marshalling

### Reading

`Volume` arrives as D-Bus type `au` (array of uint32). `QDBusReply<QList<uint>>` or extraction from `QVariantMap` via `.value("Volume").value<QList<uint>>()` both work. No extra type registration is needed for reading — Qt automatically maps `au` to `QList<QVariant>` in a `QVariantMap`, so cast with:

```cpp
QVariant raw = map.value("Volume");
QDBusArgument arg = raw.value<QDBusArgument>();
QList<uint> volumes;
arg >> volumes;
```

Alternatively, register the meta-type once in `AudioService` constructor or `main.cpp`:

```cpp
qDBusRegisterMetaType<QList<uint>>();
```

After registration, `raw.value<QList<uint>>()` works directly.

### Writing — SetSinkVolumes

The PulseAudio Core1 method signature is:

```
org.PulseAudio.Core1.SetSinkVolumes(o: sink_path, au: volumes)
```

Invoked as:

```cpp
void AudioService::setVolume(int percent) {
    percent = std::clamp(percent, 0, 100);
    if (current_sink_.path().isEmpty()) return;

    uint raw = static_cast<uint>(qRound(percent / 100.0 * 65536));

    // Build a channel-count-length array; re-read current volume to get channel count
    QList<uint> vols(channel_count_, raw);   // channel_count_ cached from last readSinkProperties

    QDBusMessage msg = QDBusMessage::createMethodCall(
        kCore1Service, current_sink_.path(), kSinkIface, "SetVolume");
    QDBusArgument arg;
    arg.beginArray(QMetaType::fromType<uint>().id());
    for (uint v : vols) { arg << v; }
    arg.endArray();
    msg << QVariant::fromValue(arg);
    pa_conn_.asyncCall(msg);
}
```

An alternative is to use `QDBusInterface::asyncCall("SetVolume", QVariant::fromValue(arg))` — both are equivalent. Use `asyncCall` to avoid blocking the GUI thread.

**Channel count**: cache the channel count when reading sink properties into a private member `channel_count_` (type `int`, default 2). Most sinks have 2 channels (stereo). Using the cached count avoids a round-trip D-Bus call on every scroll event.

---

## QML Layout

### AudioSection.qml structure

```qml
import QtQuick
import HolonightShell
import Holonight

BarSection {
    id: root

    property int stepSize: 5

    TextMetrics {
        id: labelMetrics
        font.family: "JetBrains Mono"
        font.pixelSize: 13
        text: "100%"
    }

    readonly property int iconSize: 16
    readonly property int spacing: 6

    implicitWidth: AudioService.available
        ? iconSize + spacing + labelMetrics.width
        : 0

    Image {
        id: icon
        anchors.verticalCenter: parent.verticalCenter
        width: root.iconSize
        height: root.iconSize
        source: AudioService.muted
            ? "image://icon/audio-volume-muted-symbolic"
            : AudioService.volume >= 67
                ? "image://icon/audio-volume-high-symbolic"
                : AudioService.volume >= 34
                    ? "image://icon/audio-volume-medium-symbolic"
                    : "image://icon/audio-volume-low-symbolic"
    }

    Text {
        id: volumeLabel
        anchors {
            left: icon.right
            leftMargin: root.spacing
            verticalCenter: parent.verticalCenter
        }
        width: labelMetrics.width
        text: AudioService.muted ? "" : displayVolume + "%"
        color: HoloniightPalette.onSurface
        font.family: "JetBrains Mono"
        font.pixelSize: 13

        property int displayVolume: AudioService.volume
        Behavior on displayVolume {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
    }

    WheelHandler {
        onWheel: (event) => {
            if (event.angleDelta.y > 0)
                AudioService.setVolume(Math.min(100, AudioService.volume + root.stepSize))
            else
                AudioService.setVolume(Math.max(0, AudioService.volume - root.stepSize))
        }
    }
}
```

**Key layout decisions:**

- `TextMetrics` is evaluated at component-creation time against the string `"100%"`. Because `"100%"` is the widest possible value (four characters in JetBrains Mono 13px), `labelMetrics.width` is constant for the lifetime of the component — this is the mechanism for preventing layout jitter (REQ-F-011).
- The `implicitWidth` expression collapses to 0 when `AudioService.available == false`, which causes the parent `RowLayout` to remove the section entirely (REQ-F-008).
- `displayVolume` is an intermediate `int` property that `NumberAnimation` targets. Binding `text` to `displayVolume + "%"` instead of `AudioService.volume + "%"` ensures the text tracks the animated integer, not the raw service value (REQ-F-013).
- `WheelHandler` is declared without an explicit `target`; it implicitly captures events within the parent `BarSection` bounds.
- Icon source is a computed expression; Qt's image provider `image://icon/...` looks up icons from the system XDG icon theme at runtime. No bundled SVG assets are needed.

---

## CMake Integration

### 1. Add source files to the executable target

In `qt6_add_executable(holonight-shell ...)`, append:

```cmake
src/AudioService.h
src/AudioService.cpp
```

### 2. Register QML resource aliases

After the existing `set_source_files_properties` block:

```cmake
set_source_files_properties(src/qml/Topbar/AudioSection.qml
    PROPERTIES QT_RESOURCE_ALIAS "Topbar/AudioSection.qml")
```

### 3. Add QML file to the module

Inside `qt6_add_qml_module(holonight-shell ... QML_FILES ...)`, append:

```cmake
src/qml/Topbar/AudioSection.qml
```

### 4. Linker — no changes needed

`Qt6::DBus` is already present in `target_link_libraries`. No new `find_package` or `pkg_check_modules` calls are required.

---

## Integration Point in TopBar.qml

`AudioSection` is placed between `BatterySection` and `StatusSection` (the rightmost section group):

```qml
BatterySection {
    Layout.alignment: Qt.AlignVCenter
}

AudioSection {
    Layout.alignment: Qt.AlignVCenter
}

StatusSection {
    Layout.alignment: Qt.AlignVCenter
}
```

Because `AudioSection.implicitWidth` collapses to 0 when unavailable, no conditional visibility logic is needed in `TopBar.qml`.

---

## Key Decisions

### Two-step D-Bus vs XDG portal

PulseAudio exposes a dedicated `org.PulseAudio.ServerLookup1` service specifically for clients that want direct socket access. The resulting `org.PulseAudio.Core1` interface provides a stable, documented API covering sink enumeration, property reads, signal subscriptions, and volume writes — all in one namespace. The XDG audio portal (`org.freedesktop.portal.Settings`) only exposes system-wide volume as a read-only preference; it does not support per-sink control or signal subscriptions. Two-step is therefore the only viable choice for writable volume control.

### System icon theme vs bundled SVGs

Qt's `image://icon/` provider resolves icons against the user's active XDG icon theme at runtime. This ensures the icon appearance matches the rest of the user's desktop without any asset maintenance burden. The four icon names used (`audio-volume-muted-symbolic`, `audio-volume-low-symbolic`, `audio-volume-medium-symbolic`, `audio-volume-high-symbolic`) are part of the Freedesktop icon naming specification and are present in virtually all modern icon themes.

### QML_ELEMENT + QML_SINGLETON + qmlRegisterSingletonType

This is the same dual-registration pattern used by `BatteryService`. `QML_ELEMENT` + `QML_SINGLETON` causes Qt's tooling (qmllint, Qt Creator) to recognize the type without runtime warnings. `qmlRegisterSingletonType` with a factory lambda is still required at runtime for Qt 6 to construct the singleton with the correct parent and lifetime. Both registrations must be present.

### Intermediate `displayVolume` property for animation

Animating `AudioService.volume` directly is not possible because it is a C++ property on a foreign object. The standard QML pattern is to declare a local `property int displayVolume` that mirrors `AudioService.volume` via a binding, then attach a `Behavior` to `displayVolume`. The `text` label binds to `displayVolume`, which the `NumberAnimation` smooths.

---

## Alternatives Considered

### ALSA D-Bus

ALSA does not expose a D-Bus interface on modern systems. Direct PCM device enumeration via libasound would require linking an external C library and bypasses the PulseAudio/PipeWire mixing layer entirely — setting the PCM hardware volume would not affect the application-visible mixer level. Rejected per REQ-C-006.

### XDG desktop portal

`org.freedesktop.portal.Media` and `org.freedesktop.portal.Settings` do not expose sink volume as a writable property. The portal layer is designed for sandboxed applications and abstracts away low-level audio routing. Rejected because setVolume writeback cannot be implemented this way.

### libpulse C API (PulseAudio client library)

`libpulse` provides a feature-complete async C API. However, it requires an additional `pkg_check_modules(LIBPULSE REQUIRED libpulse)` dependency and introduces a callback-heavy programming model that does not integrate naturally with Qt's signal/slot event loop. All required functionality (read volume, subscribe to changes, set volume) is available through the D-Bus interface, making the C library an unnecessary dependency. Rejected per REQ-C-002 and REQ-C-008.

---

## Known Risks

### SetSinkVolumes D-Bus signature

The exact method name and signature for setting volume on a sink varies between PulseAudio versions and PipeWire's PulseAudio compatibility layer. The method `SetVolume` on interface `org.PulseAudio.Core1.Sink` with argument type `au` is the documented path. Verify against a running instance with:

```bash
busctl --user introspect org.PulseAudio.Core1 /org/pulseaudio/core1/sink0
```

If the method is named differently or takes a different argument type (e.g., a struct instead of a plain array), the `QDBusMessage` construction in `setVolume()` will need adjustment.

### Channel count variability

The implementation caches `channel_count_` from the last `readSinkProperties` call. If a sink reports a channel count different from the `Volume` array length seen at subscribe time (e.g., after a profile change), the array passed to `SetVolume` may have the wrong length, causing the D-Bus call to fail silently. Mitigation: always use `volumes.size()` from the most recent `VolumeUpdated` signal to derive the channel count, and update `channel_count_` inside `onVolumeUpdated`.

### PipeWire compatibility

PipeWire ships a PulseAudio D-Bus compatibility module (`pipewire-pulse`). The interface names, property names, and signal names are intended to be identical to PulseAudio's. In practice, subtle differences in property types (e.g., `Volume` as `QVariantList` vs `QList<uint>`) have been observed. The explicit `QDBusArgument` extraction path (rather than `value<QList<uint>>()` directly) is more robust across both daemons and should be preferred.

### Address string encoding

The `Address` property may return an address containing shell-special characters (e.g., `unix:path=/run/user/1000/pulse/dbus-socket`). `QDBusConnection::connectToBus` accepts this string as-is. Do not URL-encode or shell-escape it — pass the raw string from the D-Bus property.

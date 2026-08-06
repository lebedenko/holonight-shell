# Audio Popup — Architecture Design

**Feature**: Audio popup panel (mini-pavucontrol) inside the existing 900×600 status popup  
**Spec**: `docs/sdd/audio-popup/SPEC.md`  
**Design reference**: `assets/dont-commit/01-audio-settings/design-idea.png`

---

## 1. Overview & Component Map

The audio popup feature is already substantially implemented. The existing codebase contains:
- `src/services/audio/AudioService.h/.cpp` — QML singleton with four model properties
- `src/services/audio/AudioDeviceModel.h/.cpp` — QAbstractListModel for sinks/sources
- `src/services/audio/AudioStreamModel.h/.cpp` — QAbstractListModel for sink-inputs/source-outputs
- `src/services/audio/AudioTypes.h` — `AudioDevice`, `AudioStream` structs, type enums
- `src/services/audio/PulseAudioBackend.h/.cpp` — pimpl'd libpulse threaded mainloop implementation
- `src/qml/Topbar/AudioWidget.qml` — bar widget with `StatusPopupTriggerArea` wired to `popupId: "audio"`

**What is missing**: the QML popup content. `StatusPopup.qml` dispatches `popupId == "audio"` to a `Loader` but the `popupSources` map does not yet include an `"audio"` entry, so the popup opens as a titled placeholder with a blank body. No new C++ classes are needed.

### New files to create

| File | Purpose |
|------|---------|
| `src/qml/AudioPopup/AudioPopupContent.qml` | Root content loaded by `StatusPopup.qml` for `popupId "audio"` |
| `src/qml/AudioPopup/AudioTabSidebar.qml` | Left tab sidebar with Output / Input / Applications entries |
| `src/qml/AudioPopup/AudioDeviceDelegate.qml` | Device row: icon, name, description, mute, slider, % |
| `src/qml/AudioPopup/AudioStreamDelegate.qml` | App stream row: icon, name, media title, mute, slider, % |
| `src/qml/AudioPopup/AudioVolumeSlider.qml` | Reusable themed slider used in both delegates and the master bar |
| `src/qml/AudioPopup/AudioMasterBar.qml` | Master volume strip pinned to the panel bottom |
| `src/qml/AudioPopup/AudioDeviceList.qml` | Scrollable ListView wrapping AudioDeviceDelegate |
| `src/qml/AudioPopup/AudioStreamList.qml` | Scrollable ListView wrapping AudioStreamDelegate |

All files are placed in `src/qml/AudioPopup/`. The QML module URI is `HolonightShell`; QRC paths follow the convention `qrc:/HolonightShell/AudioPopup/AudioPopupContent.qml` etc.

### Existing files to modify

| File | Change |
|------|--------|
| `src/qml/Topbar/StatusPopup.qml` | Add `"audio": "AudioPopup/AudioPopupContent.qml"` to `popupSources` and update `showTitle` so audio hides the placeholder title bar |
| `src/surfaces/StatusPopupSurface.cpp` | Register `IconImageProvider` on the popup `QQuickView` engine (currently missing; required for `image://icon/` URLs in stream rows) |
| `CMakeLists.txt` | Append eight new QML files to `HOLONIGHT_QML_FILES` in sorted order |

### No new C++ classes

`AudioService`, `PulseAudioBackend`, `AudioDeviceModel`, and `AudioStreamModel` are already complete and cover all 48 requirements. The IAudioBackend seam concern from the spec is addressed through the existing `SkipInitTag` constructor (used by all unit tests today) and the `PulseAudioBackend` concrete class. A formal `IAudioBackend` abstract interface is **not needed** and not added: the test seam already works — `AudioService(SkipInitTag)` skips backend construction entirely, and tests exercise `applyVolume/applyMuted/setAvailable` and the model methods directly. Introducing an interface would require ABI changes with no benefit.

---

## 2. Backend Architecture

The backend is fully implemented. This section documents it for reference and flags the one gap.

### Threading discipline (already correct)

`PulseAudioBackend::Impl` runs on the `pa_threaded_mainloop` thread. Every PA callback converts the raw `pa_sink_info*` / `pa_sink_input_info*` etc. to a value-copy `AudioDevice` / `AudioStream` struct immediately, then crosses to the Qt main thread via:

```cpp
QMetaObject::invokeMethod(self, [self, dev] { emit self->deviceAdded(dev); },
                          Qt::QueuedConnection);
```

No Qt data structures are touched on the PA thread. `AudioService` receives these signals on the main thread and delegates to `AudioDeviceModel::applyAdd/applyChange/applyRemove` synchronously. This satisfies REQ-NF-001.

### Subscription mask

`onContextReady()` subscribes with:
```
PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE |
PA_SUBSCRIPTION_MASK_SINK_INPUT | PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT |
PA_SUBSCRIPTION_MASK_SERVER
```
`PA_SUBSCRIPTION_EVENT_SERVER` triggers `queryServerInfo()` which re-fetches the default sink/source names and re-queries all four lists. This handles external default-change events (REQ-F-029).

### Re-query strategy

- NEW / CHANGE events → query single item by index (`pa_context_get_sink_info_by_index` etc.), emit `deviceAdded` or `deviceChanged`
- REMOVE events → emit `deviceRemoved(idx)` directly (no re-query)
- SERVER event → full re-query of all four lists (handles default sink/source change)

### Data structs (already defined in `AudioTypes.h`)

```cpp
struct AudioDevice {
  uint32_t id;         // PA index
  QString name;        // internal PA name (used for set-default calls)
  QString description; // human-readable
  uint8_t volume;      // 0–100 (clamped from PA_VOLUME_NORM math; see volumeToPercent)
  bool muted;
  bool is_default;
  AudioDeviceType type; // Sink or Source
};

struct AudioStream {
  uint32_t id;         // PA sink-input or source-output index
  QString name;        // media.name / PA stream name
  QString application; // application.name from proplist
  uint32_t device;     // connected sink or source index
  uint8_t volume;      // 0–100
  bool muted;
  AudioStreamType type; // SinkInput or SourceOutput
};
```

`AudioStream` is missing `icon_name` (from `application.icon_name` proplist key) needed for REQ-F-018. This is the **only data struct gap**.

#### Extending AudioStream for icon_name

Add `QString icon_name` to `AudioTypes.h`:

```cpp
struct AudioStream {
  uint32_t id;
  QString name;
  QString application;
  QString icon_name;   // NEW: from PA_PROP_APPLICATION_ICON_NAME
  uint32_t device;
  uint8_t volume;
  bool muted;
  AudioStreamType type;
};
```

In `PulseAudioBackend::Impl::sinkInputToStream()`:

```cpp
const char* icon = pa_proplist_gets(info->proplist, PA_PROP_APPLICATION_ICON_NAME);
stream.icon_name = QString::fromUtf8(icon != nullptr ? icon : "");
```

Add role `IconName` to `AudioStreamModel::Role` enum and expose it as `"iconName"` in `roleNames()`.

### Monitor source filtering

PulseAudio enumerates monitor sources (e.g., `alsa_output.pci-0000_00_1f.3.analog-stereo.monitor`) as regular sources. These should not appear in the Input Devices tab. Filter them in `sourceToDevice()`:

```cpp
// In Impl::sourceToDevice — after building dev:
if (dev.name.contains(QLatin1String(".monitor"))) {
    dev.type = AudioDeviceType::Monitor; // mark but still emit
}
```

A simpler approach that avoids adding a new enum value: filter by checking `info->monitor_of_sink != PA_INVALID_INDEX` in the callback (non-zero means it is a monitor source). In `sourceListCallback` and `sourceChangedCallback`, skip emission if `info->monitor_of_sink != PA_INVALID_INDEX`. This avoids polluting the model layer with a Monitor type.

### setDefaultOutput / setDefaultInput name vs. index

Currently `setDefaultOutput(idx)` calls `pa_context_set_default_sink` with `QString::number(idx)`. PulseAudio accepts numeric index strings, but the idiomatic call uses the sink *name*. Because `AudioDeviceModel` stores `AudioDevice` which includes `name`, `AudioService::setDefaultOutput(idx)` should look up the name from the model first. The model needs a `nameForId(uint32_t id) const` helper. Alternatively, expose the name directly from QML (the delegate knows its `model.name` role) and add:

```cpp
Q_INVOKABLE void setDefaultOutputByName(const QString& name);
Q_INVOKABLE void setDefaultInputByName(const QString& name);
```

This avoids the name-lookup indirection and keeps QML clean. The QML delegate calls `AudioService.setDefaultOutputByName(model.name)` on row click. These call `pa_context_set_default_sink(ctx, name.toUtf8().constData(), ...)` directly.

---

## 3. Model Layer

### Existing models (no structural changes needed)

`AudioDeviceModel` and `AudioStreamModel` are `QAbstractListModel` subclasses that:
- Store `QList<AudioDevice>` / `QList<AudioStream>` internally
- `applyAdd` → `beginInsertRows / endInsertRows`
- `applyChange` → linear scan by `.id`, replace in-place, emit `dataChanged(idx, idx)` for the single row
- `applyRemove` → linear scan by id, `beginRemoveRows / endRemoveRows`

This already satisfies REQ-NF-008 (granular change notifications) and REQ-F-031 (slider drag stability — an unrelated row's `dataChanged` does not reset a different row's slider, because the `ListView` delegate lifecycle only rebuilds the changed row).

### AudioService model properties (already on the QML singleton)

```cpp
Q_PROPERTY(AudioDeviceModel* outputs READ outputs CONSTANT)   // sinks
Q_PROPERTY(AudioDeviceModel* inputs READ inputs CONSTANT)     // sources (monitor-filtered)
Q_PROPERTY(AudioStreamModel* playbackStreams READ playbackStreams CONSTANT)  // sink-inputs
Q_PROPERTY(AudioStreamModel* recordingStreams READ recordingStreams CONSTANT) // source-outputs
```

QML accesses them as `AudioService.outputs`, `AudioService.inputs`, etc.

### New property needed: defaultOutputId

`AudioService` tracks `default_output_id_` privately. QML needs to observe which device row is the default output for Master Volume sync. Expose it:

```cpp
Q_PROPERTY(quint32 defaultOutputId READ defaultOutputId NOTIFY defaultOutputIdChanged)
```

`applyDefaultDeviceState(device)` already sets `default_output_id_` — add `emit defaultOutputIdChanged()` there. This lets the Master Volume bar bind to `AudioService.defaultOutputId` and look up that row's volume from the model, and lets the Output device list highlight the default row.

---

## 4. QML Structure

### How content plugs into StatusPopup.qml

`StatusPopup.qml` has a `popupSources` map and a `Loader` at `id: contentLoader`. Currently audio is absent from the map (renders as a titled placeholder). The single change:

```qml
readonly property var popupSources: ({
    "weather": "WeatherPopupContent.qml",
    "audio":   "AudioPopup/AudioPopupContent.qml"   // ADD THIS
})
readonly property bool showTitle: root.contentSource.length === 0  // unchanged — audio hides title
```

The Loader geometry for content is:
```
x: root.panelLeft + 16
y: root.panelTop + 14   (when showTitle is false — audio)
width: root.panelRight - root.panelLeft - 32
height: root.panelBottom - y - 14
```

For the audio popup (900×600 surface) the visible panel is 900px wide with 24px glow padding each side, so panel interior is 852px × ~530px. The content area available to `AudioPopupContent.qml` is therefore approximately 820px × 516px.

### AudioPopupContent.qml

Top-level layout: horizontal `RowLayout` or manual geometry.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│ [Tab Sidebar 160px] │ [Main Content Area fills remaining width]                   │
│                     │                                                             │
│  Output Devices     │  (Device or stream ListView, scrollable)                   │
│  Input Devices      │                                                             │
│  Applications       │                                                             │
│                     │                                                             │
├─────────────────────┴─────────────────────────────────────────────────────────────┤
│ [Master Volume Bar — full width, pinned bottom, ~56px tall]                       │
└──────────────────────────────────────────────────────────────────────────────────┘
```

Properties:
```qml
Item {
    id: root
    property int currentTab: 0  // 0=Output, 1=Input, 2=Applications
    // No required properties — content is self-contained, reads AudioService singleton
}
```

The `currentTab` drives which model is shown; it defaults to `0` (Output Devices) satisfying REQ-F-002.

### AudioTabSidebar.qml

A `Column` of three tab buttons. Each button:
- Text label ("Output Devices", "Input Devices", "Applications")
- Left accent bar (Rectangle, 3px wide) visible when active, color: cyan for output, violet for input, cyan for applications
- Background highlight using `HoloniightPalette.surfaceVariant` at reduced opacity when active
- `MouseArea` sets `root.currentTab` on the parent

No `required` properties needed — it receives `currentTab` and `onTabChanged` as regular properties from `AudioPopupContent`.

### AudioDeviceList.qml / AudioStreamList.qml

Thin wrappers around `ListView`:

```qml
// AudioDeviceList.qml
ListView {
    id: root
    required property var model          // AudioService.outputs or .inputs
    required property color accentColor  // cyan or violet
    required property bool isInput       // drives set-default invokable choice
    clip: true
    delegate: AudioDeviceDelegate { ... }
}
```

`AudioStreamList.qml` is identical in structure with `AudioStreamDelegate` and no `accentColor`/`isInput` concept (always cyan per spec).

### AudioDeviceDelegate.qml

```qml
Item {
    id: root
    // model roles available via ListView delegation:
    // model.deviceId, model.name, model.description,
    // model.volume, model.muted, model.isDefault
    required property color accentColor
    required property bool isInputDevice

    height: 72
    width: ListView.view.width

    // Row layout (left→right):
    // [Icon 36px] [Name+Description column, fills] [DEFAULT badge, conditional]
    //             [Mute button 32px] [AudioVolumeSlider expands] [Volume% 44px fixed]

    property bool dragging: false   // slider drag guard (REQ-F-031)

    // Click anywhere in row (except slider/mute) → set as default
    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (root.isInputDevice)
                AudioService.setDefaultInputByName(model.name)
            else
                AudioService.setDefaultOutputByName(model.name)
        }
    }

    // Icon: theme icon "audio-card" or similar fallback
    Image { source: "image://icon/audio-card"; width: 36; height: 36 }

    // Name elision
    Text {
        text: model.name
        elide: Text.ElideRight
        // ...
    }

    // DEFAULT badge
    Rectangle {
        visible: model.isDefault
        // cyan-colored pill with "DEFAULT" text
        color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.18)
        border.color: root.accentColor
        // ...
    }

    // Mute toggle
    Rectangle {
        // icon changes between volume-muted and volume-medium
        MouseArea {
            onClicked: AudioService.setDeviceMuted(model.deviceId, !model.muted)
        }
    }

    // Volume slider
    AudioVolumeSlider {
        value: model.volume
        accentColor: root.accentColor
        onDraggingChanged: root.dragging = dragging
        onValueCommitted: AudioService.setDeviceVolume(model.deviceId, value)
        onValueChanging: AudioService.setDeviceVolume(model.deviceId, value)
    }

    // Volume percentage text
    Text { text: model.volume + "%"; width: 44 }
}
```

### AudioStreamDelegate.qml

Similar to device delegate but:
- Icon from `model.iconName` via `image://icon/`, fallback to `"audio-x-generic"` on error
- Uses `model.application` for primary label, `model.name` as subtitle
- `onClicked` on row does nothing (streams have no default concept)
- `onValueCommitted` → `AudioService.setStreamVolume(model.streamId, value)`
- `onValueCommitted` mute → `AudioService.setStreamMuted(model.streamId, !model.muted)`

Icon fallback pattern (REQ-C-005):
```qml
Image {
    id: appIcon
    source: model.iconName !== "" ? "image://icon/" + model.iconName : ""
    onStatusChanged: {
        if (status === Image.Error || status === Image.Null)
            source = "image://icon/audio-x-generic"
    }
}
```

### AudioVolumeSlider.qml

A custom slider using a `MouseArea` (or `QtQuick.Controls.Slider`). Emits two signals:
- `signal valueChanging(int value)` — during drag (continuous update, REQ-F-007)
- `signal valueCommitted(int value)` — on release (lock-in)

Exposes `property bool dragging` so the delegate can gate incoming model updates.

The value is driven by `property int value` bound to the model role. To avoid a binding loop when the user drags, the slider maintains an internal `property int displayValue` that is set from `value` only when `!dragging`:

```qml
property int value           // IN: from model
property bool dragging: false
property int displayValue: value

onValueChanged: {
    if (!dragging) displayValue = value
}
```

The thumb position binds to `displayValue`. On drag start, `dragging = true` — further `value` updates are ignored. On release, `dragging = false`, and the next model update (from the PA echo of the set operation) will resync `displayValue`. This satisfies REQ-F-031.

Color theming: `property color accentColor` drives the fill bar and thumb color.

### AudioMasterBar.qml

Pinned to the bottom of `AudioPopupContent`. Spans the full content width.

```qml
Item {
    id: root
    height: 56

    // "Master Volume" label on left
    // Volume % on right (fixed width)
    // AudioVolumeSlider fills center

    // Reads defaultOutputId to find the default sink's volume in the model
    readonly property int masterVolume: AudioService.volume   // already tracked by AudioService

    AudioVolumeSlider {
        value: root.masterVolume
        accentColor: HoloniightPalette.accentCyan
        onValueChanging: AudioService.setVolume(value)
        onValueCommitted: AudioService.setVolume(value)
    }
}
```

`AudioService.setVolume(percent)` calls `backend_->setDeviceVolume(default_output_id_, percent)`. The PA echo comes back as a `deviceChanged` signal for the default sink, which calls `applyDefaultDeviceState(device)` → `applyVolume(device.volume)` → `emit volumeChanged()`. The master bar's `value` binding refreshes automatically. No explicit sync logic needed.

---

## 5. Master Volume Sync

**The bidirectional sync concern (REQ-F-024/025) is already solved by the existing architecture.**

`AudioService` maintains:
- `volume_` / `muted_` — shadow the default sink's current state
- `default_output_id_` — tracks which model row is the default

When the user drags the **device row slider** for the default sink:
1. `AudioVolumeSlider` emits `valueChanging(v)` / `valueCommitted(v)`
2. Delegate calls `AudioService.setDeviceVolume(model.deviceId, v)`
3. PA applies the change, emits a `PA_SUBSCRIPTION_EVENT_SINK` change event
4. `PulseAudioBackend` emits `deviceChanged(device)` on the Qt thread
5. `AudioService::onDeviceChanged(device)` → `model->applyChange(device)` (updates the row) + `applyDefaultDeviceState(device)` (updates `volume_`, emits `volumeChanged()`)
6. Master bar's `value: AudioService.volume` updates

When the user drags the **master volume slider**:
1. `AudioService.setVolume(v)` calls `backend_->setDeviceVolume(default_output_id_, v)`
2. Same PA → `deviceChanged` → `applyChange` → the device row's `model.volume` role updates
3. Device row's slider `value` binding refreshes (if not currently being dragged by the user)

**No binding loop risk**: the slider's `displayValue` only reads `model.volume` when `!dragging`. After the PA echo, the update arrives asynchronously (queued call), so by the time it lands on the Qt thread the drag is already complete.

**Default sink change (REQ-F-025)**: when the user clicks a device row to set it as default, the `PA_SUBSCRIPTION_EVENT_SERVER` fires, `queryServerInfo()` refreshes `default_sink_name`, re-queries all sinks, emits `deviceChanged` for each (updating `is_default` flags), and the new default's `applyDefaultDeviceState` call updates `volume_`. Master bar follows automatically.

---

## 6. Interfaces / APIs

### AudioService Q_INVOKABLE methods (existing + additions)

```cpp
// Existing:
Q_INVOKABLE void setVolume(int percent);               // master volume via default output
Q_INVOKABLE void setDefaultOutput(uint32_t idx);       // kept for completeness
Q_INVOKABLE void setDefaultInput(uint32_t idx);        // kept for completeness
Q_INVOKABLE void setDeviceVolume(uint32_t idx, int percent);
Q_INVOKABLE void setDeviceMuted(uint32_t idx, bool muted);
Q_INVOKABLE void setStreamVolume(uint32_t idx, int percent);
Q_INVOKABLE void setStreamMuted(uint32_t idx, bool muted);
Q_INVOKABLE void moveStreamToOutput(uint32_t stream_idx, uint32_t sink_idx); // deferred

// New:
Q_INVOKABLE void setDefaultOutputByName(const QString& name);
Q_INVOKABLE void setDefaultInputByName(const QString& name);
```

### AudioService Q_PROPERTY (existing + additions)

```cpp
// Existing:
Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged)
Q_PROPERTY(bool available READ available NOTIFY availableChanged)
Q_PROPERTY(AudioDeviceModel* outputs READ outputs CONSTANT)
Q_PROPERTY(AudioDeviceModel* inputs READ inputs CONSTANT)
Q_PROPERTY(AudioStreamModel* playbackStreams READ playbackStreams CONSTANT)
Q_PROPERTY(AudioStreamModel* recordingStreams READ recordingStreams CONSTANT)

// New:
Q_PROPERTY(quint32 defaultOutputId READ defaultOutputId NOTIFY defaultOutputIdChanged)
```

### AudioStreamModel role additions

```cpp
enum class Role : uint16_t {
    StreamId = Qt::UserRole + 1,
    Name,
    Application,
    IconName,       // NEW
    Volume,
    Muted,
    CurrentDevice,
};
```

Role name in `roleNames()`: `"iconName"`.

### AudioService signals

```
volumeChanged()          // existing
mutedChanged()           // existing
availableChanged()       // existing
defaultOutputIdChanged() // NEW
```

---

## 7. Key Decisions with Rationale

### Per-type models vs. combined model

The spec mandates per-type models (one for outputs, one for inputs, one for playback streams, one for recording streams) — and this is already the implementation. Rationale: different tabs bind to different models; a combined model would require filtering in QML which is both less efficient and more fragile. The spec's REQ-NF-008 (granular updates) is easier to satisfy with per-type models because a volume change on sink 0 emits `dataChanged` only on the `outputs` model, not the combined model.

### Reusing AudioService vs. a new service

No new service is created. `AudioService` already exposes all four models as QML singleton properties and provides all Q_INVOKABLE methods needed. Creating a separate `AudioPopupService` would duplicate the libpulse connection, introduce a second PA context (wasteful), and require synchronization between them.

### IAudioBackend interface not introduced

The spec mentions IAudioBackend, but the codebase already has a working test seam: `AudioService(SkipInitTag)` creates a service with `backend_ == nullptr`, allowing tests to drive it via `applyVolume`, `applyMuted`, `setAvailable`, and the model `applyAdd/applyChange/applyRemove` methods directly. Introducing a virtual interface would add indirection, require virtual dispatch on every PA event, and not enable any tests that aren't already possible. The `SkipInitTag` pattern is the project's established seam (used in `test_audio_service.cpp`).

### IconImageProvider must be registered in StatusPopupSurface

`StatusPopupSurface` creates its own `QQuickView` (separate from the bar views in `LayerShellManager`). The icon provider is registered in `LayerShellManager::createBar()` and `TrayMenuSurface`. The popup view does not currently register it. Without this fix, `image://icon/...` URLs in `AudioStreamDelegate` resolve to broken images. The fix is a one-line addition in `StatusPopupSurface::ensureSurface()` before `view_->setSource(...)`:

```cpp
#include "IconImageProvider.h"
// ...
view_->engine()->addImageProvider(QStringLiteral("icon"), new IconImageProvider());
view_->setSource(QUrl(QStringLiteral("qrc:/HolonightShell/Topbar/StatusPopup.qml")));
```

`StatusPopupSurface` lives in `holonight_surfaces`, and `IconImageProvider.h` is already in that same library, so no new include path is needed.

### Monitor source filtering in the backend

Monitor sources (virtual loopback sources PA creates for each sink) should not appear in the Input Devices list. Filtering is done in the PA callback before the queued emit, not in the model, to keep the model generic. The filter: `if (info->monitor_of_sink != PA_INVALID_INDEX) return;` in `sourceListCallback` and `sourceChangedCallback`.

### setDefault by name, not index

`pa_context_set_default_sink` accepts a sink name string. Passing a numeric index string works on most PulseAudio versions but is not guaranteed by the API. Since `AudioDevice.name` is already stored in the model, QML delegates pass `model.name` to `AudioService.setDefaultOutputByName(name)`. This is more robust than numeric lookup.

---

## 8. Alternatives Considered

| Option | Why rejected |
|--------|-------------|
| `QAbstractListModel` with filter proxy for monitor sources | Adds indirection; filter-in-backend is simpler and means the model never sees irrelevant data |
| Single `AudioPopupContent.qml` with all delegate types inlined | Splits poorly for maintainability; separate delegate files enable isolated qmllint and future reuse |
| `QtQuick.Controls.Slider` for the volume slider | Requires extensive style overriding for HoloNight look; a custom `MouseArea`-based slider gives full control over geometry and accent colors with less total code |
| `ListView` inside a `ScrollView` from QtQuick.Controls | `ScrollView` adds implicit padding; plain `ListView` with `clip: true` and custom `ScrollBar` overlay is more precise |
| New `IAudioBackend` virtual class | See decision above — adds complexity without enabling new tests |

---

## 9. Risks

### AudioWidget regression (REQ-C-002)

`AudioWidget.qml` binds exclusively to `AudioService.volume`, `AudioService.muted`, and `AudioService.available`. None of the new QML files touch these properties. The new `defaultOutputId` property is additive. The new `setDefaultOutputByName` / `setDefaultInputByName` invokables do not change existing invokable behavior. The `AudioWidget`'s `setVolume` call path (`AudioService.setVolume` → `backend_->setDeviceVolume(default_output_id_)`) is unchanged.

**Mitigation**: run `task run` and exercise scroll-wheel volume on the bar widget after any service-layer change.

### icon_name addition to AudioStream / AudioStreamModel

Adding a new field to `AudioStream` is backward-compatible (value-initialized to `""` for existing tests). Adding a new role to `AudioStreamModel` is backward-compatible (existing `data()` switch falls through the new case to `return {}`). Existing `test_audio_stream_model.cpp` tests will continue to pass; the new role simply won't be tested until a new test case is added.

**Mitigation**: add one test case verifying `IconName` role data after the struct change.

### Cross-thread race on model clear + rebuild

The `PA_SUBSCRIPTION_EVENT_SERVER` handler calls `queryServerInfo()`, which issues a full re-query of all four item types. Until the callbacks arrive, the models may briefly show stale `is_default` flags. This is a pre-existing behavior (not introduced by this feature). Since the update is queued, the QML engine always sees consistent snapshots per event-loop tick.

### Binding loop on master volume slider

If `AudioService.volume` updates while the master slider is being dragged, and the slider's `value` binding is live, the slider position would jump. This is prevented by the `dragging` guard in `AudioVolumeSlider.qml` (see Section 4).

### Slider drag stability across multiple sinks (REQ-F-031)

The `dragging` flag is per-delegate-instance. Dragging sink A's slider does not set the `dragging` flag on sink B's delegate. When an external volume change arrives for sink B, sink B's delegate receives a `dataChanged` for its row, its `displayValue` updates (because `!dragging`), and sink A's row is unaffected. This satisfies REQ-F-031 structurally.

### libpulse enumeration edge cases

- **Default sink `@DEFAULT_SINK@`**: `pa_context_set_default_sink` is called with the explicit sink name string, not the alias. The server info callback populates `default_sink_name` before any `sinkToDevice` call, so `is_default` is correct from the first emission.
- **No audio server (REQ-F-032)**: `PA_CONTEXT_FAILED` / `PA_CONTEXT_TERMINATED` emits `availableChanged(false)`. `AudioService.available` goes false. `AudioPopupContent.qml` should show the "Audio service unavailable" empty state when `!AudioService.available`.
- **Empty lists (REQ-F-033)**: `ListView` with `model.count === 0` shows nothing by default. Wrap with a `Loader` or `Visible` toggled empty-state `Text` item.

### Icon resolution failures (REQ-C-005)

`image://icon/` returns a null pixmap when the icon name is not in the system theme. The `IconImageProvider` logs a warning but does not crash. The `Image` element transitions to `Image.Error` status. The `onStatusChanged` handler in `AudioStreamDelegate` replaces the source with `"image://icon/audio-x-generic"` — a standard freedesktop icon present on all systems with an icon theme.

---

## 10. Test Plan

### GTest coverage (no Wayland session required)

All tests in `test_holonight_services` (linked against `holonight_services`) run with `QT_QPA_PLATFORM=offscreen`.

**Existing tests that must continue to pass** (no changes needed):
- `test_audio_device_model.cpp` — 8 cases covering add, change, remove, roles, data, clear, signal counts
- `test_audio_stream_model.cpp` — equivalent for streams
- `test_audio_service.cpp` — SkipInitTag seam, volume/muted/available signals, model non-null
- `test_pulse_audio_backend.cpp` — construct/destroy, stop-without-start, control-nop-without-start

**New test cases to add** (in `test_holonight_services`):

`test_audio_stream_model.cpp` additions:
- `AudioStreamModel_IconNameRoleIsStoredAndReturned`: add a stream with `icon_name = "firefox"`, verify `data(..., IconName)` returns `"firefox"`.

`test_audio_service.cpp` additions:
- `AudioService_DefaultOutputIdTracksDefault`: add a device with `is_default = true` via `onDeviceAdded`, verify `service.defaultOutputId()` equals the device id.
- `AudioService_DefaultOutputIdEmitsSignalOnChange`: spy on `defaultOutputIdChanged`, add a default device, verify signal fires once.
- `AudioService_SetDefaultOutputByNameIsNoOpWithoutBackend`: call `service.setDefaultOutputByName("hw:0")`, verify no crash (SkipInit has no backend).

`test_pulse_audio_backend.cpp` additions:
- `PulseAudioBackend_MonitorSourceFiltering`: requires real PA (`HOLONIGHT_TEST_REAL_PULSE_AUDIO`), skipped otherwise — verify no device with `.monitor` in name is emitted by `deviceAdded`.

### Manual Wayland testing (requires `task run`)

All acceptance criteria from the spec are verified manually:

| Test area | Verification method |
|-----------|---------------------|
| Tab navigation (REQ-F-001, F-002) | Click each tab; confirm only active tab content is visible |
| Device list population (REQ-F-003, F-010, F-017) | Compare popup list count vs. `pactl list sinks/sources/sink-inputs` |
| Row layout / elision (REQ-F-004, F-011, F-018, F-034) | Rename a device to 50+ chars via PulseAudio config; inspect row |
| DEFAULT badge (REQ-F-005, F-012) | `pactl set-default-sink <name>` and re-open; verify badge placement |
| Mute toggle (REQ-F-006, F-013, F-019) | Click mute; verify `pactl list sinks | grep Mute` flips |
| Volume slider (REQ-F-007, F-014, F-020) | Drag to 50%; verify `pactl list sinks | grep Volume` shows ~50% |
| Default row click (REQ-F-008, F-015) | Click non-default row; badge moves; `pactl get-default-sink` confirms |
| Volume clamp (REQ-F-009, F-021) | `pactl set-sink-volume 0 150%`; popup shows 100% |
| Input accent color (REQ-F-016) | Inspect Input tab; badges and slider are violet not cyan |
| App icon fallback (REQ-F-018, REQ-C-005) | Launch app with unknown icon name; row shows generic audio icon |
| Master volume (REQ-F-022, F-023) | Drag master slider; `pactl list sinks | grep Volume` confirms |
| Bidirectional sync (REQ-F-024) | Move device row slider; master bar follows; move master; device row follows |
| Default change sync (REQ-F-025) | Click other sink as default; master bar updates to new sink's volume |
| Live updates (REQ-F-026..F-030) | Run `pactl` commands in terminal while popup is open |
| Slider drag stability (REQ-F-031) | Drag one sink slider while externally changing another |
| Unavailable state (REQ-F-032) | Kill PulseAudio; popup shows "Audio service unavailable" |
| Empty list state (REQ-F-033) | Verify "No input devices found" in VM or minimal setup |
| Dismissal (REQ-C-003) | Click outside → closes; click widget → reopens |

---

## 11. File-by-File Change List

### New files

```
src/qml/AudioPopup/AudioPopupContent.qml
src/qml/AudioPopup/AudioTabSidebar.qml
src/qml/AudioPopup/AudioDeviceList.qml
src/qml/AudioPopup/AudioStreamList.qml
src/qml/AudioPopup/AudioDeviceDelegate.qml
src/qml/AudioPopup/AudioStreamDelegate.qml
src/qml/AudioPopup/AudioVolumeSlider.qml
src/qml/AudioPopup/AudioMasterBar.qml
```

### Modified files

**`src/services/audio/AudioTypes.h`**
- Add `QString icon_name` field to `AudioStream`

**`src/services/audio/AudioStreamModel.h`**
- Add `IconName` to `Role` enum

**`src/services/audio/AudioStreamModel.cpp`**
- Handle `Role::IconName` in `data()`, add `"iconName"` to `roleNames()`

**`src/services/audio/PulseAudioBackend.cpp`**
- `sinkInputToStream()`: read `PA_PROP_APPLICATION_ICON_NAME` into `stream.icon_name`
- `sourceListCallback` / `sourceChangedCallback`: skip monitor sources (`info->monitor_of_sink != PA_INVALID_INDEX`)
- `sourceOutputListCallback` / `sourceOutputChangedCallback`: (no change needed — source-outputs are never monitors)

**`src/services/audio/AudioService.h`**
- Add `Q_PROPERTY(quint32 defaultOutputId ...)` and `defaultOutputIdChanged` signal
- Add `Q_INVOKABLE void setDefaultOutputByName(const QString& name)`
- Add `Q_INVOKABLE void setDefaultInputByName(const QString& name)`

**`src/services/audio/AudioService.cpp`**
- Implement `setDefaultOutputByName`, `setDefaultInputByName` (delegate to `backend_->setDefaultOutputByName` or look up in models)
- Emit `defaultOutputIdChanged()` in `applyDefaultDeviceState()`

**`src/services/audio/PulseAudioBackend.h`**
- Add `void setDefaultOutputByName(const QString& name)` and `void setDefaultInputByName(const QString& name)` (or keep using `setDefaultOutput(idx)` if name-by-index lookup is done in AudioService)

**`src/surfaces/StatusPopupSurface.cpp`**
- Add `#include "IconImageProvider.h"` (already in the same `holonight_surfaces` library)
- In `ensureSurface()`, before `view_->setSource(...)`: `view_->engine()->addImageProvider(QStringLiteral("icon"), new IconImageProvider())`

**`src/qml/Topbar/StatusPopup.qml`**
- Add `"audio": "AudioPopup/AudioPopupContent.qml"` to `popupSources`

**`CMakeLists.txt`**
- Append to `HOLONIGHT_QML_FILES` in sorted order (the list is alphabetically sorted; `src/qml/AudioPopup/` sorts before `src/qml/Background/`):
```cmake
src/qml/AudioPopup/AudioDeviceDelegate.qml
src/qml/AudioPopup/AudioDeviceList.qml
src/qml/AudioPopup/AudioMasterBar.qml
src/qml/AudioPopup/AudioPopupContent.qml
src/qml/AudioPopup/AudioStreamDelegate.qml
src/qml/AudioPopup/AudioStreamList.qml
src/qml/AudioPopup/AudioTabSidebar.qml
src/qml/AudioPopup/AudioVolumeSlider.qml
```

**`tests/test_audio_stream_model.cpp`**
- Add test case for `IconName` role

**`tests/test_audio_service.cpp`**
- Add test cases for `defaultOutputId` property and `setDefaultOutputByName` no-op behavior

### Files that do NOT need changes

- `src/app/ShellApplication.h/.cpp` — `AudioService` is already registered via `reg(audio_, "AudioService")`; no new services to register
- `src/services/audio/AudioDeviceModel.h/.cpp` — no new roles needed for the device model
- `src/qml/Topbar/AudioWidget.qml` — unchanged (REQ-C-002)
- `src/qml/Topbar/StatusPopupTriggerArea.qml` — unchanged (already wired to `"audio"`)
- All other QML and C++ files — unaffected

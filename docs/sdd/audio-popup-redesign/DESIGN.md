# Audio Popup Redesign — Architecture Design

**Status**: Complete (as-built design)
**Input**: `docs/sdd/audio-popup-redesign/SPEC.md` (Stage 1, approved, ~40 EARS requirements)
**Target Directory**: `apps/shell/qml/Popups/Audio/`
**Backend**: `libs/holonight-services/src/audio/`
**Date**: 2026-08-12

---

## As-built amendments

The completed implementation incorporates the remediation pass recorded in `TASKS.md` T-030 and
T-031. These decisions supersede earlier proposal snippets in this document where they differ:

- The 780×820 popup pins Header and Master above one scroller and pins the footer below it. The
  scroll order is Output → Applications → Input.
- The enabled semantic gear uses `SettingsNavigationService` to asynchronously activate the
  `audio` settings page, then closes the popup.
- Device metadata is localized in kHz, including fractional values such as 44.1 kHz.
- Output uses cyan and Input uses violet for device controls; the live meter uses cyan/passive
  fixed-height segments. Summary rows retain DEFAULT badges, while expanded list rows do not.
- The popup root exclusively owns input-monitor acquisition/release. Full-width semantic
  separators bleed across the host's content inset while other content retains its padding.

---

## 1. Overview & Scope

The current popup (`AudioPopupContent.qml` + `AudioTabSidebar.qml`) is a tab-sidebar layout with
three switched views (Output / Input / Applications) and a pinned `AudioMasterBar.qml` footer. The
redesign replaces it with a single scrollable column: header → master-volume hero → Output
accordion → Input accordion (with a live mic-level meter) → Applications (4-row cap) → keyboard
footer. `AudioTabSidebar.qml` is deleted; no tab-selection UI remains.

This document is grounded in the actual files read during Stage 2 (all under
`apps/shell/qml/Popups/Audio/`, `libs/holonight-services/src/audio/`, and the shared control
library at `holonight-qt/qml/controls/`, which builds into the `Holonight.Controls`/
`Holonight.Core` modules this repo consumes). Two things in SPEC.md needed grounding against
source reality; both are called out explicitly in §5 ("Spec Correction") rather than re-litigated
as scope.

---

## 2. Component Tree

```
AudioPopupContent.qml                              (rewritten — root, owns accordion state)
├── AudioPopupHeader.qml                            (new)
│     speaker HnIcon + "AUDIO" label (left) | accessible HnIconButton gear (right)
├── AudioMasterPanel.qml                            (renamed+restyled from AudioMasterBar.qml)
│     glow badge | "MASTER VOLUME" label + percentage | AudioVolumeSlider | mute square button
├── AudioDeviceSection.qml  (isInput: false)        (new — Output instance)
│     ├── AudioCurrentDeviceRow.qml                 (new — summary row, shared by both sections)
│     └── AudioDeviceList.qml (existing, unchanged) → AudioDeviceDelegate.qml (extended)
├── AudioDeviceSection.qml  (isInput: true)         (new — Input instance)
│     ├── AudioCurrentDeviceRow.qml                 (same component; trailingContent = meter)
│     │     └── InputLevelMeter.qml                 (new)
│     └── AudioDeviceList.qml (existing, unchanged) → AudioDeviceDelegate.qml (extended)
├── AudioApplicationsSection.qml                    (new)
│     └── AudioStreamList.qml (existing, unchanged) → AudioStreamDelegate.qml (extended)
└── KeyboardHintFooter.qml                          (new)

Deleted: AudioTabSidebar.qml
Unchanged: AudioDeviceList.qml, AudioStreamList.qml, AudioVolumeSlider.qml
New shared helper: AudioMetadataFormat.js
```

### 2.1 Output vs. Input: one shared `AudioDeviceSection.qml`, not two files

SPEC.md's file list names `AudioOutputDeviceSection.qml` and `AudioInputSection.qml` as separate
files and explicitly leaves the sharing decision to Stage 2 (REQ-F-5001/5002/5003 vs.
REQ-F-6001/6002/6003 are structurally identical apart from label text, accent, model source, the
default-setter invokable, and the Input-only meter).

**Decision: one `AudioDeviceSection.qml` parameterized by `isInput: bool`.**

This directly matches an existing convention already in this codebase: `AudioDeviceList.qml`
(kept unchanged) already takes `isInput: bool` + `accentColor: color` and dispatches
`setDefaultOutputByName`/`setDefaultInputByName` internally via `AudioDeviceDelegate`'s
`isInputDevice` property. Introducing a second boolean-parameterized layer above it is consistent,
not a new pattern. Contract:

```qml
// AudioDeviceSection.qml
required property bool isInput
required property bool expanded        // owned by AudioPopupContent, read-only here
signal expandRequested()                // "user wants this section to be the expanded one"

readonly property AudioDeviceModel model: root.isInput ? AudioService.inputs : AudioService.outputs
readonly property color accentColor: root.isInput ? HoloniightPalette.accentViolet : HoloniightPalette.accentCyan
readonly property string sectionLabel: root.isInput ? qsTr("INPUT DEVICES") : qsTr("OUTPUT DEVICES")
readonly property var defaultDeviceInfo: root.model.defaultDevice   // see §4.2
```

Internally it composes `AudioCurrentDeviceRow` (always visible, chevron drives `expandRequested()`)
+ a conditionally-visible `sectionLabel` `Text` + the existing `AudioDeviceList`. The Input-only
meter is passed as `AudioCurrentDeviceRow.trailingContent` only when `isInput` is true (see §2.2).
Two thin instantiations in `AudioPopupContent.qml` (`isInput: false` and `isInput: true`) replace
what would otherwise be two near-duplicate ~120-line files that must stay in lockstep on every
future spacing/behavior tweak.

### 2.2 `AudioCurrentDeviceRow.qml` contract

```qml
pragma ComponentBehavior: Bound
// AudioCurrentDeviceRow.qml
required property string iconName        // defaultDeviceInfo.iconName
required property string title           // defaultDeviceInfo.description (fallback name)
required property string subtitle        // formatted metadata, see §8.2
required property bool expanded
required property color accentColor
property Component trailingContent: null // null for Output; InputLevelMeter for Input
signal toggled()
```

Per REQ-F-5001's own acceptance criteria, the Output summary row's content list is icon + name +
subtitle + DEFAULT pill + chevron — no percentage/slider. REQ-F-6001 says the Input row is
"structurally identical... except the percentage readout is replaced by" the meter, which implies
a percentage slot the Output ACs never actually list. Rather than adding a slot Output doesn't use,
`trailingContent` is `null` (nothing rendered) for Output and an `InputLevelMeter` instance for
Input — the row stays a single component with one optional slot, and Output simply never populates
it. The DEFAULT pill is unconditional (the summary row by construction always represents the
current default device) — no separate `isDefault` property needed.

Clicking anywhere on the row (not just the chevron glyph) calls `toggled()` — REQ-F-5001/6001 both
say "Summary row is clickable," not "chevron is clickable." `AudioPopupContent.qml` owns the actual
state transition (§3).

### 2.3 `AudioDeviceDelegate.qml` changes

- **Add** (leadingContent): a radio-circle-with-checkmark glyph, composed inline as a `RowLayout`
  wrapping a new small circle+tick indicator alongside the existing `ExternalIcon`. See §6.
- **Change**: `ExternalIcon.iconName` currently hardcodes `"audio-card"` / `"audio-input-microphone"`
  based on `isInputDevice`. Replace with `root.model.iconName` (new role, §4.1), falling back to
  the current hardcoded values via `ExternalIcon.fallbackIconName` (already a supported property,
  used today by `AudioStreamDelegate.qml` for `"audio-x-generic"`) — this satisfies REQ-F-4002's
  "unsupported/missing icon names fall back to a generic audio icon" for free, no new fallback
  logic needed.
- **Change**: `subtitle` currently shows the device's raw PA name (`root.model.name`). Replace with
  the formatted metadata string from `AudioMetadataFormat.js` (§8.2), reading the four new roles.
- **Remove**: the per-row `deviceMuteButton` `Item`/`MouseArea`/`ExternalIcon` trio in
  `trailingContent`. REQ-F-5002/6002's acceptance-criteria row-content lists are radio-circle,
  icon, name, metadata, slider, percentage — no mute button. Confirmed by the SPEC's own file
  summary: *"remove old 'mute button on row'"*. `AudioService.setDeviceMuted`/`setInputDeviceMuted`
  remain on the backend (still reachable via OSD/other callers) but lose their only UI entry point
  in this popup other than the default output (see §11, risk 7).

### 2.4 `AudioStreamDelegate.qml` changes

- **Remove**: the per-row `streamMuteButton` trio (same rationale — REQ-F-9001's row-content list
  has no mute button; SPEC file summary: *"remove old mute button if it existed on stream rows"*).
- **Add**: a static "..." disabled placeholder in `trailingContent`, using the semantic disabled
  text color and no
  `MouseArea`, no `onClicked` — satisfies REQ-F-12002/REQ-C-15003 by construction (there is no
  handler to remove or audit away).

### 2.5 `AudioApplicationsSection.qml` contract

```qml
// AudioApplicationsSection.qml
property bool showAll: false   // local, resets to false every popup open — see §3

// "APPLICATION VOLUME" label + "Show All" text + chevron (MouseArea, plain — see §9.4)
// A clipped, height-capped container around the existing AudioStreamList; see §8.3.
```

### 2.6 `AudioPopupHeader.qml` contract

```qml
// AudioPopupHeader.qml
// Left: HnIcon (speaker glyph) + "AUDIO" Text (uppercase, bold, per REQ-F-1003).
// Right: enabled HnIconButton with accessible name "Open Audio settings".
```

The shared button supplies hover, pressed, focus, and accessibility behavior. Its click handler
calls `SettingsNavigationService.openPage("audio")` and then `StatusPopupSurface.hide()`.

### 2.7 Chevron

Both `AudioCurrentDeviceRow.qml` and `AudioApplicationsSection.qml`'s "Show All" row use
`NetworkPopupIcon { name: "chevron" }` (already draws `M9 18l6-6-6-6`, a right-pointing ›), rotated
via the component's ordinary `rotation` property — no new glyph code needed. `rotation: expanded ?
-90 : 90` (down when collapsed, up when expanded) with a short `Behavior on rotation`.

---

## 3. Accordion State Ownership

**Owned by `AudioPopupContent.qml`** as a single property:

```qml
// AudioPopupContent.qml
property bool outputExpanded: true   // REQ-F-7002 default
readonly property bool inputExpanded: !root.outputExpanded   // mutual exclusivity, REQ-F-7001
```

A single boolean is sufficient and *structurally* enforces REQ-F-7001 ("no state exists where both
sections are expanded or both are collapsed") — there is no representable illegal state, unlike an
`int` enum or two independent booleans which would need a runtime invariant check. Each
`AudioDeviceSection` receives `expanded: root.outputExpanded` / `expanded: root.inputExpanded`
top-down (read-only from the child's perspective) and emits `expandRequested()` when its summary
row is clicked (bubbled up from `AudioCurrentDeviceRow.toggled()`); the parent's handler sets
`root.outputExpanded = true` / `false`. This mirrors the signal-up/property-down idiom already used
by the file being deleted (`AudioTabSidebar.tabSelected(index)` bubbling to `AudioPopupContent`'s
old `currentTab`) and by `NetworkActionRow.settingsRequested()`/`infoRequested()` — not a new
pattern for this codebase.

**No singleton, no persistence across opens.** `StatusPopupSurface::show()` (`libs/holonight-surfaces/
src/StatusPopupSurface.cpp`) unconditionally calls `destroySurface()` then `ensureSurface()` on
every `show()` call — the `QQuickView` and its entire QML tree (including `AudioPopupContent`) are
destroyed and rebuilt from scratch each time the popup opens, even if it was already showing a
different popup id. `outputExpanded: true` as a plain declared default therefore already satisfies
REQ-F-7002 with no explicit reset code — there is nothing to reset, because nothing survives
between opens. Same reasoning applies to `AudioApplicationsSection.showAll` (§2.5) — it does not
need to be wired back to `false` on close.

---

## 4. C++ Struct / Model Changes

### 4.1 `AudioTypes.h` — `AudioDevice` additions

```cpp
struct AudioDevice {
  uint32_t id{0};
  QString name;
  QString description;
  uint8_t volume{0};
  bool muted{false};
  bool is_default{false};
  AudioDeviceType type{AudioDeviceType::Sink};
  // --- new (REQ-F-13001) ---
  QString busType;         // "Analog" | "Digital" | "Bluetooth" | "Unknown"
  uint8_t channelCount{0};
  uint32_t sampleRate{0};
  QString codec;           // empty for non-Bluetooth devices
  QString iconName;        // e.g. "audio-speakers"; empty falls back to ExternalIcon.fallbackIconName
};
```

Purely additive; existing fields and their order are untouched (REQ-C-15007).

### 4.2 `AudioDeviceModel` — new roles + a computed `defaultDevice` property

`AudioDeviceModel::Role` (`libs/holonight-services/src/audio/AudioDeviceModel.h`) appends after
`IsDefault` — do not reorder or insert before existing entries, since the numeric values are
implicit (`Qt::UserRole + 1` auto-incrementing):

```cpp
enum class Role : uint16_t {
  DeviceId = Qt::UserRole + 1,
  Name, Description, Volume, Muted, IsDefault,
  BusType, ChannelCount, SampleRate, Codec, IconName,   // new
};
```

`data()` and `roleNames()` extend with the matching camelCase keys (`"busType"`,
`"channelCount"`, `"sampleRate"`, `"codec"`, `"iconName"`), consistent with the existing
`"deviceId"`/`"isDefault"` naming — QML delegates read them the same way they already read
`root.model.isDefault` today.

**New: `Q_PROPERTY(QVariantMap defaultDevice READ defaultDevice NOTIFY defaultDeviceChanged)`.**
This is the piece SPEC.md doesn't address: `AudioCurrentDeviceRow` needs the *currently-default*
device's full field set (icon, name, metadata, volume) and must stay live as that device's own
volume/mute/metadata changes — not just when the default *identity* changes. `AudioDeviceModel`
already stores every device it needs to scan; a `QAbstractListModel` has no idiomatic "find the row
where role X is true" from QML without a proxy model, so the model computes and caches the answer
itself:

```cpp
// AudioDeviceModel.h
[[nodiscard]] QVariantMap defaultDevice() const { return default_device_; }
Q_SIGNAL void defaultDeviceChanged();
```

Private helper `refreshDefaultDevice()` scans `devices_` for `is_default`, builds a `QVariantMap`
with the same keys as `roleNames()` (or an empty map if no device is currently default — e.g.
before the first PA callback lands), and is called at the end of `applyAdd`, `applyChange`, and
`applyRemove`. It compares the new map against the cached `default_device_` with `QVariantMap`'s
built-in `operator==` and only reassigns + emits `defaultDeviceChanged()` on an actual difference —
the same "compare, then assign-and-emit" idiom already used everywhere in `AudioService`
(`applyVolume`, `applyMuted`, `setAvailable`, `applyDefaultDeviceState`). `outputs.defaultDevice`
and `inputs.defaultDevice` are two independently-computed instances (same class, two objects), so
no cross-talk between the two `AudioDeviceSection` instances is possible.

QML then binds directly and reactively:

```qml
readonly property var defaultDeviceInfo: root.isInput ? AudioService.inputs.defaultDevice : AudioService.outputs.defaultDevice
// AudioCurrentDeviceRow { title: defaultDeviceInfo.description; iconName: defaultDeviceInfo.iconName; ... }
```

Nested-property change propagation (`defaultDeviceInfo.description`) is native QML behavior once
`defaultDeviceInfo` itself re-binds on `defaultDeviceChanged()` — no `Connections` boilerplate.

### 4.3 `PulseAudioBackend` metadata extraction

`Impl::sinkToDevice()` / `Impl::sourceToDevice()` (`libs/holonight-services/src/audio/
PulseAudioBackend.cpp`) extend to populate the five new fields, reading directly off the
`pa_sink_info*`/`pa_source_info*` the callback already receives — no new PulseAudioSystem seam
methods needed here (these are plain struct/proplist reads, not new libpulse calls):

- `channelCount`: `info->channel_map.channels` (`uint8_t`, direct — matches the struct field type
  exactly, REQ-F-13003).
- `sampleRate`: `info->sample_spec.rate` (`uint32_t`, direct).
- `iconName`: `pa_proplist_gets(info->proplist, PA_PROP_DEVICE_ICON_NAME)`; if null/empty, derive
  from `PA_PROP_DEVICE_FORM_FACTOR` (`"headphone"`→`"audio-headphones"`, `"headset"`/
  `"hands-free"`→`"audio-headset"`, `"speaker"`/`"internal"`/empty→`"audio-speakers"` for sinks or
  `"audio-input-microphone"` for sources, `"microphone"`→`"audio-input-microphone"`); if still
  empty, leave `iconName` empty and let `AudioDeviceDelegate`'s `ExternalIcon.fallbackIconName`
  (§2.3) supply the generic icon — this satisfies REQ-F-4002's fallback requirement without
  duplicating fallback logic in two places.
- `busType`: classified by a new static free function `classifyBusType(const pa_proplist*)`
  (extracted as its own testable unit — also keeps `sinkToDevice`/`sourceToDevice` under the
  project's clang-tidy cognitive-complexity threshold of 25, per `.clang-tidy`):
  1. If `PA_PROP_DEVICE_FORM_FACTOR` is `"hdmi"`, or the device name/active-port name contains
     `"hdmi"`/`"spdif"`/`"iec958"` → `"Digital"`.
  2. Else if `pa_proplist_gets(proplist, PA_PROP_DEVICE_BUS)` (`"device.bus"`) is `"bluetooth"` →
     `"Bluetooth"`.
  3. Else if `device.bus` is `"usb"` → `"Digital"`.
  4. Else if `device.bus` is non-empty (`"pci"`, `"isa"`, ...) → `"Analog"`.
  5. Else → `"Unknown"` (REQ-F-3002 fallback).
- `codec`: only computed when `busType == "Bluetooth"`. Try, in order,
  `pa_proplist_gets(proplist, "bluez.codec_name")` (the key REQ-F-13003 names explicitly), then
  `"bluetooth.codec"` (a key used by some bluez5-module versions in the wild — see §11, risk 1),
  falling back to `"PCM"` if neither is present (REQ-F-3002). For non-Bluetooth devices, `codec`
  stays empty.
- **Logging**: REQ-F-3002 wants a warning "when a metadata query returns null but a device is
  otherwise valid," without spamming. Guard with a `QSet<uint32_t> metadata_warned_ids_` member on
  `Impl` — warn once per device id the first time any of its metadata fields come back empty/zero,
  not on every re-query; erase the id from the set in the sink/source-removed handlers so a
  re-added device (e.g. USB mic unplugged/replugged) gets a fresh warning if it recurs.

None of this needs new `PulseAudioSystem` virtual methods — `sinkListCallback`/`sourceListCallback`
etc. already receive the full `pa_sink_info*`/`pa_source_info*` struct synchronously from libpulse
regardless of which system implementation is active; `test_pulse_audio_backend.cpp`'s
`FakePulseAudioSystem` already constructs these structs by hand to invoke the callbacks, so the new
field extraction is exercised by simply setting the relevant `proplist`/`channel_map`/`sample_spec`
members on those hand-built fixtures.

---

## 5. Live Input Level Monitoring

### 5.1 Spec correction (grounding, not scope change)

REQ-F-8001 says to open "a `pa_stream`... in monitor mode against the default input source
(`pa_source_info.monitor_source_name` or equivalent)." `monitor_source_name` is a field on
`pa_sink_info` (a sink's own loopback-monitor *source*, used to capture what's playing back through
that sink) — it does not exist on `pa_source_info`. A microphone (a source) is monitored by
connecting a record stream **directly to the source itself**, not to a separate "monitor of the
source." This is the standard technique pavucontrol uses for its own input-level meters:
`pa_stream_connect_record()` on the source's name with the `PA_STREAM_PEAK_DETECT` flag. The design
below uses that mechanism; REQ-F-8001's wording is treated as directional intent ("show live mic
level"), not a literal API contract, consistent with `docs/sdd/battery-popup/DESIGN.md`'s precedent
of a "Spec Corrections vs. Source Reality" section for exactly this kind of Stage-1→Stage-2 gap.

### 5.2 `PulseAudioSystem` seam extension

`PulseAudioSystem` (`libs/holonight-services/src/audio/PulseAudioSystem.h`) currently wraps only
`pa_context_*` calls — no `pa_stream_*` methods exist. `test_pulse_audio_backend.cpp` exercises the
entire backend through `FakePulseAudioSystem`, and the project's established convention (see
`.clang-tidy`/CLAUDE.md testing emphasis, and the POC remediation history in project memory) is
that every libpulse call the backend makes is seam-wrapped and unit-testable, not just live-tested.
**Decision: extend `PulseAudioSystem` (and `RealPulseAudioSystem`) with the minimal set of
`pa_stream_*` methods needed**, rather than calling libpulse's free `pa_stream_*` functions
directly from `PulseAudioBackend.cpp` (which would be the only untested libpulse call site in the
class):

```cpp
virtual pa_stream* pa_stream_new(pa_context* c, const char* name, const pa_sample_spec* ss,
                                  const pa_channel_map* map) = 0;
virtual void pa_stream_set_state_callback(pa_stream* s, pa_stream_notify_cb_t cb, void* userdata) = 0;
virtual void pa_stream_set_read_callback(pa_stream* s, pa_stream_request_cb_t cb, void* userdata) = 0;
virtual int pa_stream_connect_record(pa_stream* s, const char* dev, const pa_buffer_attr* attr,
                                      pa_stream_flags_t flags) = 0;
virtual int pa_stream_peek(pa_stream* s, const void** data, size_t* bytes) = 0;
virtual int pa_stream_drop(pa_stream* s) = 0;
virtual int pa_stream_disconnect(pa_stream* s) = 0;
virtual void pa_stream_unref(pa_stream* s) = 0;
virtual pa_stream_state_t pa_stream_get_state(pa_stream* s) = 0;
```

`RealPulseAudioSystem` implementations are one-line forwards to `::pa_stream_*`, matching every
existing method in the file. `FakePulseAudioSystem` (test-only, `test_pulse_audio_backend.cpp`)
gets matching fakes that record calls and let tests drive the read/state callbacks synchronously —
the same shape as its existing `pa_context_get_sink_info_by_index` fake.

### 5.3 `PulseAudioBackend` API and lifecycle

```cpp
void PulseAudioBackend::startInputLevelMonitor();   // idempotent — no-op if already active
void PulseAudioBackend::stopInputLevelMonitor();    // idempotent — no-op if not active
Q_SIGNALS: void inputLevelChanged(int level);       // 0-100, matches REQ-F-8001's exact signature
```

`Impl` gains `pa_stream* level_stream_{nullptr}`. `startInputLevelMonitor()`:

1. No-op if `impl_->context == nullptr`, `impl_->mainloop == nullptr`, or `level_stream_ != nullptr`
   already.
2. `threaded_mainloop_lock`. Build `pa_sample_spec{.format = PA_SAMPLE_FLOAT32LE, .rate = 30,
   .channels = 1}` — a deliberately low nominal rate. `PA_STREAM_PEAK_DETECT` makes the *server*
   compute the true peak amplitude over each real audio period and deliver one decimated sample at
   this configured rate, so setting `rate = 30` directly gives ~30 Hz delivery (REQ-F-8002)
   without any client-side throttling/coalescing timer — the server does the rate-limiting for
   free. Each delivered FLOAT32 "sample" is already a peak magnitude in `[0.0, 1.0]`.
3. `pa_buffer_attr{.maxlength = UINT32_MAX, .fragsize = pa_frame_size(&sample_spec)}` (one frame
   per fragment → one read callback per delivered sample).
4. `pulseSystem()->pa_stream_new(impl_->context, "holonight-shell-input-level", &sample_spec,
   nullptr)` (null channel map → default mono mapping).
5. Set state + read callbacks (both static `Impl` members, `userdata = impl_.get()`).
6. `pa_stream_connect_record(stream, impl_->default_source_name.toUtf8().constData(), &attr,
   PA_STREAM_DONT_MOVE | PA_STREAM_PEAK_DETECT | PA_STREAM_ADJUST_LATENCY |
   PA_STREAM_AUTO_TIMING_UPDATE)`. `PA_STREAM_DONT_MOVE` keeps this diagnostic stream from being
   silently rerouted by module-stream-restore.
7. Unlock.

**Read callback** (mainloop thread, same threading discipline as every other PA callback in this
file): `pa_stream_peek(stream, &data, &bytes)`. If `data == nullptr && bytes == 0`, nothing
available, return. If `data == nullptr && bytes > 0`, a hole (dropped samples) — just
`pa_stream_drop()` and return. Otherwise read the `float` at `data[0]`, compute
`level = std::clamp(static_cast<int>(std::abs(sample) * 100.0f), 0, 100)`, `pa_stream_drop()`, then
marshal to the Qt thread exactly like every other callback in this file:
`QMetaObject::invokeMethod(self, [self, level] { emit self->inputLevelChanged(level); },
Qt::QueuedConnection)`.

**State callback**: on `PA_STREAM_FAILED`, emit `inputLevelChanged(0)` once (queued) and log at
most one warning (a `bool level_stream_warned_` flag, matching the "max 1 warning per session"
language in REQ-F-8001) — do **not** attempt to reconnect the level stream itself; it is a
diagnostic, best-effort stream, not part of the context's health-state machine. On
`PA_STREAM_TERMINATED`, no action needed (expected during normal `stopInputLevelMonitor()`
teardown).

`stopInputLevelMonitor()`: lock, if `level_stream_ != nullptr`: clear both callbacks (`nullptr`,
`nullptr` — mirrors `teardownContext()`'s own callback-clearing-before-disconnect order),
`pa_stream_disconnect`, `pa_stream_unref`, null the member; unlock; emit `inputLevelChanged(0)`
(queued) so a quick close/reopen never shows a stale peak before the first new sample arrives
(reinforces REQ-F-8003's "renders as empty" on open).

**Default-source change while monitoring is active** (not explicitly covered by SPEC.md, but a
real correctness gap if left unhandled): if the user picks a different input device from the
expanded Input Devices list while the popup — and therefore the level stream — is still open, the
meter would otherwise keep listening to the now-stale old source indefinitely. `serverInfoCallback`
already detects `default_source_name` changes on every `PA_SUBSCRIPTION_EVENT_SERVER` event; when
`level_stream_ != nullptr` and the default source name changed, tear down and re-open the level
stream against the new name in the same callback. This is a small addition beyond the SPEC's
literal text, justified as necessary baseline correctness rather than deferred scope.

**Context loss / reconnect**: `Impl::teardownContext()` additionally tears down `level_stream_` (if
non-null) before disconnecting the context, using the same lock already held. A fresh
`startInputLevelMonitor()` call is not auto-re-issued after reconnect — `AudioService` doesn't know
whether the popup is still open by the time a reconnect completes seconds later — so a stream lost
to a context reconnect stays down until the popup is closed and reopened (acceptable: reconnects
are already rare/backoff-gated, and `AudioHealthState::Reconnecting` is visible elsewhere in the
shell).

### 5.4 `AudioService` surface

```cpp
Q_PROPERTY(int inputLevel READ inputLevel NOTIFY inputLevelChanged)
Q_INVOKABLE void startInputLevelMonitoring();
Q_INVOKABLE void stopInputLevelMonitoring();
Q_SIGNAL void inputLevelChanged(int level);
```

Both invokables forward to `backend_->startInputLevelMonitor()`/`stopInputLevelMonitor()` guarded
by `backend_ != nullptr` — the same null-check pattern every other `AudioService` invokable already
uses, so they're safe no-ops under `AudioService(SkipInit)` in tests. `inputLevelChanged` is
connected straight through (`connect(backend_, &PulseAudioBackend::inputLevelChanged, this,
&AudioService::inputLevelChanged)`), and the handler updates the cached `int input_level_` and
re-emits only on change — same idiom as `applyVolume`. Note this signal *does* carry a payload
(`int level`), unlike `AudioService`'s other change signals (`volumeChanged()`,
`mutedChanged()`, payload-less) — deliberately matching REQ-F-8001's literal
`inputLevelChanged(int level)` signature so QML can bind either to the property (`AudioService.
inputLevel`) or listen to the signal directly.

### 5.5 QML lifecycle hook: `InputLevelMeter.qml` owns its own data source

```qml
// InputLevelMeter.qml
property int level: AudioService.inputLevel
Component.onCompleted: AudioService.startInputLevelMonitoring()
Component.onDestruction: AudioService.stopInputLevelMonitoring()
```

`InputLevelMeter` is instantiated exactly once, as `AudioCurrentDeviceRow.trailingContent` inside
the Input `AudioDeviceSection` — and per §3, the Input summary row is always visible whenever the
popup is open (only its *device list* collapses under REQ-F-7001), so "meter component exists" and
"popup is open" are the same fact. Tying monitor start/stop to the meter's own `Component.
onCompleted`/`onDestruction` satisfies REQ-F-8001 ("created lazily... destroyed when popup closes")
exactly, without needing to watch `StatusPopupSurface.popupVisible` from inside the popup's own
content tree. This is a different situation from the CLAUDE.md warning against deleting a
`QQuickView` from its own QML handler — `AudioService` is a long-lived singleton unrelated to the
view being torn down; calling an invokable on it from `Component.onDestruction` is ordinary cleanup,
not a self-destruction race.

---

## 6. Master Mute — `setDefaultOutputMuted`

```cpp
// AudioService.h
Q_INVOKABLE void setDefaultOutputMuted(bool muted);

// AudioService.cpp
void AudioService::setDefaultOutputMuted(bool muted) {
  if (backend_ == nullptr || default_output_id_ == kInvalidId) {
    return;
  }
  backend_->setDeviceMuted(default_output_id_, muted);
}
```

Copies `setVolume(int)`'s exact guard shape (backend-null check + `kInvalidId` check) — no new
state, `AudioService::muted` continues to update only via the existing PulseAudio echo path
(`applyMuted`, invoked from `applyDefaultDeviceState` when a `deviceChanged` event reports the
default sink's new mute state), so mute never goes optimistic/out-of-sync with PulseAudio. QML:
`AudioMasterPanel`'s mute button `onClicked: AudioService.setDefaultOutputMuted(!AudioService.muted)`.

---

## 7. Radio-Checkmark Selection Indicator

**Lives inline in `AudioDeviceDelegate.qml`**, not as a separate shared file. It is small (a filled
or outlined ~16px circle plus, when filled, a short tick path), used in exactly one place (this
delegate's two instantiations — output and input rows share the one file already), and reading
`isDefault` off the same `root.model` the rest of the delegate already binds to needs no new
plumbing:

```qml
// inside AudioDeviceDelegate.qml leadingContent, alongside the existing ExternalIcon
RowLayout {
  spacing: 8
  Rectangle {                                   // radio circle
    width: 16; height: 16; radius: 8
    color: root.isDefault ? root.accentColor : "transparent"
    border.color: root.isDefault ? root.accentColor : HoloniightPalette.borderPassive
    border.width: 1.5
    Shape {                                      // checkmark tick, visible only when filled
      anchors.centerIn: parent
      visible: root.isDefault
      preferredRendererType: Shape.CurveRenderer
      ShapePath {
        fillColor: "transparent"
        strokeColor: HoloniightPalette.onSurface   // tick reads against the accent-filled circle
        strokeWidth: 1.6
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin
        PathSvg { path: "M3 8l3 3 6-7" }
      }
    }
  }
  ExternalIcon { /* existing device-type icon, unchanged */ }
}
```

The tick reuses the same `Shape`/`ShapePath`/`PathSvg` stroke-glyph technique already established
by `NetworkPopupIcon.qml` (§2.6/2.7), just inlined rather than added as a new named glyph there,
since it needs a filled background circle behind it that the other glyphs don't.

**No separate click handling.** The radio glyph has no `MouseArea`/`TapHandler` of its own; it sits
inside `leadingContent`, which is laid out on top of `HnSelectableDelegate`'s (the base of
`HnListDelegate`) own background/pointer handling. Clicking anywhere in the row — including on the
radio glyph — already reaches the delegate's existing `onClicked` (which calls
`setDefaultOutputByName`/`setDefaultInputByName`, unchanged). REQ-F-11001's "radio-circle is
clickable and calls..." is satisfied by construction, not by new wiring.

---

## 8. Formatting & List-Capping Decisions

### 8.1 Metadata subtitle — computed in QML, not C++

**Decision: a small shared JS function, not a C++ pre-formatted string field.**

Rationale:
- Keeps the `AudioDevice`/model roles as raw typed data (`busType`, `channelCount: uint8_t`,
  `sampleRate: uint32_t`, `codec`) reusable for anything else that might need them individually —
  a pre-joined C++ string would bake wording/locale/punctuation into the backend.
- REQ-F-3002's fallback rules (omit channel count if 0, "Unknown" bus type, etc.) are presentation
  decisions about what to *hide*, which is exactly the kind of logic every other formatted string
  in these files already handles in QML (e.g. `AudioDeviceDelegate.qml`'s existing
  `title`/`subtitle` ternaries choosing between `description`/`name`).
- Every other user-facing string in this popup is already `qsTr()`'d in QML — keeping this one
  there too avoids a second localization path.
- Both `AudioDeviceDelegate.qml` (list rows) and `AudioCurrentDeviceRow.qml` (summary row) need
  the *identical* format, and REQ-F-3003's exact punctuation ("•" separators, lowercase
  "channels", "kHz") is itself an acceptance criterion — duplicating the ternary chain in two files
  risks drift the next time wording changes.

There is no existing `.js` helper module anywhere in `apps/shell/qml/` today (checked — none of
the ~50+ existing shell QML files use one), so this introduces the first one for this codebase.
Given the narrow, pure-string-transform need and the two call sites, that's a reasonable first use
rather than a new pattern imposed prematurely:

```js
// AudioMetadataFormat.js
function formatDeviceMetadata(busType, channelCount, sampleRate, codec) {
    const bus = busType && busType.length > 0 ? busType : qsTr("Unknown")
    const rate = sampleRate > 0 ? formatLocalizedKilohertz(sampleRate) : ""
    const middle = bus === "Bluetooth"
        ? (codec && codec.length > 0 ? codec : qsTr("Unknown"))
        : (channelCount > 0 ? qsTr("%1 channels").arg(channelCount) : "")
    return [bus, middle, rate].filter(part => part.length > 0).join(" • ")
}
```

Imported as `import "AudioMetadataFormat.js" as AudioMetadataFormat` in both delegate files;
`subtitle: AudioMetadataFormat.formatDeviceMetadata(root.model.busType, root.model.channelCount,
root.model.sampleRate, root.model.codec)`.

### 8.2 `AudioCurrentDeviceRow`'s subtitle

Same function, reading off `defaultDeviceInfo` (§4.2) instead of a list delegate's `model` role —
identical shape (`QVariantMap` keys match `roleNames()` keys), so the same JS call works unchanged:
`AudioMetadataFormat.formatDeviceMetadata(defaultDeviceInfo.busType, defaultDeviceInfo.
channelCount, defaultDeviceInfo.sampleRate, defaultDeviceInfo.codec)`.

### 8.3 Application Volume 4-row cap — container height cap, not a filtered model

**Decision: cap the visible height of the existing `AudioStreamList`, not a new C++/QML filtered
model, and not per-delegate `visible` bindings.**

`AudioApplicationsSection.qml` wraps the unchanged `AudioStreamList` (bound to the unchanged
`AudioService.playbackStreams`) in a fixed container:

```qml
Item {
  id: clipContainer
  clip: true
  readonly property real collapsedHeight: 4 * 64 + 3 * streamList.spacing   // AudioStreamDelegate.implicitHeight is 64
  height: root.showAll ? streamList.contentHeight : Math.min(streamList.contentHeight, collapsedHeight)
  Behavior on height { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

  AudioStreamList {
    id: streamList
    interactive: false        // this sub-list never scrolls on its own — REQ-C-15006's "whole
                               // column scrolls" principle applies here too; only the outer
                               // popup column (REQ-F-1001) scrolls
    anchors.top: parent.top
    width: parent.width
  }
}
```

Toggling `showAll` only changes a height number; no model reset, no row destruction/recreation, no
interaction with `ListView` virtualization/recycling. This was chosen over per-row `visible: index
< 4 || showAll` because a `ListView` does **not** collapse an individual delegate's reserved
vertical space just because that delegate is `visible: false` — each item still occupies
`height` px in the view's layout unless its `height` is *also* bound to 0 when hidden, which is
extra plumbing the container-clip approach avoids entirely. It was also chosen over a new
`QSortFilterProxyModel`/C++ capped-count model because the requirement is purely a UI row-count
affordance (REQ-F-9001/9002), not content filtering — introducing a new C++ model class for a
4-vs-all visual toggle would be disproportionate machinery for what a single `height` binding
already solves. `Math.min(..., collapsedHeight)` means fewer-than-4 streams render at their natural
height with no dead space (REQ-F-9001: "if fewer than 4 streams exist, all are shown").

### 8.4 "Show All" toggle — plain `MouseArea`, not `TapHandler`

CLAUDE.md flags that `TapHandler`'s default `gesturePolicy` (`DragThreshold`) cancels a tap if the
pointer drifts during press, unlike `MouseArea.onClicked`'s more forgiving press+release-anywhere-
inside-bounds behavior, and that this exact swap caused a live regression elsewhere in this repo
that passed lint/build/automated tests. No file in the current Audio popup uses `TapHandler`
anywhere (`AudioMasterBar.qml`, `AudioVolumeSlider.qml`, `AudioDeviceDelegate.qml`'s mute button
all use plain `MouseArea`). The "Show All" toggle uses `MouseArea` for the same reason and to avoid
reintroducing a known-risky pattern into a file where nothing already uses it.

---

## 9. Data Flow

```
PulseAudio server
   │  (pa_context subscription: SINK | SOURCE | SINK_INPUT | SOURCE_OUTPUT | SERVER)
   ▼
PulseAudioBackend::Impl static callbacks         [PA mainloop thread]
   sinkListCallback / sourceListCallback / *ChangedCallback
     → sinkToDevice()/sourceToDevice() populate AudioDevice incl. busType/channelCount/
       sampleRate/codec/iconName (classifyBusType(), PA_PROP_* proplist reads)
   Impl::level read callback (new, §5.3)
     → pa_stream_peek() → float peak sample → level = clamp(|sample| * 100, 0, 100)
   │  QMetaObject::invokeMethod(..., Qt::QueuedConnection)   — thread hop, existing pattern
   ▼
PulseAudioBackend (QObject)                      [Qt GUI thread]
   emit deviceAdded/deviceChanged(AudioDevice) | inputLevelChanged(int)
   │  connected in AudioService::start()
   ▼
AudioService
   onDeviceAdded/onDeviceChanged → AudioDeviceModel::applyAdd/applyChange
     (beginInsertRows/dataChanged over ALL roles incl. new ones; refreshDefaultDevice() → maybe
      defaultDeviceChanged())
   inputLevelChanged(int) → cache input_level_, re-emit AudioService::inputLevelChanged(level)
   │
   ▼
QML (AudioDeviceModel exposed as AudioService.outputs / .inputs; AudioService.inputLevel property)
   AudioDeviceList → AudioDeviceDelegate         reads model.busType/channelCount/sampleRate/
                                                  codec/iconName/isDefault per row
   AudioDeviceSection.defaultDeviceInfo          = {outputs|inputs}.defaultDevice (QVariantMap)
   AudioCurrentDeviceRow                         reads defaultDeviceInfo.* (always the live default)
   InputLevelMeter.level                         = AudioService.inputLevel  (~30 Hz, server-decimated)
```

User-initiated writes flow the other direction through the same invokables that already exist
(`setDeviceVolume`, `setDefaultOutputByName`, `setStreamVolume`, ...) plus the two new ones
(`setDefaultOutputMuted`, §6) — none of that plumbing changes shape, only the read side gains the
new fields and the level stream.

---

## 10. Key Decisions — Summary Table

| Decision | Choice | Rationale (see section) |
|---|---|---|
| Output vs. Input sections | One `AudioDeviceSection.qml`, `isInput: bool` | §2.1 — matches existing `AudioDeviceList.qml` convention |
| Accordion state owner | Single bool on `AudioPopupContent.qml` | §3 — structurally rules out the illegal both-expanded/both-collapsed states |
| "Current default device" data source | New `AudioDeviceModel.defaultDevice` `QVariantMap` property | §4.2 — no idiomatic "find row by role" from QML otherwise |
| Metadata subtitle formatting | Shared QML `.js` function | §8.1 — presentation logic, consistent with every other formatted string in these files |
| Application 4-row cap | Container height cap over unchanged model | §8.3 — smaller, no virtualization interaction, no new model class |
| Radio-checkmark | Inline in `AudioDeviceDelegate.qml` | §7 — single consumer, reuses existing model binding |
| Chevron / gear glyphs | Reuse `NetworkPopupIcon.qml` as-is | §2.6/2.7 — already exists, flat module makes it trivially importable |
| `pa_stream_*` calls | New `PulseAudioSystem` virtual methods | §5.2 — keeps the level monitor unit-testable like everything else in the backend |
| Level sample rate | PA-side decimation via low nominal stream rate (30 Hz) | §5.3 — no client-side throttling timer needed |
| Show All toggle input | `MouseArea`, not `TapHandler` | §8.4 — sidesteps a documented live regression pattern |

---

## 11. Alternatives Considered

- **Two files (`AudioOutputDeviceSection.qml` + `AudioInputSection.qml`) as SPEC.md's file list
  literally names them.** Rejected: near-total duplication (every future spacing/behavior/label
  tweak has to land twice), when the only genuine differences (accent color, model, default-setter
  target, meter presence) are already exactly the shape `AudioDeviceList.qml` handles today with a
  single boolean.
- **C++-side pre-formatted metadata string** (e.g. an extra `QString formattedSubtitle` field
  computed once in `PulseAudioBackend` and stored on `AudioDevice`). Rejected: bakes wording into
  the backend, duplicates formatting logic between the eventual C++ and any future QML-side
  variant, and breaks the "omit channel count if 0" fallback cleanly expressible as a one-line JS
  ternary but awkward as backend string-building sprinkled through `sinkToDevice`/`sourceToDevice`.
- **Duplicating the metadata-format ternary directly in both QML files instead of a shared `.js`.**
  Considered as the lower-ceremony option (no new file type introduced). Rejected in favor of the
  shared function specifically because REQ-F-3003's punctuation is an acceptance criterion, and
  drift between two independently-hand-written copies is a realistic, cheap-to-avoid risk.
- **Per-delegate `visible: index < 4 || showAll` for the Applications cap.** Rejected: `ListView`
  delegates keep occupying their `height` in the view's content layout even when `visible: false`
  unless height is separately zeroed — extra plumbing the container-height-cap approach avoids.
- **Calling `pa_stream_*` libpulse functions directly from `PulseAudioBackend.cpp`, bypassing
  `PulseAudioSystem`.** Rejected: would be the only untested libpulse call path in a class whose
  entire existing surface is deliberately seam-wrapped and covered by `test_pulse_audio_backend.
  cpp`'s `FakePulseAudioSystem`.
- **Watching `StatusPopupSurface.popupVisible` from `AudioPopupContent.qml` to start/stop the level
  monitor**, instead of tying it to `InputLevelMeter`'s own `Component.onCompleted`/
  `onDestruction`. Rejected: adds a cross-cutting dependency on a singleton the Input meter doesn't
  otherwise need, for no behavioral difference — the meter's own instantiation lifetime already
  equals "popup open," because the whole content tree is destroyed/rebuilt on every `show()`
  (§3).
- **A new `InputLevelMonitor` class** (as SPEC.md's file list floats as an option: "new
  `InputLevelMonitor` class or enhancement to `AudioService`"). Rejected in favor of adding the
  stream lifecycle directly to `PulseAudioBackend::Impl`: the level stream needs the same
  `context`/`mainloop` the rest of the backend already owns and locks, and there's exactly one
  consumer (`AudioService`) — a separate class would need its own handle to the same context
  without adding any real separation of concerns.

---

## 12. Known Risks

1. **PulseAudio proplist key variability.** `device.bus`, `device.form_factor`, and Bluetooth codec
   keys are populated inconsistently across drivers and across native PulseAudio vs. PipeWire's
   pulse-compatibility shim — some environments may leave every classification falling through to
   `"Unknown"`. This is exactly why REQ-F-3002 mandates a fallback; still worth an early manual
   check on the actual target system (`pactl list sinks`/`sources` to see what proplist keys are
   really populated there) before assuming the classification heuristic in §4.3 produces useful
   results rather than mostly "Unknown."
2. **Bluetooth codec proplist key name is genuinely uncertain.** REQ-F-3001 cites
   `PA_PROP_BLUETOOTH_CODEC` (not a real libpulse macro to our knowledge) while REQ-F-13003 cites
   the literal string `"bluez.codec_name"`. §4.3's design tries `"bluez.codec_name"` then
   `"bluetooth.codec"` before falling back to `"PCM"` — treat both as best-guesses to be confirmed
   against a real paired Bluetooth device during implementation, not as verified facts.
3. **First `pa_stream` (as opposed to `pa_context_get_*_info`) anywhere in this codebase.**
   `AudioChannelSource` (`libs/holonight-services/src/osd/`) — the only other place mic/output
   level-adjacent code exists — is a pure `AudioService` signal observer with no PulseAudio stream
   of its own; there is no prior art here for stream-mode lifecycle bugs (state callback races,
   mid-flight failures before the first read callback, etc.). Budget real live-testing time for
   this piece specifically (`task compositor-smoke-check`-style manual pass with a real microphone,
   including unplug/replug and switching default input while the popup is open).
4. **Default-source change while the level stream is active** is handled (§5.3) but is an addition
   beyond SPEC.md's literal text — flag during review that this behavior was inferred as necessary,
   not requested verbatim.
5. **Removing all per-row/per-app mute buttons** (REQ-F-5002/6002/9001's row-content lists, §2.3/
   2.4) leaves `AudioMasterPanel`'s mute button (§6) as the *only* mute affordance left in this
   popup — `AudioService.setDeviceMuted`/`setInputDeviceMuted`/`setStreamMuted` remain fully
   functional on the backend (still reachable by other future UI, e.g. a future per-app menu) but
   have no entry point in this popup for anything other than the default output. Confirmed
   intentional by the SPEC's own acceptance criteria; flagged here for product visibility since
   SPEC.md's constraints section (REQ-C-15001..15007) never states it as a deliberate removal.
6. **Accordion collapse vs. Tab focus.** If a user has tabbed focus into a control inside the
   currently-expanded device list and then collapses that section (or the other section's
   expansion implicitly collapses it), Qt's usual behavior is to move focus along the chain past
   now-invisible items — but REQ-C-15004 forbids adding any *new* keyboard handling to compensate,
   so this needs a live check (`task compositor-smoke-check`) rather than an assumption that
   "nothing new was added, so it must already work."
7. **Longer metadata subtitle string vs. existing layout width.** "Analog • 2 channels • 48 kHz"
   is meaningfully longer than the current subtitle (the device's raw PA name); `HnListDelegate`'s
   `SingleLine` presentation already elides, but the popup's actual fixed content width (set by
   `StatusPopup.qml`'s `contentLoader.width`) should be checked live to confirm the Hz value isn't
   the part that gets clipped.
8. **`PulseAudioSystem` interface growth.** Adding ~9 `pa_stream_*` virtual methods to an interface
   that already has ~25 enlarges `RealPulseAudioSystem`/any fake's maintenance surface; accepted
   as the cost of keeping the level monitor unit-testable (§5.2), not treated as a reason to skip
   the seam.

---

## 13. Test Plan

### 13.1 GTest (no Wayland session required)

- **`test_audio_device_model.cpp`** (extend): new roles present in `roleNames()`; `data()` returns
  the five new fields correctly for a populated device; `defaultDevice` property starts as an
  empty `QVariantMap` with no devices added; becomes populated (and `defaultDeviceChanged()` fires
  exactly once) when a device with `is_default = true` is added; re-emits when the default row's
  own volume/mute/metadata changes while remaining default; does *not* re-emit on an unrelated
  non-default row's change; goes back to an empty map (with a signal) when the default device is
  removed.
- **`test_pulse_audio_backend.cpp`** (extend): unit-test `classifyBusType()` as an extracted free
  function directly against hand-built `pa_proplist`-equivalent fixtures for each branch
  (hdmi/spdif form-factor, bluetooth bus, usb bus, pci/other bus, empty); `sinkToDevice()`/
  `sourceToDevice()` populate `channelCount`/`sampleRate` from the fixture's `channel_map`/
  `sample_spec`; codec fallback chain (`bluez.codec_name` present → used; absent but
  `bluetooth.codec` present → used; both absent on a Bluetooth-classified device → `"PCM"`;
  non-Bluetooth device → always empty). Extend `FakePulseAudioSystem` with the new `pa_stream_*`
  methods (§5.2) and add: `startInputLevelMonitor()` calls `pa_stream_new`/`connect_record` with
  the expected sample spec/flags; feeding a fake read callback with a FLOAT32 sample emits
  `inputLevelChanged` with the correctly scaled/clamped percent; a fake `PA_STREAM_FAILED` state
  emits `inputLevelChanged(0)` and logs at most one warning across repeated failures;
  `stopInputLevelMonitor()` clears both stream callbacks before disconnecting/unref'ing (order
  matters — assert call order via the fake's recorded call log, same technique the file already
  uses for `teardownContext()`); a `default_source_name` change while monitoring is active tears
  down and reconnects the stream to the new name.
- **`test_audio_service.cpp`** (extend): `setDefaultOutputMuted` no-ops safely under
  `AudioService(SkipInit)` (mirrors the existing `SetVolumeIsNoOpWithoutBackend` test); with a
  backend, delegates to `setDeviceMuted(defaultOutputId(), muted)` — assert via a fake/spy backend
  the same way existing device-control tests already assert delegation; `startInputLevelMonitoring`/
  `stopInputLevelMonitoring` no-op safely under `SkipInit`; `inputLevelChanged` forwards from
  backend to service unchanged.

### 13.2 QtQuickTest (`tst_*.qml`, offscreen via `test_holonight_qml_harness`)

- Extend `tests/qml/tst_AudioSharedControls.qml` or add a new `tst_AudioPopupRedesign.qml`:
  - Accordion mutual exclusivity: instantiate `AudioPopupContent` (or `AudioDeviceSection` pairs
    directly with a shared parent state), click the Output row → Input's device list is not
    visible and vice versa; assert exactly one `AudioDeviceList` is visible at any point.
  - Default state on creation: `outputExpanded === true`, Output list visible, Input list not.
  - `AudioDeviceDelegate` radio-circle: `findChild` the circle `Rectangle`, assert its `color`
    switches between `"transparent"` and `accentColor` as the row's `isDefault` role flips (same
    `list.model.setProperty(...)` technique the existing test file already uses).
  - `AudioMetadataFormat.js`'s `formatDeviceMetadata` is directly callable from a `TestCase`
    function (pure JS, no QML instantiation needed) — cover: normal device, zero channel count
    (omitted), zero sample rate (omitted), Bluetooth with codec present, Bluetooth with codec
    missing (falls back per whatever `codec` value the C++ layer already resolved to, since the
    JS function itself does not re-apply the `"PCM"` fallback — that fallback lives in C++, §4.3).
  - Applications 4-row cap: a `ListModel` with 6 stream entries; assert `clipContainer.height`
    equals the 4-row `collapsedHeight` initially, and equals `streamList.contentHeight` after
    `showAll = true`.
  - Disabled gear/`"..."` buttons: `findChild` fails to find any `MouseArea`/`TapHandler` attached
    to those items (structural absence, not a "click and assert nothing happened" test) — matches
    REQ-C-15002/15003's own acceptance criteria phrasing ("code review confirms no click handler").
  - `InputLevelMeter` smoke test: instantiate directly with `level` set to 0/50/100, assert bar
    heights track proportionally and no "loading"/"error" text `Item` exists at any level (REQ-F-8003).
    This cannot exercise the real `pa_stream` path (no live PA in the offscreen harness) — that
    remains GTest's job (13.1) plus live manual testing (13.3).

### 13.3 Manual / live testing (per project convention — never drive shell UI programmatically;
provide a checklist and wait for confirmation)

Follows SPEC.md's own "Acceptance Test Plan → Manual Testing Checklist" (items 1–12), plus:
real-microphone level meter behavior (talk/silence/clipping at max volume), switching the default
input device while the popup is open and confirming the meter follows without a stale reading,
unplugging a USB mic while its device row is visible, and a `task compositor-smoke-check` pass for
Tab-focus traversal through the new accordion structure (risk 6, §12).

---

## 14. Files to Create

```
apps/shell/qml/Popups/Audio/AudioPopupHeader.qml
apps/shell/qml/Popups/Audio/AudioMasterPanel.qml        (replaces AudioMasterBar.qml)
apps/shell/qml/Popups/Audio/AudioDeviceSection.qml
apps/shell/qml/Popups/Audio/AudioCurrentDeviceRow.qml
apps/shell/qml/Popups/Audio/InputLevelMeter.qml
apps/shell/qml/Popups/Audio/AudioApplicationsSection.qml
apps/shell/qml/Popups/Audio/KeyboardHintFooter.qml
apps/shell/qml/Popups/Audio/AudioMetadataFormat.js
```

## 15. Files to Edit

```
apps/shell/qml/Popups/Audio/AudioPopupContent.qml       (full rewrite — single scrollable column)
apps/shell/qml/Popups/Audio/AudioDeviceDelegate.qml      (radio indicator, device icon role, metadata
                                                           subtitle, remove per-row mute button)
apps/shell/qml/Popups/Audio/AudioStreamDelegate.qml      (remove per-row mute button, add disabled "...")

libs/holonight-services/src/audio/AudioTypes.h            (5 new AudioDevice fields)
libs/holonight-services/src/audio/AudioDeviceModel.h      (5 new roles, defaultDevice property+signal)
libs/holonight-services/src/audio/AudioDeviceModel.cpp    (data()/roleNames() extension, refreshDefaultDevice())
libs/holonight-services/src/audio/PulseAudioBackend.h     (startInputLevelMonitor/stopInputLevelMonitor,
                                                           inputLevelChanged signal, level_stream_ member)
libs/holonight-services/src/audio/PulseAudioBackend.cpp   (metadata extraction, classifyBusType(),
                                                           level-stream lifecycle + callbacks)
libs/holonight-services/src/audio/PulseAudioSystem.h      (9 new pa_stream_* virtual methods +
                                                           RealPulseAudioSystem forwards)
libs/holonight-services/src/audio/AudioService.h          (setDefaultOutputMuted, inputLevel property,
                                                           startInputLevelMonitoring/stopInputLevelMonitoring,
                                                           inputLevelChanged signal)
libs/holonight-services/src/audio/AudioService.cpp        (implementations, all additive)

tests/test_audio_device_model.cpp                         (new-role + defaultDevice coverage)
tests/test_pulse_audio_backend.cpp                         (classifyBusType, metadata extraction,
                                                           FakePulseAudioSystem pa_stream_* fakes,
                                                           level-monitor lifecycle)
tests/test_audio_service.cpp                               (setDefaultOutputMuted, level monitoring
                                                           SkipInit no-ops)
tests/qml/tst_AudioSharedControls.qml                       (or new tst_AudioPopupRedesign.qml —
                                                           accordion, radio indicator, format function,
                                                           4-row cap, disabled buttons, meter smoke test)
```

## 16. Files to Delete

```
apps/shell/qml/Popups/Audio/AudioTabSidebar.qml
```

`AudioDeviceList.qml`, `AudioStreamList.qml`, and `AudioVolumeSlider.qml` are reused unchanged — no
edits needed to any of the three.

## 17. Files That Do NOT Need Changes

- `apps/shell/CMakeLists.txt` — QML files are picked up by the existing recursive glob
  (`file(GLOB_RECURSE HOLONIGHT_QML_FILES ... qml/*.qml)`); no per-file registration exists to
  update.
- `libs/holonight-surfaces/src/StatusPopupSurface.cpp` / `StatusPopup.qml` — the audio popup
  already has `contentSource` set to `AudioPopupContent.qml` and `showTitle: false`; the redesign
  builds its own header inside that content, no chrome-level change needed. The `image://icon/`
  provider is already registered on this view's engine (`StatusPopupSurface::ensureSurface()`),
  covering every `ExternalIcon`/ `image://icon/` usage in the new files without further wiring.
- `libs/holonight-services/src/osd/AudioChannelSource.{h,cpp}` — unrelated observer of
  `AudioService::volumeChanged`/`mutedChanged`; the new `inputLevelChanged`/`setDefaultOutputMuted`
  additions don't touch the OSD's volume/mute channel.

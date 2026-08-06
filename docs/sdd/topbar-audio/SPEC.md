# SPEC — topbar-audio

## Overview

The `topbar-audio` feature provides a system audio volume widget integrated into the holonight-shell top bar. The widget displays the current volume percentage and mute state of the default PulseAudio/PipeWire sink, allows users to adjust volume via scroll wheel, and gracefully adapts to audio server availability. This is implemented via a C++/Qt `AudioService` singleton that communicates with PulseAudio over D-Bus, paired with a QML `AudioSection` widget for rendering.

## Requirements

### Functional

#### REQ-F-001: AudioService singleton initialization
The `AudioService` C++ singleton shall establish a connection to the PulseAudio server and expose volume/mute state to QML.
**AC**:
- `AudioService` instance is accessible from QML via `import HolonightShell; AudioService`
- `AudioService.volume` property is readable immediately after application startup
- `AudioService.muted` property is readable immediately after application startup
- `AudioService.available` property reflects whether PulseAudio/PipeWire is running

#### REQ-F-002: PulseAudio D-Bus lookup
When `AudioService` initializes, the system shall retrieve the PulseAudio server socket address from `org.PulseAudio.ServerLookup1` on the session bus.
**AC**:
- `AudioService` reads the `Address` property from `/org/pulseaudio/server_lookup1`
- A secondary `QDBusConnection` is opened to the returned socket address
- If the lookup service is unavailable, `AudioService.available` is set to `false`

#### REQ-F-003: Default sink tracking
The `AudioService` shall monitor the default sink and follow changes when the user switches audio outputs.
**AC**:
- `AudioService` reads the `FallbackSink` property from `org.PulseAudio.Core1` on startup
- `AudioService` subscribes to the `FallbackSinkUpdated` signal on `org.PulseAudio.Core1`
- When `FallbackSinkUpdated` fires, `AudioService` updates its internal sink object reference within 500 ms
- Volume and mute properties reflect the new sink within 1 second of a sink change

#### REQ-F-004: Volume property exposure
The `AudioService` shall expose the default sink's volume as an integer percentage (0–100).
**AC**:
- `AudioService.volume` returns an integer in the range 0–100 (inclusive)
- The value is computed from the PulseAudio `Volume` array property using the formula: `round(channelVolume / 65536 * 100)` where 65536 is `PA_VOLUME_NORM`
- If multiple channels exist, the mean of all channels is used
- `AudioService.volume` updates within 500 ms of a `VolumeUpdated` signal

#### REQ-F-005: Mute property exposure
The `AudioService` shall expose the default sink's mute state as a boolean.
**AC**:
- `AudioService.muted` is `true` when the sink's `Mute` property is `true`
- `AudioService.muted` is `false` when the sink's `Mute` property is `false`
- `AudioService.muted` updates within 500 ms of a `MuteUpdated` signal

#### REQ-F-006: Volume adjustment via Q_INVOKABLE
When QML calls `AudioService.setVolume(int percent)`, the system shall update the default sink's volume.
**AC**:
- `AudioService` exposes a public slot `void setVolume(int percent)`
- The slot clamps the input to 0–100
- The slot calls `org.PulseAudio.Core1.SetSinkVolumes(...)` with a per-channel uint32 array
- All channels in the array receive the same volume value: `percent / 100.0 * 65536`
- The new volume is audible (or confirmed in log) within 1 second of the call

#### REQ-F-007: Live volume and mute updates
The `AudioService` shall subscribe to D-Bus signals from the current sink and emit Qt property change signals.
**AC**:
- `AudioService` subscribes to `VolumeUpdated` signal on the sink object
- `AudioService` subscribes to `MuteUpdated` signal on the sink object
- When either signal fires, `AudioService` updates the corresponding property and emits `volumeChanged()` or `mutedChanged()`
- The volume/mute value in the widget updates within 500 ms of external changes (e.g., volume buttons, other applications)

#### REQ-F-008: AudioSection widget visibility
The `AudioSection.qml` widget shall render in the top bar and collapse when the audio service is unavailable.
**AC**:
- `AudioSection` root is a `BarSection` component
- `AudioSection` is located at `src/qml/Topbar/AudioSection.qml`
- When `AudioService.available == true`, `AudioSection.implicitWidth > 0`
- When `AudioService.available == false`, `AudioSection.implicitWidth == 0`
- The widget is registered in the `HolonightShell` QML module

#### REQ-F-009: Volume icon selection
The `AudioSection` widget shall display a context-appropriate icon for the current volume.
**AC**:
- Icon is an `Image` component sourced via Qt's icon provider (e.g., `"image://icon/audio-volume-low-symbolic"`)
- When `AudioService.muted == true`, icon name is `"audio-volume-muted-symbolic"`
- When `AudioService.volume >= 1 && <= 33`, icon name is `"audio-volume-low-symbolic"`
- When `AudioService.volume >= 34 && <= 66`, icon name is `"audio-volume-medium-symbolic"`
- When `AudioService.volume >= 67`, icon name is `"audio-volume-high-symbolic"`
- Icon updates within 200 ms of a volume or mute change

#### REQ-F-010: Volume percentage text display
The `AudioSection` widget shall display the current volume as a text label.
**AC**:
- Text component displays `AudioService.volume + "%"` (e.g., "67%") when `AudioService.muted == false`
- Text component displays an empty string when `AudioService.muted == true`
- Text color is `HoloniightPalette.onSurface`
- Font family is JetBrains Mono, size 13 px
- Text updates within 200 ms of a volume or mute change

#### REQ-F-011: Fixed-width widget layout
The `AudioSection` widget shall maintain constant width regardless of mute state to prevent layout jitter.
**AC**:
- `AudioSection.implicitWidth` is computed as: `icon.width + spacing + textMetrics.width`
- `textMetrics` measures the text `"100%"` in the same font as the volume label
- The computed width is set once on component creation and does not change when muted/unmuted
- The icon and text remain visually aligned in a single row

#### REQ-F-012: Scroll-wheel volume control
When a user scrolls on the `AudioSection` widget, the system shall adjust the volume by 5% steps.
**AC**:
- A `WheelHandler` is declared in `AudioSection` and receives scroll events
- Scrolling up (positive `angleDelta.y`) calls `AudioService.setVolume(min(100, AudioService.volume + 5))`
- Scrolling down (negative `angleDelta.y`) calls `AudioService.setVolume(max(0, AudioService.volume - 5))`
- Volume changes are applied immediately and smoothly clamped to 0–100 bounds
- Multiple rapid scroll events are processed without delay

#### REQ-F-013: Volume animation
The `AudioSection` widget shall animate the displayed volume number when it changes.
**AC**:
- A `Behavior on volume` (or equivalent state binding) applies a `NumberAnimation` to smooth volume changes
- Animation duration is 200 ms
- Easing curve is `Easing.OutCubic`
- If the widget is muted, the text fades or transitions smoothly (no abrupt disappear)

#### REQ-F-014: Theme color compliance
All colors used in `AudioSection.qml` shall be sourced from `HoloniightPalette`.
**AC**:
- Text color is `HoloniightPalette.onSurface`
- No hardcoded hex color values (`#rrggbb`) are present in `AudioSection.qml`
- Icon colors inherit from the system theme via the Qt icon provider

### Non-Functional

#### REQ-NF-001: Volume response time
The `AudioService` shall update the QML widget within 500 ms of any external volume/mute change.
**AC**:
- D-Bus signal delivery latency + property update + QML binding propagation totals ≤ 500 ms
- This is verified by triggering a volume change from another application (e.g., `pactl`) and measuring time to widget update

#### REQ-NF-002: Initialization time
The `AudioService` shall complete D-Bus setup and expose available volume/mute/available properties within 2 seconds of application startup.
**AC**:
- Measuring from `AudioService` constructor entry to first valid `volume` property read takes < 2 seconds
- If PulseAudio is unavailable, `available = false` is set within 2 seconds (not a timeout hang)

#### REQ-NF-003: Memory efficiency
The `AudioService` shall not leak memory when subscribing to D-Bus signals or switching sinks.
**AC**:
- Running the application for 5 minutes with 20 sink changes (via `pactl move-sink-input`) does not increase RSS memory by > 5 MB
- All `QDBusConnection` objects and signal subscriptions are properly disconnected when sinks change

#### REQ-NF-004: CPU efficiency
Scroll events and D-Bus signal handling shall not cause sustained CPU usage spikes > 5%.
**AC**:
- Rapidly scrolling (10 events/second) on the widget for 30 seconds maintains CPU < 5% (measured via `top` or profiler)
- Processing a `VolumeUpdated` signal does not spike CPU usage

#### REQ-NF-005: QML module registration
The `AudioService` class shall be properly registered in the `HolonightShell` QML module.
**AC**:
- `qmllint` reports no errors for `AudioSection.qml` imports of `AudioService`
- `AudioService` is accessible from QML without runtime warnings or registration errors

### Constraints

#### REQ-C-001: C++ naming conventions
`AudioService` class and all member variables shall follow the project's C++ naming rules.
**AC**:
- Class name is `AudioService` (CamelCase)
- Member variables are `lower_case_` (trailing underscore for private members)
- Public methods are `camelCase`
- All conform to `.clang-tidy` configuration in the repository

#### REQ-C-002: Qt6 and PulseAudio integration
`AudioService` shall use Qt6's `QDBusConnection` for D-Bus communication.
**AC**:
- The service uses `QDBusConnection::sessionBus()` to reach `org.PulseAudio.ServerLookup1`
- A second `QDBusConnection` is constructed from the `Address` property to reach `org.PulseAudio.Core1`
- No external PulseAudio C libraries are linked; only `QtDbus` module is used

#### REQ-C-003: QML module location and structure
`AudioSection.qml` shall be placed in the existing `src/qml/Topbar/` directory structure.
**AC**:
- File path is `src/qml/Topbar/AudioSection.qml`
- A corresponding `CMakeLists.txt` entry registers the file with `QT_RESOURCE_ALIAS` to strip the `src/qml/` prefix
- QRC path is `qrc:/HolonightShell/Topbar/AudioSection.qml`

#### REQ-C-004: No hardcoded audio parameters
Audio behavior (e.g., 5% step size, 200 ms animation duration) shall be configurable via properties, not hardcoded constants.
**AC**:
- Step size (currently 5%) can be changed by modifying a property, not by editing multiple files
- Animation duration is declared as a property and can be adjusted for tuning

#### REQ-C-005: Wayland and Qt6.11+ compatibility
The implementation shall not conflict with layer shell or other Wayland protocols.
**AC**:
- `AudioService` initialization does not interfere with surface role assignment or layer shell setup
- The widget renders correctly in a layer shell Wayland surface without rendering artifacts

#### REQ-C-006: No fallback to ALSA
The implementation shall not attempt to fall back to ALSA if PulseAudio is unavailable.
**AC**:
- If `org.PulseAudio.ServerLookup1` is not available, `AudioService.available` is `false`
- No ALSA D-Bus queries or direct PCM device enumeration are performed
- The widget cleanly collapses without errors

#### REQ-C-007: No per-application or input device control
The `topbar-audio` feature shall not expose per-application volume control, microphone input level, or input device selection.
**AC**:
- No per-app volume sliders are rendered
- No input device selection UI is implemented
- Only default sink output volume/mute is exposed

#### REQ-C-008: Build system integration
`AudioService` shall integrate with the project's CMake build system and `task` commands.
**AC**:
- `task configure` and `task build` build the feature without errors
- No new external dependencies are required (Qt6 `QtDbus` is already available)
- `task test` (if GTest suite exists) does not fail due to this feature

---

## Notes for Implementation

- Refer to the existing `BatteryService` implementation in the codebase for the Qt singleton pattern (`QML_ELEMENT`, `QML_SINGLETON`, `qmlRegisterSingletonType`).
- Consult `assets/dont-commit/status-indicators/*.svg` for visual icon geometry and placement guidance.
- The D-Bus `Address` property from `org.PulseAudio.ServerLookup1` may contain special characters (e.g., socket paths) that require careful URL/path encoding.
- Test with both PulseAudio and PipeWire (if available) to ensure compatibility with both D-Bus-compliant audio servers.

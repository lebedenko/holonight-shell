# SDD Tasks — topbar-audio

- [x] T-001: CMake: add AudioService and AudioSection to build
  - REQs: REQ-C-008, REQ-C-002
  - Check: `task configure && task build` completes without CMake or linker errors, and `task qml-lint` reports no registration issues for `AudioService` in `AudioSection.qml`.

- [x] T-002: AudioService header — class skeleton with Q_PROPERTY, QML_ELEMENT, QML_SINGLETON, and private members
  - REQs: REQ-F-001, REQ-C-001
  - Check: File `src/AudioService.h` exists with complete class declaration including `volume`, `muted`, `available` properties, `setVolume()` method, signal declarations, and all private member variables typed as per design.

- [x] T-003: AudioService constructor — two-step D-Bus lookup (session bus → dedicated socket)
  - REQs: REQ-F-002, REQ-NF-002
  - Check: `AudioService` constructor completes in <2 seconds, reads `org.PulseAudio.ServerLookup1` on session bus, connects to returned address via `QDBusConnection::connectToBus`, and sets `available` based on success/failure.

- [x] T-004: AudioService sink tracking — readFallbackSink, subscribeSink, unsubscribeSink, FallbackSinkUpdated handler
  - REQs: REQ-F-003, REQ-NF-003
  - Check: `onFallbackSinkUpdated` handler disconnects old sink signals, subscribes to new sink, and `current_sink_` updates within 500 ms of manually triggering a sink change (e.g., via `pactl`).

- [x] T-005: AudioService volume/mute property reads — readSinkProperties, VolumeUpdated/MuteUpdated handlers, PA_VOLUME_NORM conversion
  - REQs: REQ-F-004, REQ-F-005, REQ-F-007
  - Check: `readSinkProperties` reads `Volume` and `Mute` from sink object, converts uint32 arrays to 0–100 percent via `qRound(mean / 65536.0 * 100)`, emits change signals, and external volume changes propagate to QML within 500 ms.

- [x] T-006: AudioService setVolume — Q_INVOKABLE implementation with QDBusArgument marshalling and asyncCall
  - REQs: REQ-F-006, REQ-NF-004, REQ-C-004
  - Check: Calling `AudioService.setVolume(50)` clamps input to 0–100, constructs per-channel uint32 array with `percent / 100.0 * 65536`, and invokes `SetSinkVolumes` asynchronously; new volume is audible within 1 second.

- [x] T-007: Register AudioService singleton in main.cpp
  - REQs: REQ-F-001, REQ-NF-005
  - Check: `qmlRegisterSingletonType<AudioService>` registered in `src/main.cpp` factory lambda; `qmllint` reports no errors for `import HolonightShell; AudioService` in QML files.

- [x] T-008: AudioSection.qml — BarSection root with icon + text layout, TextMetrics fixed-width measurement, collapse when unavailable
  - REQs: REQ-F-008, REQ-F-009, REQ-F-010, REQ-F-011, REQ-F-014
  - Check: File `src/qml/Topbar/AudioSection.qml` exists as BarSection, `implicitWidth` = 0 when `AudioService.available == false`, icon displays correct symbol based on mute/volume tiers, text shows "N%" or empty string (when muted), `TextMetrics` measures "100%" for fixed width, all colors from `HoloniightPalette.onSurface`.

- [x] T-009: AudioSection.qml — WheelHandler scroll-to-adjust volume by 5% steps
  - REQs: REQ-F-012
  - Check: Scrolling up calls `setVolume(min(100, volume + 5))`, scrolling down calls `setVolume(max(0, volume - 5))`, volume changes are applied immediately without lag.

- [x] T-010: AudioSection.qml — NumberAnimation on displayVolume for smooth transitions
  - REQs: REQ-F-013
  - Check: `displayVolume` property animates toward `AudioService.volume` with 200 ms duration and `Easing.OutCubic`, text updates smoothly when muted/unmuted, no abrupt jumps.

- [x] T-011: Wire AudioSection into TopBar.qml between BatterySection and StatusSection
  - REQs: REQ-F-008
  - Check: `AudioSection` appears in `TopBar.qml` with `Layout.alignment: Qt.AlignVCenter`, widget renders in expected position without layout conflicts.

- [x] T-012: Build verification and lint clean
  - REQs: REQ-C-008, REQ-NF-005
  - Check: `task configure && task build` completes without errors, `task qml-lint` reports no issues, `task format-check` shows no formatting violations.

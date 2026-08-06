# Audio Popup Feature Specification

## Overview

The Audio Popup is a focused mini-audio control panel integrated into the holonight-shell top bar. It provides user-friendly access to PulseAudio device management and per-application volume control via a layer-shell popup surface (900×600, id "audio"). The popup is triggered by clicking the existing AudioWidget in the top bar and displays three tabs: Output Devices, Input Devices, and Applications. The backend uses libpulse (via PipeWire's pipewire-pulse compatibility layer) with a test-seam architecture to enable unit testing of device/stream models.

**Design reference**: `assets/dont-commit/01-audio-settings/design-idea.png`

---

## Functional Requirements

### Tab & Navigation

**REQ-F-001: Tab Sidebar Structure**  
The <audio popup> shall display a left-aligned tab sidebar with three mutually-exclusive tabs: "Output Devices", "Input Devices", and "Applications". Each tab switches the main panel content one-at-a-time.  
Acceptance: User clicks each tab in sequence; only the selected tab's content is visible in the main area; other tabs' content is hidden (not rendered or invisible).

**REQ-F-002: Default Tab on Open**  
When <the audio popup is first opened>, the <system> shall display the "Output Devices" tab selected by default.  
Acceptance: `task run` in Wayland; click the top-bar AudioWidget; the Output Devices tab is pre-selected and visible.

### Output Devices Section

**REQ-F-003: Output Device List**  
While <the Output Devices tab is active>, the <system> shall display all PulseAudio output sinks (audio devices capable of playback) in a scrollable list.  
Acceptance: At least 2 output sinks are available on the test system; both appear in the list without duplication.

**REQ-F-004: Output Device Row Layout**  
Each <output device row> shall contain, in left-to-right order: (1) a device icon (theme icon, e.g., speaker/headphone), (2) device name as the primary label, (3) device description as a subtitle, (4) a mute toggle button, (5) a volume slider, and (6) a volume percentage number (0–100). The row layout shall not overflow the 900-pixel width; long names shall elide with "…" rather than wrap or overflow.  
Acceptance: Render a device with a >30-character name; the name field clips to "…" without affecting slider or button width; all elements remain visible and clickable.

**REQ-F-005: Output Device Default Badge**  
If <an output device is the current default output sink>, then the <system> shall display a "DEFAULT" badge (styled with cyan accent color) on that device's row. No other row shall display a default badge.  
Acceptance: Set a sink as the default via `pactl set-default-sink <sink>`; reload the popup; only that sink's row has the badge.

**REQ-F-006: Output Device Mute Toggle**  
Each <output device row> shall include a mute toggle button that reflects the device's mute state. When <the user clicks the mute toggle>, the <system> shall invert the device's mute state and update the display within 100ms.  
Acceptance: Click the mute button; the button appearance toggles (e.g., icon changes); the device audio is muted/unmuted (verify via `pactl list sinks` or audible audio test).

**REQ-F-007: Output Device Volume Slider**  
Each <output device row> shall include an interactive volume slider representing the device's current volume (0–100%). When <the user drags the slider>, the <system> shall update the device volume continuously. When <the slider drag completes>, the <system> shall lock in the final volume value.  
Acceptance: Drag the slider to 50%; the device volume is set to 50% (verify via `pactl list sinks | grep "Volume"`); drag again to 75%; the value updates to 75%.

**REQ-F-008: Output Device Default Selection**  
When <the user clicks an output device row that is not the current default>, the <system> shall set that device as the default output sink. The "DEFAULT" badge shall move from the previous row to the newly-selected row within 100ms.  
Acceptance: Two sinks exist; click the non-default sink's row; `pactl get-default-sink` returns the newly-clicked sink's name; the badge visually moves on the popup.

**REQ-F-009: Output Device Volume Clamp**  
All <output device volume> values shall be clamped to the range [0%, 100%]. If <libpulse reports a volume outside this range>, then the <system> shall clamp it to 100%.  
Acceptance: Attempt to set a sink to 150% volume via `pactl set-sink-volume <sink> 150%`; the popup displays it as 100%; slider handle is at the maximum position.

### Input Devices Section

**REQ-F-010: Input Device List**  
While <the Input Devices tab is active>, the <system> shall display all PulseAudio input sources (audio devices capable of recording) in a scrollable list.  
Acceptance: At least 1 input source is available on the test system; it appears in the list without duplication.

**REQ-F-011: Input Device Row Layout**  
Each <input device row> shall follow the same layout as output device rows: icon, name, description, mute toggle, volume slider, and percentage. Long names shall elide with "…".  
Acceptance: Render an input device with a >30-character name; the name elides without breaking the layout.

**REQ-F-012: Input Device Default Badge**  
If <an input device is the current default input source>, then the <system> shall display a "DEFAULT" badge (styled with cyan accent color) on that device's row. No other input row shall display a default badge.  
Acceptance: Set a source as the default via `pactl set-default-source <source>`; reload the popup; only that source's row has the badge.

**REQ-F-013: Input Device Mute Toggle**  
Each <input device row> shall include a mute toggle button that reflects the device's mute state. When <the user clicks the mute toggle>, the <system> shall invert the device's mute state and update the display within 100ms.  
Acceptance: Click an input device's mute button; the button appearance toggles; verify mute state via `pactl list sources | grep Mute`.

**REQ-F-014: Input Device Volume Slider**  
Each <input device row> shall include an interactive volume slider representing the device's current volume (0–100%). When <the user drags the slider>, the <system> shall update the device volume continuously and lock it in when the drag completes.  
Acceptance: Drag an input source's slider to 60%; the source volume is set to 60% (verify via `pactl list sources | grep "Volume"`).

**REQ-F-015: Input Device Default Selection**  
When <the user clicks an input device row that is not the current default>, the <system> shall set that device as the default input source. The "DEFAULT" badge shall move from the previous row to the newly-selected row within 100ms.  
Acceptance: Two input sources exist; click the non-default source's row; `pactl get-default-source` returns the newly-clicked source's name; the badge visually moves on the popup.

**REQ-F-016: Input Device Accent Color**  
All <accent colors in the Input Devices section> (badges, highlights, sliders) shall use violet instead of cyan.  
Acceptance: Inspect the Input Devices tab in the running popup; default badge, slider thumb, and focus states are violet, not cyan.

### Applications Section

**REQ-F-017: Application Stream List**  
While <the Applications tab is active>, the <system> shall display all active PulseAudio sink-input streams (application playback streams) in a scrollable list.  
Acceptance: Play audio from at least 2 applications (e.g., Firefox tab + system notification sound); both streams appear in the list.

**REQ-F-018: Application Stream Row Layout**  
Each <application stream row> shall contain: (1) an application icon (from `application.icon_name` property, resolved via `image://icon`, with a generic audio icon fallback), (2) application name (from `application.name` property), (3) media title as a subtitle (from `media.name` property), (4) a mute toggle, (5) a volume slider, and (6) a percentage number. Long names shall elide.  
Acceptance: Play audio from an app with a >40-character name; the name elides without breaking layout; icon is visible (not a broken-image placeholder).

**REQ-F-019: Application Stream Mute Toggle**  
Each <application stream row> shall include a mute toggle that reflects the stream's mute state. When <the user clicks the mute toggle>, the <system> shall invert the stream's mute state and update the display within 100ms.  
Acceptance: Click an app stream's mute button; the icon toggles; audio from that app is muted/unmuted.

**REQ-F-020: Application Stream Volume Slider**  
Each <application stream row> shall include an interactive volume slider representing the stream's current volume (0–100%). When <the user drags the slider>, the <system> shall update the stream volume continuously and lock it in when the drag completes.  
Acceptance: Drag an app stream's slider to 40%; the stream volume is set to 40% (verify via `pactl list sink-inputs | grep "Volume"`).

**REQ-F-021: Application Stream Volume Clamp**  
All <application stream volume> values shall be clamped to the range [0%, 100%]. If <libpulse reports a volume outside this range>, then the <system> shall clamp it to 100%.  
Acceptance: Set an app stream to 120% via `pactl set-sink-input-volume <id> 120%`; the popup displays it as 100%.

### Master Volume Control

**REQ-F-022: Master Volume Bar**  
The <audio popup> shall display a "Master Volume" bar pinned to the bottom of the popup, spanning the full width, controlling the volume of the current default output sink.  
Acceptance: The Master Volume bar is visible at the bottom of all three tabs; it remains in place when scrolling device/stream lists.

**REQ-F-023: Master Volume Slider Interaction**  
When <the user drags the Master Volume slider>, the <system> shall update the default output sink's volume continuously. When <the slider drag completes>, the <system> shall lock in the final volume.  
Acceptance: Drag the Master Volume slider to 70%; the default sink's volume is set to 70% (verify via `pactl list sinks | grep "Volume"`).

**REQ-F-024: Master Volume Synchronization**  
The <Master Volume slider> shall always reflect the current default output sink's volume. If <the user changes a device row's slider for the current default sink>, then the <Master Volume slider> shall update to the same value within 50ms. If <the user changes the Master Volume slider>, then the <device row's slider for the current default sink> shall update to the same value within 50ms.  
Acceptance: Move the Master Volume slider; the matching device row's slider moves to the same position; move a device row's slider for the default sink; the Master Volume slider follows.

**REQ-F-025: Master Volume Reflects Default Change**  
When <the user changes the default output sink via a device row click>, the <Master Volume slider> shall reflect the new default sink's volume within 100ms.  
Acceptance: Two sinks exist with different volumes (e.g., 50% and 80%); Master Volume shows 50%; click the other sink to make it default; Master Volume updates to 80% within 100ms.

### Live Updates

**REQ-F-026: Device Add / Remove Updates**  
If <a new output/input device becomes available (hot-plugged)>, then the <corresponding tab's device list> shall reflect the new device within 200ms. If <an output/input device is removed>, then the <list> shall remove it within 200ms.  
Acceptance: Plug in a USB audio device while the popup is open; the device appears in the Output or Input list within 200ms (verify via `pactl list sinks/sources`).

**REQ-F-027: Volume Live Update**  
If <a device or stream volume is changed externally (e.g., via `pactl set-sink-volume` from a terminal)>, then the <corresponding row's slider and percentage> shall update within 100ms.  
Acceptance: While the popup is open, run `pactl set-sink-volume 0 50%` in a terminal; the output device's slider and percentage update within 100ms.

**REQ-F-028: Mute Live Update**  
If <a device or stream's mute state is changed externally (e.g., via `pactl set-sink-mute` from a terminal)>, then the <corresponding row's mute toggle appearance> shall update within 100ms.  
Acceptance: While the popup is open, run `pactl set-sink-mute 0 toggle` in a terminal; the device row's mute button appearance toggles within 100ms.

**REQ-F-029: Default Device Change External Update**  
If <the default output or input device is changed externally (e.g., via `pactl set-default-sink`)>, then the <"DEFAULT" badge shall move to the new default row> and the <Master Volume slider shall update to the new default's volume> within 100ms.  
Acceptance: While the popup is open, run `pactl set-default-sink <other-sink>` in a terminal; the DEFAULT badge moves to that sink's row within 100ms.

**REQ-F-030: Application Stream Add / Remove**  
If <a new application stream becomes active (app starts playing audio)>, then the <Applications list> shall include the new stream within 200ms. If <an application stream stops (app stops playing)>, then the <list> shall remove the stream within 200ms.  
Acceptance: While the popup is open, play audio from a new browser tab; the stream appears in the Applications list within 200ms (verify stream count).

**REQ-F-031: Slider Drag Stability**  
While <the user is dragging a volume slider>, the <system> shall not apply unrelated live updates that would interrupt or reset the slider position>.  
Acceptance: Drag a device slider to 50% over 1 second; while dragging, externally change another device's volume (different sink/source); the slider under the user's cursor maintains its dragged position throughout the drag without jumping.

### Edge Cases & Unavailability

**REQ-F-032: Unavailable Audio Server**  
If <libpulse is not available or the audio server is unreachable>, then the <audio popup> shall open and display an empty-state message (e.g., "Audio service unavailable") rather than crash or show a broken layout.  
Acceptance: Kill PulseAudio (or run in an environment without audio); click the top-bar AudioWidget to open the popup; it displays a user-friendly unavailable message and does not crash; restart PulseAudio and reload the popup (via re-click toggle); it recovers and shows devices.

**REQ-F-033: Empty Device Lists**  
If <a device list (output, input, or application) is empty>, then the <tab> shall display an empty-state message (e.g., "No output devices found") instead of a broken or blank layout.  
Acceptance: In a VM or minimal audio setup with no input sources, click the Input Devices tab; an "No input devices found" message is displayed, not a blank scrollable area.

**REQ-F-034: Name Elision**  
If <a device name or application name exceeds the available row width>, then the <system> shall elide the name with a trailing "…" and show the full name in a tooltip or truncate gracefully without overflow or text wrapping>.  
Acceptance: Set a custom device name with 50+ characters via PulseAudio config; the row displays the name truncated with "…"; no text overflows the row bounds.

---

## Non-Functional Requirements

**REQ-NF-001: Thread Safety**  
The <audio model> shall not mutate Qt properties or models from the PulseAudio callback thread. All model updates shall be marshalled to the Qt main thread via queued invocation (Qt::QueuedConnection or `QMetaObject::invokeMethod`).  
Acceptance: Run GTest suite with `task test` and verify that the fake audio backend and model-building logic pass unit tests for concurrent device/stream mutations; run `task build` and verify no thread-safety warnings from clang-tidy.

**REQ-NF-002: Test Seam Architecture**  
The <audio service> shall provide a test seam that allows model-building and marshalling logic to be exercised in GTest without a running PulseAudio server. This is satisfied by the existing `AudioService(SkipInitTag)` constructor, which skips libpulse backend construction so tests can drive `applyVolume`/`applyMuted`/`setAvailable` and the model `applyAdd`/`applyChange`/`applyRemove` methods directly. A separate abstract `IAudioBackend` interface is NOT required (amended after design review: `PulseAudioBackend` already exists as a concrete pimpl class and the `SkipInitTag` seam already enables all needed tests without virtual dispatch).  
Acceptance: Unit tests construct an `AudioService(SkipInitTag)`, feed device/stream fixtures via the public apply/model methods, and verify the models are populated and updated correctly without touching the real audio system.

**REQ-NF-003: Reuse of StatusPopupSurface**  
The <audio popup content> shall be loaded into the existing `StatusPopupSurface` singleton with id "audio" (900×600 pre-reserved). No new top-level layer-shell surface shall be created.  
Acceptance: Inspect CMakeLists.txt and C++ code; StatusPopupSurface uses id "audio"; no duplicate layer-shell surface creation in the audio implementation.

**REQ-NF-004: QML Module Structure**  
The <audio popup QML components> shall be part of the `HolonightShell` QML module. All new `.qml` files shall be placed under `src/qml/` in a directory hierarchy (e.g., `src/qml/AudioPopup/`) and added to `HOLONIGHT_QML_FILES` in CMakeLists.txt.  
Acceptance: All audio popup QML files are listed in CMakeLists.txt; `task qml-lint` reports no import errors for `HolonightShell` module imports.

**REQ-NF-005: QRC Path Convention**  
All <audio popup QML files> shall be accessible via QRC paths with the prefix `/HolonightShell/` (e.g., `qrc:/HolonightShell/AudioPopup/AudioPopupPanel.qml`). The QRC alias shall be derived from the file's location under `src/qml/` via CMake `QT_RESOURCE_ALIAS`.  
Acceptance: CMakeLists.txt correctly maps audio popup `.qml` files to `/HolonightShell/` prefixed aliases; `task build` succeeds without QRC errors.

**REQ-NF-006: Color Theming**  
All <colors in the audio popup> shall come from the HoloNight theme via `HoloniightPalette.<token>` imports. No hardcoded hex color values shall be used. Output accent: cyan, Input accent: violet, muted state: gray, default badge: cyan.  
Acceptance: Grep the audio popup QML files for hex color literals (e.g., `#FFF`); none shall be found. `task format-check` and `task tidy` pass.

**REQ-NF-007: Latency**  
Live updates (volume, mute, default changes) shall reflect in the UI within 100–200ms of the change in PulseAudio.  
Acceptance: Measure latency by making an external PulseAudio change and timing the UI update; most updates appear within 100ms, max 200ms (tolerance for system load).

**REQ-NF-008: Model Performance**  
The <device and stream list models> shall not re-render or re-create rows when unrelated properties change (e.g., changing one sink's volume shall not re-layout other sinks' rows). Qt's `QAbstractListModel` role-based updates shall be used to ensure granular change notifications.  
Acceptance: While the user drags one sink's slider, other sinks' rows do not flicker or re-layout; `dataChanged` signal is emitted only for affected model indices.

---

## Constraints

**REQ-C-001: PulseAudio Compatibility**  
The <audio backend> shall use libpulse (or PipeWire's pipewire-pulse compatibility layer) and shall not depend on PipeWire-specific APIs.  
Acceptance: The build links against libpulse; the code compiles and runs on both PulseAudio and PipeWire systems.

**REQ-C-002: Audio Widget Regression Prevention**  
The <existing top-bar AudioWidget> (default-sink volume / mute display) shall remain unchanged in behavior and appearance. The audio popup implementation shall not break or regress the widget's existing functionality.  
Acceptance: Build the project; run `task run`; verify that the top-bar AudioWidget displays the default sink's volume and mute state correctly; clicking the widget opens the audio popup without breaking the widget.

**REQ-C-003: Dismissal Behavior**  
The <audio popup> shall use the existing `StatusPopupSurface` dismissal framework (outside-click and re-click toggle). Esc key dismissal is best-effort and not a hard requirement (Hyprland layer-shell focus limitation).  
Acceptance: Click the top-bar AudioWidget; the popup opens; click outside the popup; it closes; click the widget again; the popup reopens (toggle behavior works).

**REQ-C-004: Build Verification**  
The <audio popup feature> shall pass all automated checks: `task build`, `task qml-lint`, `task format-check`, and `task tidy` without regressions in checked-in code.  
Acceptance: Run all four verification tasks; no new errors or warnings are reported in files modified by the audio popup implementation.

**REQ-C-005: Icon Resolution Fallback**  
If <an application icon name cannot be resolved via `image://icon`>, then the <system> shall display a generic audio icon (e.g., from the HoloNight icon set) as a fallback, not a broken-image placeholder.  
Acceptance: Launch an app with a missing or non-standard icon name; the stream row displays a fallback audio icon, not a broken-image indicator.

**REQ-C-006: Backend Initialization Order**  
The <audio backend> shall complete its initial connection and enumeration (devices/streams) before the audio popup attempts to populate its models. Initialization shall be synchronous or async-with-blocking in the popup's loader to ensure models are ready on display.  
Acceptance: Open the audio popup immediately after startup; device and stream lists are populated (not empty or "loading"); no "undefined" or null model entries.

---

## Out of Scope (Deferred)

The following features are **deferred** and documented in `docs/audio-panel-extras.md`:

- **Stream-to-Device Routing**: moving an application stream to a different output device via a per-row menu (⋮ button).
- **Device Port & Profile Selection**: selecting alternative ports or profiles on a device.
- **Presets / Profiles**: "Meeting Mode", "Headset Mode", or other audio configuration presets.
- **Over-Amplification**: boost volume above 100% (e.g., 150%).
- **Settings & Add-Device UI**: the "+" add-device buttons and Settings sidebar entry in the mockup.

---

## Acceptance Criteria Summary

| Category | Count | Verifiable By |
|----------|-------|---------------|
| Functional Requirements | 34 | Manual Wayland testing (`task run`), `pactl` CLI, GTest suite |
| Non-Functional Requirements | 8 | `task build`, `task qml-lint`, `task test`, `task format-check`, `task tidy` |
| Constraints | 6 | Integration testing, regression checks, build verification |
| **Total** | **48** | Cross-functional testing |

All requirements are independently verifiable and falsifiable. Each has an "Acceptance:" criterion that describes a testable condition, not a restatement of the requirement.

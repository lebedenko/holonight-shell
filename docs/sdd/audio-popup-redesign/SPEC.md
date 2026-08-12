# Audio Popup Redesign — Specification

**Status**: Complete (implemented and verified)
**Target Directory**: `apps/shell/qml/Popups/Audio/`
**Backend**: `libs/holonight-services/src/audio/`, `PulseAudioBackend`
**Date**: 2026-08-12

---

## Overview

The audio status popup is being redesigned from a tab-sidebar layout to a single-column scrollable interface with collapsible Output/Input device sections (accordion pattern), real device metadata (bus type, channel count, sample rate, codec), device-type icons, and a live microphone level meter on the Input summary row. This specification defines the new structure, data flow, visual elements, and behavioral patterns.

---

## 1. Page Structure & Layout

### REQ-F-1001: Single-Column Scrollable Layout
**Ubiquitous**: The audio popup shall render as a single vertical scrollable column (not a tab sidebar).

*Files likely touched*: `AudioPopupContent.qml`, removal of `AudioTabSidebar.qml`.

**Acceptance Criteria**:
- Popup opens and displays all sections in top-to-bottom order without any tab-selection UI.
- Column is scrollable when the total height exceeds the popup viewport height.
- No horizontal tab bar, no tab navigation, no StackLayout switching between Output/Input/Applications views.

### REQ-F-1002: Section Order
**Ubiquitous**: The popup shall arrange sections in this exact top-to-bottom order: Master Volume hero panel, Output Device section, Application Volume section, Input Device section, keyboard-hint footer.

*Files likely touched*: `AudioPopupContent.qml`.

**Acceptance Criteria**:
- Visual inspection confirms sections appear in the specified order.
- Master Volume hero is the first interactive element below the header.
- Keyboard footer is pinned or always-visible at the bottom.

### REQ-F-1003: Header Row with Settings Icon
**Ubiquitous**: The popup shall display a header row above all sections containing: a speaker icon + "AUDIO" label (uppercase, bold) on the left; a settings gear icon on the right.

*Files likely touched*: `AudioPopupHeader.qml` (new component or enhancement to existing header).

**Acceptance Criteria**:
- Header row renders at the top of the popup.
- "AUDIO" text is uppercase and bold.
- Speaker icon and "AUDIO" label are left-aligned.
- Settings gear icon appears on the right side of the header.
- Gear icon is enabled, keyboard-focusable, and has accessible name "Open Audio settings".

---

## 2. Master Volume Hero Panel

### REQ-F-2001: Master Volume Hero Panel Styling
**Ubiquitous**: The master volume section shall render as a hero panel with: a large circular icon (speaker glyph inside a glowing circular badge) on the left; to its right, a "MASTER VOLUME" label (small, uppercase, accent-colored) above a large percentage readout (e.g., "41%"); a horizontal slider spanning the width below the label/percentage; a separate small square mute button (speaker-muted icon) at the far right.

*Files likely touched*: `AudioMasterBar.qml` (restyle), or new `AudioMasterPanel.qml`.

**Acceptance Criteria**:
- Master volume panel renders below the header as the first interactive section.
- Large circular icon badge is visually distinct (e.g., has a glow effect per HoloNight design).
- "MASTER VOLUME" text is uppercase and uses an accent color from `HoloniightPalette`.
- Percentage text is large and clearly readable.
- Horizontal slider spans the available width and responds to drag/click.
- Mute button is a separate square icon to the far right, not integrated into the slider.
- Slider percentage and mute state both bind to existing `AudioService.volume` and `AudioService.muted`.

### REQ-F-2002: Master Volume Control Binding
**Ubiquitous**: The master volume slider shall bind to and update the system audio master volume through the existing `AudioService.setVolume(int)` invokable. The mute button shall bind to a NEW invokable that toggles mute on the current default output device, since no master-mute setter exists today (`AudioService.muted` is currently read-only, mirroring the default sink's mute state).

*Files likely touched*: `AudioMasterBar.qml`, `AudioService.h/cpp` (new invokable), `PulseAudioBackend` (delegates to existing `setDeviceMuted`).

**Acceptance Criteria**:
- Dragging the slider calls `AudioService.setVolume(newVolume)` and the system master volume updates audibly/visibly.
- A new `Q_INVOKABLE void AudioService::setDefaultOutputMuted(bool muted)` is added; it resolves the current `defaultOutputId()` and delegates to the same backend path as `setDeviceMuted` (i.e. `backend_->setDeviceMuted(defaultOutputId(), muted)`), so PulseAudio state and `AudioService::muted` stay authoritative and no duplicate mute-tracking state is introduced.
- Clicking the mute button calls `AudioService.setDefaultOutputMuted(!AudioService.muted)`, and the button icon toggles once `mutedChanged()` fires.
- The percentage readout updates in real-time as the slider moves or the service property changes.
- This is an additive invokable only — no existing `AudioService` signature changes (REQ-C-15007).

---

## 3. Device Metadata (Bus Type, Channels, Rate, Codec)

### REQ-F-3001: Device Metadata Capture from PulseAudio
**Ubiquitous**: The backend shall query and expose device metadata from PulseAudio: bus/connection type (Analog, Digital, Bluetooth), channel count, sample rate, and codec (for Bluetooth devices only).

*Files likely touched*: `PulseAudioBackend`, `AudioTypes.h`, `AudioDevice` struct.

**Acceptance Criteria**:
- `PulseAudioBackend::pa_sink_info_cb()` and `pa_source_info_cb()` callbacks now extract and store:
  - Bus type from the device's active port form-factor property (e.g., `PA_PROP_DEVICE_FORM_FACTOR`) or card bus name (e.g., "usb", "bluetooth", "analog").
  - Channel count from `pa_channel_map.channels` in the device info.
  - Sample rate from `pa_sample_spec.rate`.
  - Codec name (e.g., "AAC", "SBC") from Bluetooth-specific proplist keys (e.g., `PA_PROP_BLUETOOTH_CODEC`) — Bluetooth devices only.
- New fields added to `AudioDevice` struct: `QString busType`, `uint8_t channelCount`, `uint32_t sampleRate`, `QString codec`.
- Existing `AudioService` model role list updated to expose these new fields (or new roles added).

### REQ-F-3002: Device Metadata Fallback for Unavailable Data
**Ubiquitous**: When device metadata is unavailable from PulseAudio (null/missing proplist entries), the backend shall use sensible fallbacks or omit the data point.

*Files likely touched*: `PulseAudioBackend`.

**Acceptance Criteria**:
- If bus type is unknown, fallback to "Unknown" or an empty string (QML handles display).
- If channel count is 0, fallback to "Unknown" or omit the channel part of the subtitle.
- If sample rate is 0, fallback to a default (e.g., 48000) or omit.
- If codec is missing for a Bluetooth device, fallback to "Unknown" or "PCM".
- Backend logs a warning (via custom Logger) when a metadata query returns null but a device is otherwise valid.

### REQ-F-3003: Device Metadata Subtitle Format in QML
**Ubiquitous**: QML device rows shall render a metadata subtitle using the format: "BusType • ChannelCount channels • SampleRate kHz" for standard devices; "BusType • Codec • SampleRate kHz" for Bluetooth devices.

*Files likely touched*: `AudioDeviceDelegate.qml`, `AudioCurrentDeviceRow.qml` (new).

**Acceptance Criteria**:
- Metadata subtitle renders below the device name on all device rows.
- Subtitle uses exact formatting specified (bullets as separators, lowercase "channels", localized units in kHz).
- Bluetooth devices show codec in place of channel count (e.g., "Bluetooth • AAC • 48 kHz").
- Non-Bluetooth devices show channel count (e.g., "Analog • 2 channels • 48 kHz").
- Subtitle text color/size is consistent with the design (small, neutral/secondary color per `HoloniightPalette`).

---

## 4. Device-Type Icons

### REQ-F-4001: Device Icon Mapping from PulseAudio
**Ubiquitous**: The backend shall query the device icon name from PulseAudio and expose it on the `AudioDevice` struct for QML icon mapping.

*Files likely touched*: `PulseAudioBackend`, `AudioTypes.h`, `AudioDevice`.

**Acceptance Criteria**:
- `PulseAudioBackend` reads `device.icon_name` proplist property or derives icon from card form-factor (e.g., "audio-speakers", "audio-headphones", "audio-input-microphone").
- New `AudioDevice` field: `QString iconName`.
- Icon name is exposed to QML via `AudioDeviceModel` role list (or direct property access).

### REQ-F-4002: QML Device-Type Icon Rendering
**Ubiquitous**: QML device rows shall render a device-type icon (speaker, monitor, headphones, display, microphone) derived from the device's icon name, using a project icon set.

*Files likely touched*: `AudioDeviceDelegate.qml`, icon assets.

**Acceptance Criteria**:
- Device icon renders on the left of each device row (before the device name).
- Icon is mapped from PulseAudio's icon name to the project's icon set (e.g., map "audio-speakers" → `qrc:/HolonightShell/…/speaker.svg`).
- Unsupported/missing icon names fall back to a generic audio icon (no missing-icon placeholder).
- Icon color/size is consistent with the design (e.g., 24x24 px, tinted per `HoloniightPalette.<token>`).

---

## 5. Output Device Section (Accordion)

### REQ-F-5001: Output Device Current-Device Summary Row
**Ubiquitous**: The Output Device section shall display a current-device summary row (collapsible toggle) containing: device icon, device name (bold), metadata subtitle, a "DEFAULT" pill badge, and a chevron (pointing down when collapsed, up when expanded).

*Files likely touched*: `AudioCurrentDeviceRow.qml` (new), `AudioOutputDeviceSection.qml` (new or enhanced).

**Acceptance Criteria**:
- Summary row renders at the top of the Output section.
- Device icon, name, and metadata subtitle are left-aligned and vertically centered.
- "DEFAULT" pill badge appears to the right of the metadata subtitle.
- Chevron icon appears at the far right and visually rotates/changes direction on toggle.
- Summary row is clickable; clicking toggles the expanded device list visibility below it.
- When the Output section is collapsed (chevron down), no device list is visible below the summary.
- When the Output section is expanded (chevron up), the full device list renders immediately below the summary.

### REQ-F-5002: Output Device List (Full Device Rows)
**Ubiquitous**: When the Output Device section is expanded, the popup shall render a list of all output devices, each row showing: a radio-circle-with-checkmark selection indicator, device-type icon, device name, metadata subtitle, a per-row volume slider, and a percentage readout.

*Files likely touched*: `AudioDeviceDelegate.qml`, `AudioDeviceModel` (QML binding updates).

**Acceptance Criteria**:
- Device list rows render immediately below the summary row when section is expanded.
- Each row displays all specified elements (radio-circle, icon, name, metadata, slider, percentage).
- Radio-circle is filled/checked for the currently default output device; empty circle for others.
- Selected device row has a visually highlighted background or border (per design).
- Volume slider on each row is independent and calls `AudioService.setDeviceVolume(deviceId, volume)` on change.
- Percentage readout updates in real-time as slider moves or device volume property changes.
- Clicking the radio-circle or any non-slider part of the row calls the existing `AudioService.setDefaultOutputByName(name)` invokable.

### REQ-F-5003: Output Section "OUTPUT DEVICES" Plural Label
**Ubiquitous**: The Output Device section shall display an "OUTPUT DEVICES" label (uppercase, small, accent-colored) above the device list (when expanded).

*Files likely touched*: `AudioOutputDeviceSection.qml`.

**Acceptance Criteria**:
- "OUTPUT DEVICES" text renders above the device list with uppercase styling.
- Text uses an accent color from `HoloniightPalette`.
- Text size is small (consistent with section labels in the design).
- Label only appears when the device list is visible (section expanded).

---

## 6. Input Device Section (Accordion + VU Meter)

### REQ-F-6001: Input Device Current-Device Summary Row with VU Meter
**Ubiquitous**: The Input Device section shall display a current-device summary row containing an icon, name, metadata, live segmented VU meter, percentage, DEFAULT badge, and chevron.

*Files likely touched*: `AudioCurrentDeviceRow.qml`, `AudioInputSection.qml` (new or enhanced), `InputLevelMeter.qml` (new).

**Acceptance Criteria**:
- Summary row layout matches Output Device summary (device icon, name, metadata, DEFAULT badge, chevron).
- Chevron toggles the expanded input device list below (same accordion behavior as Output).
- Live VU meter renders to the right of the metadata/badge area, using a column of small vertical bars.
- Meter animates smoothly to reflect real-time microphone input level (see REQ-F-7001 for metering data source).
- Active meter bars use the section's cyan accent and inactive bars use the passive border token.
- Meter is NOT present on individual rows inside the expanded Input Devices list (see REQ-F-6003).

### REQ-F-6002: Input Device List (Full Device Rows)
**Ubiquitous**: When the Input Device section is expanded, the popup shall render a list of all input devices, each row showing: a radio-circle-with-checkmark selection indicator, device-type icon, device name, metadata subtitle, a per-row volume slider, and a percentage readout (no per-row VU meter).

*Files likely touched*: `AudioDeviceDelegate.qml` (mode variant for input rows), `AudioInputSection.qml`.

**Acceptance Criteria**:
- Device list rows render immediately below the summary row when section is expanded.
- Each row displays all specified elements except the VU meter (radio-circle, icon, name, metadata, slider, percentage).
- Radio-circle is filled/checked for the currently default input device; empty circle for others.
- Selected device row has a visually highlighted background or border.
- Volume slider on each row calls `AudioService.setInputDeviceVolume(deviceId, volume)` on change.
- Percentage readout updates in real-time as slider moves or device volume property changes.
- Clicking the radio-circle or any non-slider part of the row calls the existing `AudioService.setDefaultInputByName(name)` invokable.

### REQ-F-6003: Input Section "INPUT DEVICES" Plural Label
**Ubiquitous**: The Input Device section shall display an "INPUT DEVICES" label (uppercase, small, accent-colored) above the device list (when expanded).

*Files likely touched*: `AudioInputSection.qml`.

**Acceptance Criteria**:
- "INPUT DEVICES" text renders above the device list with uppercase styling and accent color.
- Text size is small (consistent with section labels).
- Label only appears when the device list is visible (section expanded).

---

## 7. Output/Input Accordion Behavior (Mutual Exclusivity)

### REQ-F-7001: Accordion Mutual Exclusivity
**State-driven**: While a section (Output or Input) is expanded, the opposite section shall be collapsed, maintaining exactly one expanded at all times.

*Files likely touched*: `AudioPopupContent.qml`, output/input section managers.

**Acceptance Criteria**:
- When the user clicks the Output summary chevron to expand Output, the Input section's device list collapses (Input summary remains visible, but list is hidden).
- When the user clicks the Input summary chevron to expand Input, the Output section's device list collapses.
- No state exists where both sections are expanded or both are collapsed (device lists always hidden/visible in pairs).
- Visual inspection of the popup confirms exactly one device list is visible at any moment.

### REQ-F-7002: Default Accordion State on Popup Open
**Event-driven**: When the audio popup opens, the Output Device section shall default to expanded (device list visible) and the Input Device section shall default to collapsed (device list hidden).

*Files likely touched*: `AudioPopupContent.qml`, section state initialization.

**Acceptance Criteria**:
- Popup opens and the Output section's device list is immediately visible; Input section shows only the summary row.
- Output summary chevron points up (expanded state icon).
- Input summary chevron points down (collapsed state icon).
- No user action is required to see the Output device list on first popup open.

---

## 8. Live Input Metering (VU/Peak Level Monitor)

### REQ-F-8001: Live Input Level Data from PulseAudio
**Ubiquitous**: The backend shall implement real-time microphone input level monitoring via a PulseAudio monitoring stream (peak or RMS level on the default input source).

*Files likely touched*: `PulseAudioBackend`, new `InputLevelMonitor` class or enhancement to `AudioService`.

**Acceptance Criteria**:
- A `pa_stream` is opened in monitor mode against the default input source (`pa_source_info.monitor_source_name` or equivalent).
- Callback reads peak/RMS level from the stream data (e.g., via `pa_stream_peek()` or peak sample per callback).
- Level is normalized to 0–100 scale (or 0–1, with QML converting to 0–100).
- Level updates are emitted at a fixed sample rate (e.g., 30 Hz) to QML via a signal/property (e.g., `AudioService::inputLevelChanged(int level)`).
- Stream is created lazily only when the Input section summary is visible or the popup is open; destroyed when the popup closes to avoid unnecessary CPU.
- Backend logs no spam (max 1 warning per session if stream creation fails); gracefully handles source unavailability (uses 0 level, no crash).

### REQ-F-8002: Live Meter Rendering and Animation
**Ubiquitous**: QML shall render the live input level as a row of small vertical bars, each bar animating smoothly from its current height to the new level value on each update.

*Files likely touched*: `InputLevelMeter.qml` (new).

**Acceptance Criteria**:
- Meter component receives level updates from `AudioService.inputLevelChanged` signal or property.
- Meter renders N vertical bars (e.g., 10 bars, with 10% per bar, or 12 bars for classic VU layout) arranged horizontally.
- Each bar's height is proportional to the current level (0 level = no height, 100 level = full height).
- Bars animate smoothly (e.g., 100–150 ms easing) on each level update, not snap.
- Active bars use `HoloniightPalette.accentCyan`; inactive bars use `HoloniightPalette.borderPassive`.
- Meter updates remain responsive (no UI freezing) when input level changes continuously.

### REQ-F-8003: Meter Behavior on Popup Open (Before First Sample)
**Event-driven**: When the Input section summary is first visible and no live level data has yet arrived, the meter shall render as empty (all bars at 0 height) and not display an error placeholder.

*Files likely touched*: `InputLevelMeter.qml`.

**Acceptance Criteria**:
- Meter component initializes with level = 0 (all bars invisible/flat).
- When the first level update arrives from the backend, bars animate upward smoothly.
- No "loading", "error", or "unavailable" text/icon appears in the meter area.
- Meter is visually indistinguishable from a silent/muted input on first open (zero level).

### REQ-F-8004: Meter Cleanup on Popup Close
**Event-driven**: When the audio popup closes, the backend shall stop the input level monitoring stream to free resources.

*Files likely touched*: `PulseAudioBackend`, `AudioService`, popup lifecycle.

**Acceptance Criteria**:
- Monitoring stream is closed when the popup is dismissed or hidden.
- No PA callbacks fire after popup close (no dangling stream references).
- CPU usage returns to baseline (no continuous PA polling after popup close).
- Reopening the popup recreates a fresh monitoring stream.

---

## 9. Application Volume Section

### REQ-F-9001: Application Volume Section with 4-Row Cap
**Ubiquitous**: The Application Volume section shall display up to 4 playback stream rows by default, each showing: a full-color application icon, application name, a per-row volume slider, a percentage readout, and a "..." (more options) icon button.

*Files likely touched*: `AudioApplicationsSection.qml`, `AudioStreamDelegate.qml`.

**Acceptance Criteria**:
- "APPLICATION VOLUME" label (uppercase, small, accent-colored) appears above the stream list.
- Up to 4 rows render by default (if fewer than 4 streams exist, all are shown).
- Each row displays app icon, name, slider, percentage, and "..." button.
- Rows beyond the first 4 are hidden when the section is in its collapsed state (see REQ-F-9002).
- Volume slider on each row calls `AudioService.setStreamVolume(streamId, volume)` on change.
- Percentage readout updates in real-time as slider moves or stream volume property changes.
- "..." button is rendered in a visually disabled/grayed-out state (see REQ-F-12001).

### REQ-F-9002: Application Volume "Show All" Toggle
**Ubiquitous**: The Application Volume section header shall include a "Show All" text link + chevron (pointing down when collapsed, up when expanded); clicking it shall toggle visibility of stream rows beyond the first 4.

*Files likely touched*: `AudioApplicationsSection.qml`.

**Acceptance Criteria**:
- "Show All" text link and chevron icon appear to the right of the "APPLICATION VOLUME" label.
- On first popup open, the section shows only the first 4 streams; "Show All" is visible and clickable.
- Clicking "Show All" expands the list to show all streams (5+, if any exist).
- Clicking "Show All" again collapses the list back to 4 rows.
- Chevron direction changes on toggle (points down = collapsed, up = expanded).
- "Show All" link text does not change (e.g., does not toggle to "Show Less"); only chevron direction changes visually.

### REQ-F-9003: Application Volume Per-Row Interaction
**Ubiquitous**: Clicking the volume slider or percentage area on an application row shall update the stream volume; clicking the "..." button shall do nothing (disabled placeholder).

*Files likely touched*: `AudioStreamDelegate.qml`.

**Acceptance Criteria**:
- Dragging the slider calls `AudioService.setStreamVolume(streamId, volume)`.
- Volume percentage updates visually as the slider moves.
- Clicking the "..." button does not open a menu, change focus, or emit any signal.
- "..." button remains a static, non-interactive visual element.

---

## 10. Keyboard Hint Footer

### REQ-F-10001: Keyboard Hint Footer Bar
**Ubiquitous**: The popup shall display a keyboard hint footer bar pinned at the bottom (or always visible at end of scrollable column), showing three hint chips: "Tab + ↔ Navigate", "Enter Select", "M Mute".

*Files likely touched*: `KeyboardHintFooter.qml` (new), `AudioPopupContent.qml`.

**Acceptance Criteria**:
- Footer bar renders below all sections (last visible element in the column).
- Three hint chips are rendered with consistent spacing/styling (e.g., small text, neutral background, rounded corners per design).
- Text is exactly as specified: "Tab + ↔ Navigate", "Enter Select", "M Mute" (no changes to wording).
- Footer is visually distinct from the section content (e.g., different background color, subtle separator line).
- Footer does not obstruct the Application Volume section when scrolling; popup scrolling does not hide the footer if popup height allows.

### REQ-F-10002: Keyboard Footer is Visual Only
**Ubiquitous**: The keyboard hint footer shall be a static visual element; no new keyboard event handlers or shortcuts shall be implemented as part of this redesign.

*Files likely touched*: (no new keyboard handlers added).

**Acceptance Criteria**:
- Footer renders but does not respond to any keyboard input.
- Pressing Tab, Enter, or M does not trigger any new popup behavior beyond existing Tab focus traversal.
- Existing keyboard focus (Tab) navigation between controls continues to work as before (unmodified).
- Code review confirms no new `onPressed`, `Keys.onPressed`, or global shortcut handlers are added for this footer.

---

## 11. Radio-Checkmark Selection Indicator

### REQ-F-11001: Radio-Circle Selection Indicator in Device Rows
**Ubiquitous**: Each device row in the expanded Output and Input device lists shall render a radio-circle-with-checkmark selection indicator on the far left, filled/checked for the currently default device, empty circle for others.

*Files likely touched*: `AudioDeviceDelegate.qml`.

**Acceptance Criteria**:
- Radio-circle icon appears at the far left of each device row (before the device-type icon).
- Currently-selected device (marked `is_default: true` in `AudioDevice`) renders a filled circle with a checkmark icon inside.
- Non-selected devices render an empty circle (outline only, no checkmark).
- Radio-circle is clickable and calls the existing `AudioService.setDefaultOutputByName(name)` invokable (or `setDefaultInputByName(name)` for input rows).
- Visual state updates immediately when the selection changes (no delay).

---

## 12. Disabled/Unwired UI Elements

### REQ-F-12001: Settings Gear Navigation
**Event-driven**: Activating the header's settings gear shall asynchronously request the `audio` page from `holonight-settings` and close the popup after dispatch.

*Files likely touched*: `AudioPopupHeader.qml`.

**Acceptance Criteria**:
- Gear uses the shared `HnIconButton` interaction and focus visuals.
- Gear exposes accessible name "Open Audio settings".
- Activation calls `SettingsNavigationService.openPage("audio")`, then `StatusPopupSurface.hide()`.
- D-Bus failures are warning-only and never crash or block the shell.

### REQ-F-12002: Per-App "..." Menu Button (Disabled Placeholder)
**Ubiquitous**: Each application row's "..." (more options) button shall be rendered in a visually disabled state and shall not respond to any click or interaction.

*Files likely touched*: `AudioStreamDelegate.qml`.

**Acceptance Criteria**:
- "..." button is visible on the right side of each stream row but appears visually disabled (opacity < 100%, grayed).
- Clicking the "..." button does not open a menu, emit a signal, or change focus.
- No `onClicked` handler is attached to the button.
- Button remains a static visual element; no future wiring is implied.

---

## 13. Data Flow and Backend Updates

### REQ-F-13001: AudioTypes Struct Updates
**Ubiquitous**: The `AudioDevice` struct shall be extended with new fields to expose device metadata and icon information.

*Files likely touched*: `libs/holonight-services/src/audio/AudioTypes.h`.

**Acceptance Criteria**:
- `AudioDevice` struct now includes:
  - `QString busType` (e.g., "Analog", "Digital", "Bluetooth", "Unknown").
  - `uint8_t channelCount` (e.g., 2 for stereo; 0 if unavailable).
  - `uint32_t sampleRate` (e.g., 48000; 0 if unavailable).
  - `QString codec` (e.g., "AAC", "SBC"; empty for non-Bluetooth devices).
  - `QString iconName` (e.g., "audio-speakers", "audio-headphones").
- All new fields have sensible default initializers (empty string, 0 for numbers).
- Existing fields (`id`, `name`, `description`, `volume`, `muted`, `is_default`, `type`) remain unchanged.

### REQ-F-13002: AudioDeviceModel Role Updates
**Ubiquitous**: The `AudioDeviceModel` (QAbstractListModel) shall expose the new device metadata fields as roles or properties for QML binding.

*Files likely touched*: `libs/holonight-services/src/audio/AudioDeviceModel.h/cpp`, `AudioService` (if roles are exposed via service).

**Acceptance Criteria**:
- New roles added to the model: `BusType`, `ChannelCount`, `SampleRate`, `Codec`, `IconName` (or equivalent).
- QML can bind to these roles in a Repeater/ListView (e.g., `model.busType`).
- Existing roles (`DeviceId`, `Name`, `Description`, `Volume`, `Muted`, `IsDefault`) remain unmodified.
- Model updates trigger role updates to QML when device properties change (via `dataChanged` signal).

### REQ-F-13003: PulseAudioBackend Metadata Extraction
**Ubiquitous**: The `PulseAudioBackend` shall extract and populate the new `AudioDevice` metadata fields from PulseAudio device info callbacks.

*Files likely touched*: `libs/holonight-services/src/audio/PulseAudioBackend.cpp`.

**Acceptance Criteria**:
- `pa_sink_info_cb()` and `pa_source_info_cb()` callbacks now read:
  - Bus type from `pa_proplist_gets(info->proplist, PA_PROP_DEVICE_FORM_FACTOR)` or card bus name.
  - Channel count from `info->channel_map.channels`.
  - Sample rate from `info->sample_spec.rate`.
  - Icon name from `pa_proplist_gets(info->proplist, PA_PROP_DEVICE_ICON_NAME)`.
  - Codec from `pa_proplist_gets(info->proplist, "bluez.codec_name")` (Bluetooth only) or similar.
- Extracted data is stored on the `AudioDevice` struct before adding to the model.
- Fallback values are applied gracefully (no crashes on missing data; see REQ-F-3002).

### REQ-F-13004: Input Level Monitoring Backend
**Ubiquitous**: The `PulseAudioBackend` (or new `InputLevelMonitor` class) shall implement real-time level monitoring of the default input source.

*Files likely touched*: `libs/holonight-services/src/audio/PulseAudioBackend.cpp`, new files if applicable.

**Acceptance Criteria**:
- A monitoring stream is created when input level monitoring is requested (lazy initialization).
- Stream connects to the default input source's monitor source.
- Peak or RMS level is extracted per callback and normalized to 0–100.
- Level updates are emitted via `AudioService::inputLevelChanged(int)` signal.
- Stream is destroyed when monitoring is stopped (on popup close).
- No memory leaks or dangling callbacks after stream destruction.

### REQ-F-13005: Master Mute Toggle Invokable
**Ubiquitous**: `AudioService` shall expose a new `Q_INVOKABLE void setDefaultOutputMuted(bool muted)` that toggles mute on the current default output device, since no such invokable exists today (`AudioService.muted` is currently a read-only mirror of the default sink's PulseAudio mute state, with no setter — the topbar `AudioWidget` only displays it, never toggles it).

*Files likely touched*: `libs/holonight-services/src/audio/AudioService.h/cpp`.

**Acceptance Criteria**:
- `AudioService.h` declares `Q_INVOKABLE void setDefaultOutputMuted(bool muted)`.
- Implementation resolves `defaultOutputId()` and delegates to the same backend path already used by `setDeviceMuted` (`backend_->setDeviceMuted(defaultOutputId(), muted)`), so `AudioService::muted` continues to update via the existing PulseAudio echo path (`applyMuted`) rather than being set directly.
- No-ops safely (no crash) when `defaultOutputId()` is invalid (e.g. `AudioService(SkipInit)` in tests, matching the existing no-op pattern for `setDefaultOutputByName`).
- This is purely additive — existing `AudioService` invokables/signals are unchanged (REQ-C-15007).

---

## 14. Visual Design Consistency

### REQ-NF-14001: HoloNight Palette and Theming
**Ubiquitous**: All colors, gradients, and glows shall derive exclusively from the `HoloniightPalette` (note: double-i spelling) and must never use hardcoded hex values in QML or C++.

*Files likely touched*: All new QML components.

**Acceptance Criteria**:
- No QML files contain hardcoded colors like `"#ff0000"`, `"#ffffff"`, etc.
- All colors reference `HoloniightPalette.<token>` (e.g., `HoloniightPalette.accentColor`, `HoloniightPalette.surfaceBase`).
- Glow effects use palette accent or highlight colors.
- Text elements use palette text-color tokens.
- Background elements use palette surface/base tokens.
- Design review confirms visual consistency with existing shell UI (topbar, sidebar, etc.).

### REQ-NF-14002: Icon Sizing and Alignment
**Ubiquitous**: All icons shall maintain consistent sizing and alignment across the popup layout.

*Files likely touched*: All QML components with icons.

**Acceptance Criteria**:
- Device-type icons: 24x24 px (or design-specified size), left-aligned in rows.
- Application icons: 32x32 px or 24x24 px (consistent size per row), left-aligned.
- Slider icons (mute, level meter bars): consistent with design spacing.
- Icons are vertically centered within their row.
- No icon scaling artifacts (pixelation or blurriness).

### REQ-NF-14003: Layout Spacing and Padding
**Ubiquitous**: All sections, rows, and interactive elements shall use consistent spacing and padding per the design mockup.

*Files likely touched*: All new QML layouts.

**Acceptance Criteria**:
- Section vertical spacing is uniform (e.g., 16 px between Master Volume and Output section).
- Row padding is uniform (e.g., 12 px horizontal, 8 px vertical per row).
- Label/text vertical alignment is consistent (e.g., baseline or center per row type).
- No overlapping elements, no cramped layouts, no excessive whitespace.
- Popup width/height constraints allow readable text and interactive slider/button sizes.

---

## 15. Constraints and Non-Goals

### REQ-C-15001: No Bluetooth Device Pairing
**Constraint**: This redesign does NOT include any Bluetooth device discovery, pairing, or management features. Any "Pair New Device" UI elements in mockups or design references are explicitly out of scope.

*Rationale*: Bluetooth pairing is a separate workflow and will be owned by a dedicated Bluetooth pairing popup in a future cycle.

**Acceptance Criteria**:
- No Bluetooth pairing UI, buttons, or workflows are implemented in this redesign.
- The popup does NOT display a "Pair New Device" button or link.
- Existing Bluetooth devices (already paired in the system) appear in the device list; unpaired devices do NOT appear.
- No new backend D-Bus services, methods, or signals are added for pairing/discovery.

### REQ-C-15002: Settings Page Activation Contract
**Constraint**: Settings navigation shall use the existing `org.freedesktop.Application` D-Bus contract without a process-launch fallback or private protocol.

*Rationale*: `holonight-settings` owns application activation and page-key validation.

**Acceptance Criteria**:
- The asynchronous call targets service `org.holonight.Settings`, object `/org/holonight/Settings`, interface `org.freedesktop.Application`, method `ActivateAction`.
- Arguments are the `audio` action name, an empty variant list, and an empty platform-data map.
- No `QProcess`, URI activation, or direct QML D-Bus binding is used.

### REQ-C-15003: No Per-App Menu Wiring
**Constraint**: The "..." (more options) button on each application stream row is a visual-only placeholder in this cycle. No context menu or action is triggered by clicking the button.

*Rationale*: Per-app stream management (e.g., move to device, routing options) is deferred to a future cycle.

**Acceptance Criteria**:
- Code review confirms no click handler is attached to the "..." button.
- Button remains visually disabled (no hover feedback, no cursor shape change).
- No TODO or FIXME comment in code implies future wiring is pending.

### REQ-C-15004: No New Keyboard Shortcuts
**Constraint**: This redesign does NOT implement new keyboard shortcuts (e.g., M for mute, Enter for device selection, etc.). The keyboard hint footer is visual only.

*Rationale*: Keyboard navigation and shortcuts are complex interaction design topics; they are deferred to a future cycle. Existing Tab-based focus traversal continues to work.

**Acceptance Criteria**:
- No new `Keys.onPressed`, `onKeyPressed`, or global shortcut handlers are added.
- No new QML animation, event, or signal emission responds to M, Enter, or other hint footer keys.
- Existing Tab focus traversal between sliders, buttons, and sections works as before (no regression).
- Code review confirms zero new keyboard handling code is introduced.

### REQ-C-15005: No Per-Row Input Device VU Meter
**Constraint**: The live VU/level meter is displayed ONLY on the Input Device section's current-device summary row. Individual rows inside the expanded "Input Devices" list DO NOT show a per-row meter; they use the standard slider + percentage pattern (same as Output Device rows).

*Rationale*: Per-row metering is expensive (requires monitoring multiple source streams) and not part of the design.

**Acceptance Criteria**:
- Input summary row displays a live VU meter on the right side (instead of a percentage readout).
- Rows inside the expanded "Input Devices" list display a slider + percentage readout (no meter).
- No per-row monitoring stream is created for device rows inside the list.
- Visual inspection confirms meter appears ONLY on the summary row, not on the list rows.

### REQ-C-15006: No Input/Output Device List Pagination
**Constraint**: Output and Input device lists always show the complete list of devices when their section is expanded. There is no "Show More" pagination, no per-row capping, no scrolling within a sublist. (The Application Volume section has a 4-row cap and "Show All" toggle; device lists do not.)

*Rationale*: Device counts are typically small (< 10); pagination complexity is unnecessary.

**Acceptance Criteria**:
- When the Output section is expanded, all output devices are visible as rows (no scrolling within the device list, no "Show More" button).
- When the Input section is expanded, all input devices are visible as rows.
- If devices exceed the popup viewport height, the entire popup column scrolls (not the device list subgroup alone).
- No "Show More Devices" or "..." device list controls are present.

### REQ-C-15007: No Backward-Incompatible Changes to AudioService Public API
**Constraint**: Existing `AudioService` invokables and signals used by other parts of the shell (topbar, widgets, notifications) must remain unchanged. New invokables or properties may be added; existing ones must not be removed or have their signatures altered.

*Rationale*: Compatibility with existing consumers across the shell.

**Acceptance Criteria**:
- Existing invokables (`setVolume`, `setMuted`, `setDefaultOutput`, `setDeviceVolume`, etc.) retain their signatures and behavior.
- Existing signals/properties (`volumeChanged`, `defaultOutputChanged`, `outputsModel`, `inputsModel`, etc.) are not removed or renamed.
- Code review compares old and new `AudioService` headers; no breaking changes are flagged.
- Other parts of the shell (topbar, OSD, notifications) continue to work with the updated `AudioService` without modification.

---

## Acceptance Test Plan (High-Level)

### Manual Testing Checklist
1. **Popup Opens**: Popup opens with Master Volume at top, Output section expanded, Input section collapsed.
2. **Accordion Toggle**: Clicking Output chevron collapses Output and expands Input (and vice versa). Exactly one device list is visible at any time.
3. **Device Metadata**: Each device row shows metadata subtitle with bus type, channels/codec, and sample rate (e.g., "Analog • 2 channels • 48 kHz").
4. **Device Icons**: Each device row has a distinct icon (speaker, headphones, monitor, etc.) matching PulseAudio device type.
5. **Radio-Checkmark Indicator**: Default device row shows a filled radio-circle with checkmark; others show empty circles. Clicking a radio-circle or row changes the default device.
6. **Master Volume Control**: Dragging the master slider updates system volume; clicking the mute button toggles mute state. Icon glow and percentage update in real-time.
7. **Input VU Meter**: When Input section is expanded, the summary row shows a live VU meter (bars animating with microphone input level). No meter appears on individual device rows.
8. **Application Volume 4-Row Cap**: Popup shows up to 4 app streams by default. Clicking "Show All" expands to show all streams (5+, if any).
9. **Settings and deferred stream actions**: The enabled settings gear opens the `audio` settings page and closes the popup. The trailing "..." stream action remains a disabled placeholder.
10. **Keyboard Footer**: Footer bar at the bottom displays three hint groups ("Tab + ← → Navigate", "Enter Select", "M Mute"). Popup-scoped `M` mute and existing focus/selection behavior make the advertised hints truthful.
11. **Theming**: All colors come from `HoloniightPalette`; no hardcoded hex values are visible in QML source.
12. **Scroll Behavior**: Popup content scrolls smoothly; footer remains visible or scrolls with content (per design).

### Automated Testing
- Unit tests for new `AudioDevice` struct fields (serialization, defaults).
- Unit tests for metadata extraction logic in `PulseAudioBackend` (fallback behavior, edge cases).
- QML smoke tests for new components (InputLevelMeter, AudioCurrentDeviceRow) render without errors.
- Integration test: `AudioService` model roles include new metadata fields; QML can bind and display them.

---

## Summary of Files Likely to Change

**QML Components** (new or significant edits):
- `apps/shell/qml/Popups/Audio/AudioPopupContent.qml` — Main layout redesign (single column, sections).
- `apps/shell/qml/Popups/Audio/AudioPopupHeader.qml` — Add enabled, accessible settings navigation.
- `apps/shell/qml/Popups/Audio/AudioMasterBar.qml` or `AudioMasterPanel.qml` — Restyle master volume hero panel.
- `apps/shell/qml/Popups/Audio/AudioCurrentDeviceRow.qml` (new) — Current-device summary row with metadata and chevron.
- `apps/shell/qml/Popups/Audio/AudioOutputDeviceSection.qml` (new) — Output section with accordion.
- `apps/shell/qml/Popups/Audio/AudioInputSection.qml` (new) — Input section with accordion + VU meter on summary.
- `apps/shell/qml/Popups/Audio/InputLevelMeter.qml` (new) — Live VU/level meter visualization.
- `apps/shell/qml/Popups/Audio/AudioDeviceDelegate.qml` — Add radio-checkmark indicator, metadata subtitle, device icon; remove old "mute button on row".
- `apps/shell/qml/Popups/Audio/AudioApplicationsSection.qml` (new or enhanced) — 4-row cap + "Show All" toggle.
- `apps/shell/qml/Popups/Audio/AudioStreamDelegate.qml` — Add "..." button (disabled); remove old mute button if it existed on stream rows.
- `apps/shell/qml/Popups/Audio/KeyboardHintFooter.qml` (new) — Keyboard hint footer bar.
- (Remove `apps/shell/qml/Popups/Audio/AudioTabSidebar.qml` — tab sidebar is replaced by accordion sections).

**C++ / Backend**:
- `libs/holonight-services/src/audio/AudioTypes.h` — Add `busType`, `channelCount`, `sampleRate`, `codec`, `iconName` to `AudioDevice` struct.
- `libs/holonight-services/src/audio/AudioDeviceModel.h/cpp` — Add roles for new metadata fields.
- `libs/holonight-services/src/audio/PulseAudioBackend.cpp` — Extract and populate new `AudioDevice` fields from PA callbacks; implement input level monitoring stream.
- `libs/holonight-services/src/audio/AudioService.h/cpp` — Emit `inputLevelChanged(int)` signal; add `setDefaultOutputMuted(bool)` invokable (REQ-F-13005); no breaking changes to existing invokables.

**Assets** (if not already available):
- Icon mappings for device types (speaker, headphones, monitor, display, microphone) in the project icon set.

---

## Open Questions / Future Clarifications

None at this stage. All decisions have been made during requirements grilling and are reflected above.

---

## Approval / Sign-Off

**Specification Written**: 2026-08-12
**SDD Stage**: Final
**Completion**: Implementation and automated verification complete. Bluetooth pairing and Audio settings-page content remain separate follow-up work.

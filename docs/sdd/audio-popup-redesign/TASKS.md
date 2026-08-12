# SDD Tasks — audio-popup-redesign

## C++ Backend: Type & Model Foundation

- [x] T-001: Add new `AudioDevice` struct fields
  - REQs: REQ-F-13001
  - Check: `AudioTypes.h` defines `AudioDevice` with five new fields (`busType`, `channelCount`, `sampleRate`, `codec`, `iconName`); all have sensible default initializers; all existing fields remain unchanged.

- [x] T-002: Extend `AudioDeviceModel` with new roles and `defaultDevice` property
  - REQs: REQ-F-13002
  - Check: `AudioDeviceModel` role enum appends `BusType`, `ChannelCount`, `SampleRate`, `Codec`, `IconName` after existing roles; `roleNames()` includes camelCase keys; new `Q_PROPERTY defaultDevice` and `Q_SIGNAL defaultDeviceChanged()` are declared; `refreshDefaultDevice()` helper scans for `is_default` and emits on change.

- [x] T-003: Implement `classifyBusType()` and metadata extraction in `PulseAudioBackend`
  - REQs: REQ-F-13003, REQ-F-3001, REQ-F-3002, REQ-F-4001
  - Check: New static `classifyBusType(const pa_proplist*)` function covers hdmi/spdif, bluetooth, usb, pci detection with "Unknown" fallback; `sinkToDevice()` and `sourceToDevice()` populate all five new fields from PA callbacks; icon fallback chain uses `PA_PROP_DEVICE_ICON_NAME` then form-factor-derived fallback; metadata-warned-ids tracking logs once per device.

- [x] T-004: Add `setDefaultOutputMuted` invokable to `AudioService`
  - REQs: REQ-F-13005, REQ-F-2002, REQ-C-15007
  - Check: `AudioService::setDefaultOutputMuted(bool)` is declared and forwards to `backend_->setDeviceMuted(defaultOutputId(), muted)` with null-backend guard; signature matches REQ-F-13005's literal wording; existing `AudioService` signatures unchanged.

- [x] T-005: Extend `PulseAudioSystem` with `pa_stream_*` virtual methods
  - REQs: REQ-F-13004, REQ-F-8001
  - Check: `PulseAudioSystem` declares ~9 new virtual `pa_stream_*` methods (new, set_state_callback, set_read_callback, connect_record, peek, drop, disconnect, unref, get_state); `RealPulseAudioSystem` forwards each to `::pa_stream_*`; `FakePulseAudioSystem` has matching stubs.

- [x] T-006: Implement input-level monitoring stream lifecycle in `PulseAudioBackend`
  - REQs: REQ-F-13004, REQ-F-8001, REQ-F-8004
  - Check: `startInputLevelMonitor()` and `stopInputLevelMonitor()` methods are idempotent; `startInputLevelMonitor()` opens a PA_STREAM_PEAK_DETECT record stream at 30 Hz against `default_source_name` with read callback computing `level = clamp(|sample| * 100, 0, 100)` and emitting `inputLevelChanged(int)` on Qt thread; state callback emits `inputLevelChanged(0)` on PA_STREAM_FAILED with ≤1 warning; `stopInputLevelMonitor()` tears down callbacks before disconnect; default-source change while active reconnects stream to new name.

- [x] T-007: Surface input-level monitoring in `AudioService`
  - REQs: REQ-F-13004, REQ-F-8001
  - Check: `AudioService` declares `Q_PROPERTY int inputLevel`, `Q_INVOKABLE void startInputLevelMonitoring()`, `Q_INVOKABLE void stopInputLevelMonitoring()`, `Q_SIGNAL void inputLevelChanged(int)` with matching null-backend guards and property-change-only re-emission; signal signature matches REQ-F-8001.

## GTest Coverage — C++ Backend

- [x] T-008: Write unit tests for new `AudioDeviceModel` roles and `defaultDevice` property
  - REQs: REQ-F-13002
  - Check: `test_audio_device_model.cpp` verifies new roles in `roleNames()`; `data()` returns the five fields correctly; `defaultDevice` starts empty, becomes populated when an `is_default` device is added, re-emits on that device's metadata/volume change, goes back to empty when removed.

- [x] T-009: Write unit tests for `classifyBusType()` and metadata extraction
  - REQs: REQ-F-13003, REQ-F-3001, REQ-F-3002, REQ-F-4001
  - Check: `test_pulse_audio_backend.cpp` unit-tests `classifyBusType()` as a free function against hand-built proplist fixtures for each branch (hdmi, bluetooth, usb, pci, empty); `sinkToDevice()`/`sourceToDevice()` tests verify `channelCount`/`sampleRate` population and codec fallback chain (bluez.codec_name → bluetooth.codec → "PCM" for Bluetooth, empty for others).

- [x] T-010: Extend `FakePulseAudioSystem` and test level-monitor lifecycle
  - REQs: REQ-F-13004, REQ-F-8001, REQ-F-8004
  - Check: `FakePulseAudioSystem` implements `pa_stream_*` methods with call-order recording; tests verify `startInputLevelMonitor()` calls stream_new/connect_record with correct sample spec/flags; fake read callback emits `inputLevelChanged` with correct percent scaling; `PA_STREAM_FAILED` state emits `inputLevelChanged(0)` and logs ≤1 warning; `stopInputLevelMonitor()` clears callbacks before disconnect/unref (order asserted via call log); default_source_name change while monitoring reconnects stream.

- [x] T-011: Write unit tests for `AudioService::setDefaultOutputMuted` and level monitoring
  - REQs: REQ-F-13005, REQ-F-13004, REQ-C-15007
  - Check: `test_audio_service.cpp` verifies `setDefaultOutputMuted` no-ops safely under `AudioService(SkipInit)` and delegates to `setDeviceMuted(defaultOutputId(), muted)` with a mock backend; `startInputLevelMonitoring`/`stopInputLevelMonitoring` no-op safely under `SkipInit`; `inputLevelChanged` forwards from backend to service.

## QML: Shared Helpers & Leaf Components

- [x] T-012: Create `AudioMetadataFormat.js` helper function
  - REQs: REQ-F-3003, REQ-NF-14001
  - Check: `AudioMetadataFormat.js` exports `formatDeviceMetadata(busType, channelCount, sampleRate, codec)` returning localized kHz metadata where middle is channel count for non-Bluetooth or codec for Bluetooth and zero-value parts are omitted.

- [x] T-013: Create `AudioPopupHeader.qml` with speaker icon and settings navigation
  - REQs: REQ-F-1003, REQ-F-12001, REQ-NF-14001
  - Check: `AudioPopupHeader.qml` renders a speaker icon and semantic label plus an enabled, focusable `HnIconButton` that opens page key `audio` and closes the popup.

- [x] T-014: Create `AudioMasterPanel.qml` hero panel with icon badge, slider, and mute button
  - REQs: REQ-F-2001, REQ-F-2002, REQ-NF-14001, REQ-NF-14002, REQ-NF-14003
  - Check: `AudioMasterPanel.qml` renders large circular glow badge on left; "MASTER VOLUME" uppercase accent-colored label + large percentage readout; horizontal slider spanning width bound to `AudioService.setVolume`; small square mute button on far right calling `AudioService.setDefaultOutputMuted(!AudioService.muted)` with immediate icon/mute-state visual feedback.

- [x] T-015: Create `InputLevelMeter.qml` animated VU meter component
  - REQs: REQ-F-8002, REQ-F-8003, REQ-NF-14001
  - Check: `InputLevelMeter.qml` binds to `AudioService.inputLevel`, renders ten fixed-height segments using cyan/passive semantic colors, and initializes inactive; the popup root exclusively owns monitoring acquisition and release.

- [x] T-016: Create `AudioCurrentDeviceRow.qml` summary row with metadata and optional meter
  - REQs: REQ-F-5001, REQ-F-6001, REQ-NF-14001, REQ-NF-14002, REQ-NF-14003
  - Check: `AudioCurrentDeviceRow.qml` takes `required` properties (iconName, title, subtitle, expanded, accentColor) and optional `trailingContent` component; renders icon + name + metadata subtitle + DEFAULT pill + chevron (rotation 90°/−90° with Behavior); row is clickable and emits `toggled()` on click; chevron is visible and rotates smoothly.

- [x] T-017: Extend `AudioDeviceDelegate.qml` with radio-checkmark, new icon role, and metadata subtitle
  - REQs: REQ-F-11001, REQ-F-4002, REQ-F-3003, REQ-NF-14001
  - Check: `AudioDeviceDelegate.qml` adds radio-circle glyph (filled for `isDefault: true`, empty circle otherwise) in `leadingContent` using inline `Rectangle` + `Shape` with tick `PathSvg`; changes `ExternalIcon.iconName` to read `root.model.iconName` with fallback to hardcoded defaults via `fallbackIconName`; replaces `subtitle` to use `AudioMetadataFormat.formatDeviceMetadata()`; removes old `deviceMuteButton` trio.

- [x] T-018: Extend `AudioStreamDelegate.qml` with disabled "..." button
  - REQs: REQ-F-9001, REQ-F-12002, REQ-C-15003, REQ-NF-14001
  - Check: `AudioStreamDelegate.qml` adds static "..." `HnIcon` in `trailingContent` with `iconState: HnIcon.Disabled`; no `MouseArea`/`TapHandler` attached to button; removes old `streamMuteButton` trio; button is visually disabled (grayed out).

## QML: Composite Sections & Layout

- [x] T-019: Create `AudioDeviceSection.qml` parameterized output/input accordion wrapper
  - REQs: REQ-F-5001, REQ-F-5002, REQ-F-5003, REQ-F-6001, REQ-F-6002, REQ-F-6003, REQ-NF-14001, REQ-NF-14003
  - Check: `AudioDeviceSection.qml` takes `required bool isInput` and `bool expanded`; emits `expandRequested()` when summary row is clicked; conditionally renders "OUTPUT DEVICES" or "INPUT DEVICES" label only when expanded; composes `AudioCurrentDeviceRow` (with meter only for Input) + existing `AudioDeviceList` unchanged; maps Input/Output model + accent colors + default-setter target internally.

- [x] T-020: Create `AudioApplicationsSection.qml` with 4-row cap and "Show All" toggle
  - REQs: REQ-F-9001, REQ-F-9002, REQ-F-9003, REQ-NF-14001, REQ-NF-14003
  - Check: `AudioApplicationsSection.qml` renders "APPLICATION VOLUME" label + "Show All" text + chevron (plain `MouseArea`, not `TapHandler`) toggling `showAll` property; wraps `AudioStreamList` in a clip container with `height: showAll ? contentHeight : Math.min(contentHeight, collapsedHeight)` where `collapsedHeight` is 4 rows + spacing; container animates height smoothly; fewer than 4 streams renders all at natural height.

- [x] T-021: Create `KeyboardHintFooter.qml` static visual footer bar
  - REQs: REQ-F-10001, REQ-F-10002, REQ-NF-14001, REQ-NF-14003
  - Check: `KeyboardHintFooter.qml` renders three hint chips ("Tab + ↔ Navigate", "Enter Select", "M Mute") at bottom with consistent styling; no keyboard event handlers attached; no `Keys.onPressed` or global shortcuts added in any file.

## QML: Root Redesign

- [x] T-022: Rewrite `AudioPopupContent.qml` as single-column accordion layout
  - REQs: REQ-F-1001, REQ-F-1002, REQ-F-7001, REQ-F-7002, REQ-F-8004, REQ-NF-14003
  - Check: `AudioPopupContent.qml` uses pinned Header/Master and Footer regions around one scrollable column; renders Output → Applications → Input; owns mutually exclusive accordion state and the input-monitor lifecycle; separators span the full panel interior.

- [x] T-023: Delete `AudioTabSidebar.qml` (no longer referenced)
  - REQs: REQ-F-1001
  - Check: `AudioTabSidebar.qml` is deleted from disk; `AudioPopupContent.qml` imports have no reference to it.

## QtQuickTest Coverage

- [x] T-024: Write QML unit tests for audio popup redesign
  - REQs: REQ-F-7001, REQ-F-7002, REQ-F-11001, REQ-F-3003, REQ-F-9001, REQ-F-9002, REQ-F-8003, REQ-F-12001, REQ-F-12002
  - Check: `tst_AudioPopupRedesign.qml` instantiates full `AudioPopupContent` or isolated `AudioDeviceSection` pairs and verifies: exactly one device list is visible (accordion mutual exclusivity); default state is Output expanded, Input collapsed; radio-circle `Rectangle` color switches between transparent and accentColor as `isDefault` flips; `AudioMetadataFormat.formatDeviceMetadata()` handles normal/zero-channel/zero-rate/Bluetooth cases correctly; `clipContainer.height` respects 4-row cap and `showAll` toggle; `findChild` fails to locate `MouseArea`/`TapHandler` on gear and "..." buttons (structural absence); `InputLevelMeter` instantiated with various levels renders proportional bar heights and no error text.

## Verification & Build

- [x] T-030: Remediate popup composition and reference geometry
  - Check: audio content policy is 780×820 with 600px minimum width; section order is Header → Master → Output → Applications → Input → Footer; Input summary owns meter + percentage and both summary rows retain DEFAULT badges; expanded lists contain no duplicate DEFAULT badge; sample rates use localized kHz formatting.

- [x] T-031: Integrate page-aware settings navigation
  - Check: shell-owned `SettingsNavigationService` sends asynchronous `ActivateAction("audio", [], {})`; enabled accessible gear dispatches it and closes the popup; unit and QML tests cover the message and click behavior.

- [x] T-025: Run `task build` and verify no errors
  - REQs: All (every task contributes to a buildable state)
  - Check: `task build` completes with exit code 0; no new compilation errors or warnings in audio subsystem.

- [x] T-026: Run `task qml-lint` on all audio QML files
  - REQs: REQ-NF-14001
  - Check: `task qml-lint` produces no errors on any new or edited QML file under `apps/shell/qml/Popups/Audio/` or modified Qt QML test files; warnings pre-existing to this cycle are not flagged as new failures.

- [x] T-027: Run `task format-check` and `task tidy` on all files touched this cycle
  - REQs: REQ-C-15007
  - Check: `task format-check` passes on edited `.qml` and `.js` files (no reformatting needed); `task tidy` runs on all edited C++ files under `libs/holonight-services/src/audio/` and `tests/` with no new errors reported in those files (pre-existing warnings unrelated to this cycle are not flagged).

- [x] T-028: Run `ctest` for audio subsystem
  - REQs: REQ-F-13001–REQ-F-13005, REQ-F-8001–REQ-F-8004
  - Check: All three extended GTest files (`test_audio_device_model.cpp`, `test_pulse_audio_backend.cpp`, `test_audio_service.cpp`) pass; offscreen QML harness (`test_holonight_qml_harness -V` with `AudioPopupRedesign` filter) runs without timeout or assertion failure.

## Manual Integration Follow-up

- [ ] T-029: Optional hardware-matrix testing per DESIGN.md §13.3
  - REQs: REQ-F-1001–REQ-F-13005, REQ-NF-14001–REQ-C-15007
  - Check: Exercise microphone activity/silence, default-input switching, and USB unplug/replug across representative hardware. This is a non-blocking compatibility matrix, not unfinished SDD scope.

---

## Summary

**Total Tasks**: 31 implementation tasks complete; one optional hardware-matrix follow-up remains.

**Ordering Rationale**:

1. **Buildability at each step**: C++ types and models are added before any QML can bind to them (T-001 → T-002 → T-003 → T-004 → T-005 → T-006 → T-007); GTest coverage (T-008–T-011) immediately follows backend code to catch bugs before QML integration.

2. **QML dependencies**: Leaf components that have no interdependencies (`AudioMetadataFormat.js`, `AudioPopupHeader.qml`, `AudioMasterPanel.qml`, `InputLevelMeter.qml`, `AudioCurrentDeviceRow.qml`) are written before composite sections that consume them (T-012 → T-013–T-018 → T-019–T-021).

3. **Root assembly last**: The complex accordion state management in `AudioPopupContent.qml` (T-022) only lands once all child components are complete and individually unit-tested (T-024).

4. **Deletion timing**: Old `AudioTabSidebar.qml` (T-023) is deleted after `AudioPopupContent` is rewritten to remove dependencies, keeping intermediate states buildable.

5. **Verification in order**: Build (T-025), linting (T-026), static analysis (T-027), and unit tests (T-028) run before manual integration (T-029), catching integration issues early.

6. **Manual testing last**: Live Wayland testing (T-029) is deferred per project convention (never drive shell UI programmatically; provide a checklist for human confirmation), ensuring all automated checks pass first.

This ordering ensures the project remains buildable and testable after every task, matching CLAUDE.md's emphasis on incremental verification and the SDD cycle's goal of catching architectural mismatches early rather than at integration time.

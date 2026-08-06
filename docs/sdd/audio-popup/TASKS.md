# SDD Tasks — audio-popup

## Backend & Model Changes (C++ Foundation)

- [x] T-001: Add `icon_name` field to AudioStream struct
  - REQs: REQ-F-018, REQ-NF-002
  - Check: `src/services/audio/AudioTypes.h` contains `QString icon_name` member in `AudioStream` struct; existing code compiles without breaking.

- [x] T-002: Add IconName role to AudioStreamModel
  - REQs: REQ-F-018, REQ-NF-008
  - Check: `AudioStreamModel::Role` enum includes `IconName`; `roleNames()` returns `"iconName"` for that role; `data()` returns the icon_name field; `task build` succeeds.

- [x] T-003: Populate icon_name from PulseAudio in PulseAudioBackend
  - REQs: REQ-F-018, REQ-NF-001
  - Check: `sinkInputToStream()` and `sourceOutputToStream()` read `PA_PROP_APPLICATION_ICON_NAME` from the proplist; icon names populate correctly in models (verified via GTest).

- [x] T-004: Filter monitor sources in PulseAudioBackend
  - REQs: REQ-F-010, REQ-NF-001
  - Check: `sourceListCallback()` and `sourceChangedCallback()` skip sources where `info->monitor_of_sink != PA_INVALID_INDEX`; no device with `.monitor` in name appears in the Input Devices list.

- [x] T-005: Add setDefaultOutputByName and setDefaultInputByName to PulseAudioBackend
  - REQs: REQ-F-008, REQ-F-015
  - Check: `PulseAudioBackend` provides `void setDefaultOutputByName(const QString& name)` and `setDefaultInputByName(const QString& name)` methods; they call `pa_context_set_default_sink/source` with the name string.

- [x] T-006: Expose defaultOutputId property and signal in AudioService
  - REQs: REQ-F-005, REQ-F-024, REQ-F-025, REQ-NF-002
  - Check: `AudioService` declares `Q_PROPERTY(quint32 defaultOutputId ...)` with `defaultOutputIdChanged()` signal; `applyDefaultDeviceState()` emits the signal when the default device changes; `task build` succeeds.

- [x] T-007: Add setDefaultOutputByName and setDefaultInputByName invokables to AudioService
  - REQs: REQ-F-008, REQ-F-015
  - Check: `AudioService` declares `Q_INVOKABLE void setDefaultOutputByName(const QString& name)` and `setDefaultInputByName(const QString& name)`; they delegate to the backend; `task build` succeeds.

## StatusPopupSurface Fix

- [x] T-008: Register IconImageProvider in StatusPopupSurface
  - REQs: REQ-F-018, REQ-C-005, REQ-NF-003
  - Check: `src/surfaces/StatusPopupSurface.cpp` includes `IconImageProvider.h` and calls `view_->engine()->addImageProvider(...)` before setting the source; `image://icon/...` URLs resolve correctly in the popup (verified via manual Wayland test).

## GTest Coverage for New Backend Changes

- [x] T-009: Add GTest for AudioStreamModel IconName role
  - REQs: REQ-F-018, REQ-NF-002
  - Check: `tests/test_audio_stream_model.cpp` contains a test that constructs a stream with `icon_name = "firefox"`, adds it to the model, and verifies `data(..., Role::IconName)` returns the correct string.

- [x] T-010: Add GTest for AudioService defaultOutputId property and signal
  - REQs: REQ-F-005, REQ-NF-002
  - Check: `tests/test_audio_service.cpp` contains tests verifying `defaultOutputId()` returns the correct id when a default device is set, and that `defaultOutputIdChanged()` signal fires; `task test` passes all audio tests.

- [x] T-011: Add GTest for setDefaultOutputByName no-op behavior
  - REQs: REQ-NF-002
  - Check: `tests/test_audio_service.cpp` contains a test calling `service.setDefaultOutputByName("test")` on a SkipInit service; no crash occurs; `task test` passes.

## QML Leaf Components

- [x] T-012: Create AudioVolumeSlider component
  - REQs: REQ-F-007, REQ-F-014, REQ-F-023, REQ-F-031
  - Check: `src/qml/AudioPopup/AudioVolumeSlider.qml` exists; it exposes `value`, `dragging`, `accentColor` properties and emits `valueChanging(int)` and `valueCommitted(int)` signals; `task qml-lint` passes; added to HOLONIGHT_QML_FILES.

- [x] T-013: Create AudioDeviceDelegate component
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-034
  - Check: `src/qml/AudioPopup/AudioDeviceDelegate.qml` renders device row with icon, name, description, mute button, volume slider, percentage, and DEFAULT badge; uses HoloniightPalette tokens only; name elides with "…"; `task qml-lint` passes; added to HOLONIGHT_QML_FILES.

- [x] T-014: Create AudioStreamDelegate component
  - REQs: REQ-F-018, REQ-F-019, REQ-F-020, REQ-C-005
  - Check: `src/qml/AudioPopup/AudioStreamDelegate.qml` renders stream row with icon (from `model.iconName` with fallback to audio-x-generic), application name, media title, mute button, volume slider, percentage; uses HoloniightPalette tokens only; `task qml-lint` passes; added to HOLONIGHT_QML_FILES.

## QML Composite Components (Lists & Sidebar)

- [x] T-015: Create AudioDeviceList component
  - REQs: REQ-F-003, REQ-F-010, REQ-NF-008
  - Check: `src/qml/AudioPopup/AudioDeviceList.qml` is a ListView wrapping AudioDeviceDelegate; accepts `model`, `accentColor`, `isInput` properties; supports scrolling; `task qml-lint` passes; added to HOLONIGHT_QML_FILES.

- [x] T-016: Create AudioStreamList component
  - REQs: REQ-F-017, REQ-NF-008
  - Check: `src/qml/AudioPopup/AudioStreamList.qml` is a ListView wrapping AudioStreamDelegate; accepts `model` property; supports scrolling; `task qml-lint` passes; added to HOLONIGHT_QML_FILES.

- [x] T-017: Create AudioTabSidebar component
  - REQs: REQ-F-001, REQ-F-016
  - Check: `src/qml/AudioPopup/AudioTabSidebar.qml` displays three tab buttons ("Output Devices", "Input Devices", "Applications"); left accent bar visible when active (cyan for output/applications, violet for input); `task qml-lint` passes; added to HOLONIGHT_QML_FILES.

- [x] T-018: Create AudioMasterBar component
  - REQs: REQ-F-022, REQ-F-023, REQ-F-024, REQ-F-025
  - Check: `src/qml/AudioPopup/AudioMasterBar.qml` displays "Master Volume" label, AudioVolumeSlider, and percentage at popup bottom; binds to AudioService.defaultOutputId and volume; slider updates propagate to AudioService; `task qml-lint` passes; added to HOLONIGHT_QML_FILES.

## QML Root Content & Integration

- [x] T-019: Create AudioPopupContent root component
  - REQs: REQ-F-001, REQ-F-002, REQ-F-032, REQ-F-033, REQ-NF-003, REQ-NF-004
  - Check: `src/qml/AudioPopup/AudioPopupContent.qml` is a root Item with RowLayout; left sidebar (AudioTabSidebar, 160px), main content area (AudioDeviceList/AudioStreamList/empty-state Loader, dynamic), AudioMasterBar pinned bottom; shows empty-state message when `!AudioService.available`; shows "No devices found" when lists are empty; uses HoloniightPalette only; `task qml-lint` passes; added to HOLONIGHT_QML_FILES.

## Wiring QML into StatusPopup

- [x] T-020: Wire audio popup to StatusPopup.qml and CMakeLists.txt
  - REQs: REQ-NF-003, REQ-NF-004, REQ-NF-005
  - Check: `src/qml/Topbar/StatusPopup.qml` includes `"audio": "AudioPopup/AudioPopupContent.qml"` in `popupSources` map; all eight QML files are added to `HOLONIGHT_QML_FILES` in CMakeLists.txt in sorted order; `task build` succeeds and `task qml-lint` passes; `qrc:/HolonightShell/AudioPopup/` paths resolve correctly.

## Integration & Manual Verification

- [x] T-021: Verify build, formatting, and static analysis
  - REQs: REQ-C-004, REQ-NF-006
  - Check: `task build` succeeds; `task qml-lint` reports no errors on new QML files; `task format-check` passes; `task tidy` reports no new issues in modified C++ files.

- [x] T-022: Manual Wayland verification of audio popup feature
  - REQs: REQ-F-001..034, REQ-C-002, REQ-C-003, REQ-C-006
  - Check: Run `task run` in a live Wayland session; click top-bar AudioWidget to open popup; verify: (a) three tabs switch content correctly with Output tab default; (b) device/stream rows render with correct layout and no overflow; (c) mute buttons toggle audio state; (d) sliders update volume correctly via pactl verification; (e) DEFAULT badges appear and move on device selection; (f) Master Volume bar is pinned and synchronizes bidirectionally with default sink slider; (g) external pactl commands cause live updates within 100ms; (h) empty-state messages display when lists are empty or audio service is unavailable; (i) clicking outside popup closes it, clicking widget reopens it (toggle behavior); (j) AudioWidget in top bar continues to display default sink volume and mute state without regression.

- [x] T-023: Verify all GTest suites pass
  - REQs: REQ-NF-001, REQ-NF-002
  - Check: Run `task test` from the build directory; all audio service and model tests pass (existing + new); no thread-safety warnings from test execution; coverage includes icon_name role, defaultOutputId tracking, and setDefaultOutputByName no-op.

---

## Summary

**Total tasks**: 23

**Ordering rationale**: 
1. Tasks T-001..T-007 establish the C++ backend (new fields, roles, invokables, properties)
2. Task T-008 fixes the StatusPopupSurface icon provider registration gap
3. Tasks T-009..T-011 add GTest coverage for new backend changes
4. Tasks T-012..T-018 build QML components bottom-up (leaf sliders and delegates before composite lists and sidebar)
5. Task T-019 assembles the root AudioPopupContent from all leaf components
6. Task T-020 wires the popup into StatusPopup.qml and updates CMakeLists.txt (unblocks build)
7. Task T-021 verifies all automated checks (build, lint, format, tidy)
8. Task T-022 covers full manual Wayland integration testing against all functional and constraint requirements
9. Task T-023 verifies all unit tests pass with new coverage

Each task builds on the previous; the project remains buildable after each task.

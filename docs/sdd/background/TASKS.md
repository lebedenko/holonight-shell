# SDD Tasks — background

- [x] T-001: Add BackgroundConfig struct to ConfigService.h
  - REQs: REQ-F-003, REQ-NF-002
  - Check: ConfigService.h declares BackgroundConfig with QStringList images field and operator== using order-sensitive comparison; background() accessor and backgroundChanged() signal exist.

- [x] T-002: Implement parseBackground() and integrate into ConfigService::parseFile()
  - REQs: REQ-F-003, REQ-F-004, REQ-F-013, REQ-NF-002
  - Check: parseBackground() in ConfigService.cpp expands tilde paths and parses [background]/images as a TOML array; parseFile() calls it, emits backgroundChanged() only if the parsed value differs; writeMissingDefaults() and writeConfig() write back [background] with images=[] when absent.

- [x] T-003: Implement resolveImagePath() helper in BackgroundManager (placed as BackgroundConfig::imageForMonitor in holonight_core so core tests can link it)
  - REQs: REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009
  - Check: resolveImagePath(const QStringList& images, int monitorIndex) returns images[monitorIndex] if in bounds, returns images.last() if monitorIndex >= size, and returns "" if images is empty.

- [x] T-004: Create BackgroundManager.h skeleton with initialization guards
  - REQs: REQ-F-001, REQ-F-016, REQ-NF-003
  - Check: BackgroundManager class declares LayerShell shell_ member, bool initialized_ guard, std::vector<std::pair<LayerSurface*, QQuickView*>> backgrounds_, and connects to ConfigService::backgroundChanged() signal in constructor.

- [x] T-005: Implement BackgroundManager::initializeBackgrounds() and createBackground()
  - REQs: REQ-F-001, REQ-F-002, REQ-C-002, REQ-F-016
  - Check: initializeBackgrounds() iterates QGuiApplication::screens(), calls createBackground() per screen; createBackground() sets layer_background, anchors all four edges, exclusive zone 0, creates empty input region via wl_compositor_create_region(), injects imagePath via setInitialProperties, and sets source to qrc:/HolonightShell/Background/Background.qml.

- [x] T-006: Implement BackgroundManager::onBackgroundChanged() live-reload slot
  - REQs: REQ-F-013, REQ-F-014
  - Check: onBackgroundChanged() re-resolves imagePath for each background view via resolveImagePath(), calls view->rootObject()->setProperty("imagePath", newPath) for each, triggering QML crossfade animation.

- [x] T-007: Create Background.qml with two-layer Image structure and 250ms crossfade
  - REQs: REQ-F-009, REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-014, REQ-F-015, REQ-NF-001, REQ-NF-004
  - Check: Background.qml declares required property string imagePath; has surfaceVariant Rectangle base; two Image layers (currentImage/incomingImage) with PreserveAspectCrop and sourceSize Qt.size(Screen.width, Screen.height); NumberAnimation fadeIn 250ms cross-fades incomingImage.opacity from 0 to 1 and swaps currentImage on finish; onImagePathChanged triggers crossfade or clears both images if imagePath is empty; both Images log console.warn on Image.Error.

- [x] T-008: Add BackgroundManager to CMakeLists.txt and register Background.qml
  - REQs: REQ-C-001, REQ-C-003
  - Check: CMakeLists.txt lists BackgroundManager.h and BackgroundManager.cpp in holonight_surfaces target; src/qml/Background/Background.qml is added to HOLONIGHT_QML_FILES.

- [x] T-009: Wire BackgroundManager into ShellApplication lifecycle
  - REQs: REQ-NF-003
  - Check: ShellApplication::startShell() constructs BackgroundManager after layer_shell_manager_, passing config_service_ as parameter; BackgroundManager is stored as member and lives until ShellApplication destruction.

- [x] T-010: Unit test BackgroundConfig parsing and operator==
  - REQs: REQ-F-003, REQ-F-004, REQ-NF-002
  - Check: ConfigServiceBackgroundTest::ParsesBackgroundSection, TildeExpansionApplied, EmptySectionGivesSolidFallback, AbsentSectionGivesSolidFallback, OperatorEqualsIsOrderSensitive, WritesBackMissingSection, SignalEmittedOnChange, SignalNotEmittedOnSameValue all pass via ctest.

- [x] T-011: Unit test resolveImagePath() mapping logic
  - REQs: REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009
  - Check: ConfigServiceBackgroundTest::OverflowIgnored (3 images, 1 screen returns first), UnderflowRepeatsLast (1 image, 3 screens returns same image), EmptyListReturnsEmpty (empty list returns "") all pass via ctest.

- [x] T-012: Build, format, lint, and manual visual verification (automated gates pass: build, format-check, qml-lint, 30/30 ctest, clang-tidy exit 0; manual visual verification confirmed on live Hyprland session after fixing three runtime-only bugs: exclusive_zone -1 for full-screen-under-bar, file:// URL for Image.source, and the setInitialProperties onImagePathChanged double-trigger crossfade race that blanked the initial wallpaper)
  - REQs: REQ-C-001, REQ-F-001 through REQ-F-016, REQ-NF-001 through REQ-NF-004, REQ-C-002, REQ-C-003, REQ-C-004
  - Check: task build succeeds with no errors; task format-check reports no violations; task qml-lint reports no errors in Background.qml; manual test on real Wayland compositor confirms full-screen image rendering, crossfade on reload, fallback to surfaceVariant, input passthrough, and bar renders above background.

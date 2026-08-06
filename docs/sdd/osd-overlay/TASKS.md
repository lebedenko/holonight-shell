# SDD Tasks — osd-overlay

- [x] T-001: Define OSD config structs with defaults
  - REQs: REQ-C-009, REQ-C-005, REQ-C-011
  - Check: `OsdConfig` and `OsdChannelConfig` compile with default-initialized values matching spec (enabled=true, timeout=1500, position=CenterBottom); a unit test round-trips TOML with all six keys through parser and writer unchanged.

- [x] T-002: Integrate OSD config into ConfigService
  - REQs: REQ-C-009
  - Check: `ConfigService::osd()` returns `const OsdConfig&`; `osdConfigChanged()` signal emits when config reloads; `task test` passes a unit test parsing config with/without `[osd]` section and verifying defaults.

- [x] T-003: Define normalized OSD event types
  - REQs: REQ-F-001
  - Check: `OsdLevelEvent` and `OsdSelectionEvent` compile as `Q_GADGET` value types with correct members (channel, value/muted, short_label/full_label); both are copyable and assignable; `Q_DECLARE_METATYPE` declarations are present; a unit test instantiates both and assigns values without error.

- [x] T-004: Implement OsdChannelSource base class
  - REQs: REQ-F-002
  - Check: `OsdChannelSource` declares `channel()` pure virtual and both `eventObserved`/`availableChanged` signals; a test-only `FakeChannelSource` subclass compiles and can emit both signals on demand; `grep OsdController.cpp` finds zero references to `AudioService`, `BrightnessService`, or `KeyboardLayoutService`.

- [x] T-005: Implement AudioChannelSource and BrightnessChannelSource adapters
  - REQs: REQ-F-009, REQ-F-010
  - Check: `AudioChannelSource` connects to `AudioService` volume/muted/available signals and emits `OsdLevelEvent` with correct channel names; `BrightnessChannelSource` emits with `channel="screen-brightness", muted=false`; unit tests with mocks verify correct event emission for each service.

- [x] T-006: Add layoutName property to KeyboardLayoutService
  - REQs: REQ-C-014
  - Check: `KeyboardLayoutService::layoutName()` returns `QString`; `Q_PROPERTY(QString layoutName ... NOTIFY layoutNameChanged)` is declared; `setLayoutName()` retains the full name while `setLayoutCode()` sets the code; unit test feeds "English (US)"/"EN" pair and verifies both are stored; existing keyboard-layout-service tests pass unchanged.

- [x] T-007: Implement KeyboardLayoutChannelSource adapter
  - REQs: REQ-F-011
  - Check: `KeyboardLayoutChannelSource` observes both `layoutCodeChanged` and `layoutNameChanged` signals; emits `OsdSelectionEvent` with `short_label = layoutCode()` and `fullLabel` derived per spec (fallback to code if name is empty); unit test verifies diff compares only short_label, so name-only changes emit no event.

- [x] T-008: Implement OsdController state machine and gating
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-007, REQ-F-008, REQ-NF-002, REQ-NF-008, REQ-NF-010
  - Check: Constructor accepts `std::vector<OsdChannelSource*>` (raw, Qt-parented — NOT `shared_ptr`, per REQ-F-025) and a `NowFn` defaulting to `steady_clock::now`; `onEventObserved` updates `cache_` unconditionally, then gates in the order prime → diff → master_enabled → channel_enabled → grace → suppressed; `displayLevelEvent`/`displaySelectionEvent` dispatch by `std::visit` on the variant alternative; `setSuppressed`/`setChannelEnabled`/`setEnabled`/`setTimeoutMs` are `Q_INVOKABLE`. `ctest -R test_osd_controller` passes with cases proving: first observed event emits nothing, a repeat of the same value emits nothing, and a changed value after the grace period emits exactly one signal.

- [x] T-009: Extend OsdController tests to the full gating matrix
  - REQs: REQ-C-015, REQ-F-004, REQ-F-005, REQ-F-007, REQ-F-017, REQ-C-011
  - Check: `ctest -R test_osd_controller` covers, beyond T-008: an event discarded by the grace period still updates the cache (proven by the next in-grace repeat emitting nothing after the clock advances); un-suppressing does not retroactively fire; per-channel disable blocks only its own channel; `setTimeoutMs` clamps to [300, 10000]; the hide timer restarts rather than stacks on same-channel updates (real `QTimer` + `QSignalSpy::wait()`). No test touches PulseAudio, D-Bus, sysfs, or a real clock.

- [x] T-010: Extract anchorFlagsForPosition helper from WidgetSurfacePolicy
  - REQs: REQ-C-006
  - Check: `anchorFlagsForPosition(WidgetPosition)` is declared public in `WidgetSurfacePolicy.h` and moved out of anonymous namespace in `.cpp`; `widgetSurfacePlacement()` calls it unchanged; existing widget-placement tests still pass; `task architecture-check` passes.

- [x] T-011: Add currentTabChanged signal and accessor to SidebarManager
  - REQs: REQ-F-006
  - Check: `SidebarManager` emits `currentTabChanged(const QString& monitor_name, int tab_index)` from `onCurrentTabChanged()`; `[[nodiscard]] Q_INVOKABLE int currentTabForMonitor(const QString& monitor_name) const` accessor works over existing `current_tabs_` map; unit test calls accessor after tab change and gets correct value.

- [x] T-012: Implement OsdSurface C++ class with layer-shell lifecycle
  - REQs: REQ-F-012, REQ-F-013, REQ-C-002, REQ-C-004, REQ-C-013, REQ-NF-007, REQ-NF-009
  - Check: `OsdSurface` owns `LayerShell`, `LayerSurface*`, and `QQuickView*`; `ensureSurface(screen)` creates view on-demand or reuses if same monitor, destroys and rebuilds if monitor changes (matching `NotificationToastSurface` pattern exactly); `showLevel`/`showSelection` push content; `hide()` starts exit animation; `isActive()` returns false after `destroySurface()`; layer-shell role is set to overlay; margins/anchors use extracted `anchorFlagsForPosition()`; `updateSurfaceSize()` connects to `implicitWidth/HeightChanged` and resizes layer surface to match QML content.

- [x] T-013: Implement input region and keyboard interactivity in OsdSurface
  - REQs: REQ-F-024, REQ-C-013
  - Check: `OsdSurface::applyInputRegion()` creates empty `wl_region` via native compositor interface and sets it on `wl_surface`; method is called from `createSurface()` and connected to `LayerSurface::configured()` via `Qt::SingleShotConnection` with `if (!isActive()) return;` guard (per CLAUDE.md race documentation); `set_keyboard_interactivity(keyboard_interactivity_none)` is called at creation; surface width/height track QML content (not full output width); manual check: clicking the OSD area focuses the window behind it.

- [x] T-014: Add brightness icon to UtilityIcon/BarIcon QML
  - REQs: REQ-F-021
  - Check: `UtilityIcon.qml` gains a `"brightness"` branch in `drawUtility()` drawing a sun glyph with `HoloniightPalette` stroke colors, and `BarIcon.qml`'s dispatch list routes `"brightness"` to it; `task qml-lint` passes and `grep -n '#[0-9a-fA-F]\{6\}'` finds no hex literal in either file. (Ordered before the renderers because T-015 references this icon name.)

- [x] T-015: Implement OsdLevelRenderer QML component with icon mapping
  - REQs: REQ-F-014, REQ-F-020, REQ-F-023, REQ-NF-003, REQ-NF-004
  - Check: `OsdLevelRenderer.qml` accepts `channel`, `value` (0–100), `muted`; icon maps to `audio-volume-muted` when muted, `audio-volume-high/medium/low` at ≥67/≥34/below, and `brightness` for `screen-brightness`; when muted the bar still renders at the real value but dimmed, with the label reading "Muted" rather than a percentage; `Behavior on value` animates over 100 ms; `task qml-lint` and `task build` pass; no hex color literal appears in the file.

- [x] T-016: Implement OsdSelectionRenderer QML component
  - REQs: REQ-F-015, REQ-NF-003, REQ-NF-004
  - Check: `OsdSelectionRenderer.qml` accepts `shortLabel` (large) and `fullLabel` (smaller), renders no progress bar, routes its icon through `BarIcon { name: "keyboard" }`, and cross-fades on update via `Behavior on opacity`; `task qml-lint` passes; no hex color literal appears in the file.

- [x] T-017: Implement OsdSurface QML root with entrance/exit and dispatch
  - REQs: REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-019, REQ-NF-009, REQ-NF-003
  - Check: `OsdView.qml`'s root exposes `kind`, `channel`, `value`, `muted`, `shortLabel`, `fullLabel`, `hiding`, `configured`; nothing renders until `configured` is true (REQ-NF-009); a `Loader` dispatches on `kind` to the two renderers; a same-channel update animates in place while a channel switch replays the entrance; `visible` is gated on the animated opacity crossing zero, not on `hiding` directly (per the CLAUDE.md `visible`-beats-Behavior gotcha). `task qml-lint` passes and the file contains no `MouseArea`, `TapHandler`, or `focus:` assignment. Entrance/exit smoothness and the update-vs-replace distinction are **manual checks in a live Hyprland session** — no automated assertion exists for them.

- [x] T-018: Construct OsdController and OsdSurface in ShellApplication
  - REQs: REQ-C-001, REQ-C-002
  - Check: `ShellApplication.h` declares `OsdController* osd_controller_` and `OsdSurface* osd_surface_` immediately after `brightness_service_` member; constructor initializer-list creates both after all three services (audio, brightness, keyboard_layout) exist; both are parented to `this`; `registerQmlTypes()` registers both as QML singletons; `task build` succeeds; `task architecture-check` passes.

- [x] T-019: Wire OsdController to OsdSurface in ShellApplication
  - REQs: REQ-F-008, REQ-C-012
  - Check: `ShellApplication::startServices()` connects `OsdController::displayLevelEvent` → `ShellApplication` → `OsdSurface::showLevel` (via `ensureSurface(resolveOsdMonitor())`); similar for `displaySelectionEvent` → `showSelection`; `hideRequested` → `hide()`; `resolveOsdMonitor()` returns `ActiveWindowService::focusedMonitorName()` or primary screen fallback; manual test: volume change shows OSD on focused monitor; switching focus to other monitor routes next event there.

- [x] T-020: Wire OSD suppression in ShellApplication
  - REQs: REQ-F-006, REQ-C-007, REQ-C-008
  - Check: `ShellApplication` slots `updateAudioOsdSuppression()` and `updateBrightnessOsdSuppression()` recompute suppression flags by checking popup/sidebar visibility; audio slot connects to `StatusPopupSurface::popupVisibleChanged` and `activePopupChanged`; brightness slot connects to `SidebarManager::sidebarOpened/Closed/currentTabChanged`; calls `OsdController::setSuppressed(channel, bool)` accordingly; keyboard-layout never suppressed; `grep -r setSuppressed apps/ libs/` shows only ShellApplication calls; manual test: opening audio popup suppresses volume OSD; opening sidebar on QuickSettings tab suppresses brightness OSD; closing them re-enables.

- [x] T-021: Wire OSD config application in ShellApplication
  - REQs: REQ-C-010, REQ-C-011, REQ-C-005
  - Check: `ShellApplication::applyOsdConfig()` reads `config_service_->osd()` and pushes all fields to `OsdController::setEnabled/setChannelEnabled/setTimeoutMs` and `OsdSurface::setPosition()`; called once in `startServices()` and connected to `ConfigService::osdConfigChanged()` signal; `osd.enabled=false` prevents all events from emitting (verified by GTest); manual test: modifying config and reloading shows position change and per-channel disable take effect immediately.

- [x] T-022: Add QML tests for OSD renderers
  - REQs: REQ-F-014, REQ-F-015, REQ-NF-003
  - Check: `tests/qml/tst_OsdLevelRenderer.qml` and `tst_OsdSelectionRenderer.qml` instantiate renderers; test icon mapping (volume tiers, muted, brightness), text rendering (percentage vs. "Muted", labels), and that `Behavior` updates retarget smoothly without restart; `task test` runs harness and tests pass; code coverage for renderer logic is verifiable via QML test assertions.

- [x] T-023: Document the `[osd]` config section and run the live smoke checklist
  - REQs: REQ-C-009, REQ-C-005 (no dedicated documentation requirement exists in SPEC.md — this task is cycle hygiene)
  - Check: `README.md` documents every `[osd]` key with its default and valid range, matching `OsdConfig`'s defaults exactly; `task compositor-smoke-check`, `task format-check`, `task tidy`, `task qml-lint`, and `task test` all run, with any new failures attributable to files touched in this cycle fixed; the SPEC.md "Acceptance Criteria Summary" checklist is walked item by item in a live Hyprland session and every manual item is confirmed by the user.


---

## Live verification record — 2026-07-27

The SPEC.md "Acceptance Criteria Summary" checklist was walked item by item in a live Hyprland
session (two outputs, `eDP-1` and `DP-5`) and every manual item is confirmed. Three defects that
only live testing could surface were found, fixed, and re-verified live in the same session:

1. **Fatal abort at the end of every OSD's fade-out.** `OsdView.qml`'s exit callback reached
   `OsdSurface::destroySurface()`, which `delete`d the `QQuickView` while the animation handler
   that called it was still on the stack — `Object … destroyed while one of its QML signal handlers
   is in progress`, exit 134. Split into a queued, state-guarded `destroyAfterHide()`.
2. **Card clipped on both sides.** The layer surface stayed at its 220x96 fallback because the
   sizing was driven by `implicitWidth/HeightChanged`, which QtQuick.Layouts can emit before the
   final geometry exists (and not at all while the root is invisible). Re-sampled once per frame
   from `QQuickWindow::afterAnimating`.
3. **OSD silently stopped working until restart.** The show was driven by `onChannelChanged`, which
   does not fire for two events on the same channel; when the second landed during the 150 ms
   fade-out the surface was stranded alive at `opacity: 0` with the teardown already cancelled.
   Replaced with an explicit `present()` called from C++ after each content push.

None of the three were detectable by `task build`, `task qml-lint`, `task format-check`,
`task tidy`, or the 1062-test suite, all of which were green before and after. `OsdSurface` has no
unit tests — it requires a live layer-shell — so this class of defect has no automated safety net
in this feature. Treat a green verify run as saying nothing about `OsdSurface.cpp`/`OsdView.qml`.

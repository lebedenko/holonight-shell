# SDD Tasks — mpris-desktop-widget

**Status: Complete — implementation and live compositor verification finalized 2026-08-04.**

## Phase 1: Cross-Repo Prerequisite (holonight-config in holonight-settings repo)

**HARD DEPENDENCY: All holonight-shell tasks are blocked until these complete and the package is installed.**

- [x] T-001: Add WidgetType::Mpris enum and MprisWidgetConfig struct to holonight-config
  - REQs: REQ-C-001, REQ-C-002
  - Check: After editing `holonight-config/include/holonight_config/config_structs.h`, the new `WidgetType::Mpris` enum value and `MprisWidgetConfig` struct with `int pause_hide_minutes{10}` field and min/max constants are defined and compile in isolation.

- [x] T-002: Add parseWidgetEntry and writeWidgetDefinition handlers for WidgetType::Mpris in holonight-config
  - REQs: REQ-C-002, REQ-C-003
  - Check: TOML `[[widget]] type = "mpris" pause_hide_minutes = 15` can be parsed without errors; `pause_hide_minutes` outside `[1, 60]` is clamped; config round-trips (parse/write) without loss.

- [x] T-003: Build, test, and install updated holonight-config package
  - REQs: REQ-C-001
  - Check: After `cmake --install` from the holonight-settings build directory, `find_package(HolonightConfig CONFIG REQUIRED)` in holonight-shell's CMake resolves and `WidgetType::Mpris` is available at compile time.

---

## Phase 2: MprisPlayer Struct Extensions

- [x] T-004: Add five new fields to MprisPlayer struct
  - REQs: REQ-F-011, REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-015
  - Check: `MprisPlayer` header declares `QString album`, `QString art_url`, `qint64 position{0}`, `qint64 length{0}`, `bool can_seek{false}` with correct default values; code compiles.

- [x] T-005: Extend MprisMetadata parsing to extract album, art_url, and length
  - REQs: REQ-F-011, REQ-F-012, REQ-F-014
  - Check: `MprisMetadata::extractFields()` parses `xesam:album` (QString), `mpris:artUrl` (QString verbatim), and `mpris:length` (int64, clamped to ≥0) from MPRIS metadata; unit test verifies all three fields populate and handle missing keys correctly.

---

## Phase 3: MprisService Backend Additions (Properties and Centralized Tracking)

- [x] T-006: Add five Q_PROPERTYs to MprisService for album, artwork URL, position, length, and seek capability
  - REQs: REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-019, REQ-F-020
  - Check: `MprisService` declares `activeAlbum`, `activeArtUrl`, `activePosition`, `activeLength`, `activeCanSeek` properties with NOTIFY signals; `applyActiveSnapshot()` diffs and emits each when the active player's field changes.

- [x] T-007: Add manager-lifetime PositionTrackingHandle RAII class and interest-counting infrastructure to MprisService
  - REQs: REQ-F-021, REQ-F-061
  - Check: `MprisService::PositionTrackingHandle` is move-only and has a destructor that calls private `releasePositionTracking()`; each configured `MprisWidgetManager` acquires exactly one handle for its lifetime, independent of occupancy; private `position_tracking_refcount_` tracks configured consumers and reaches zero after config teardown.

- [x] T-008: Implement position extrapolation arithmetic and reconciliation timer in MprisService
  - REQs: REQ-F-021, REQ-F-022, REQ-F-024
  - Check: `activePosition()` read accessor returns `position_mark_us_ + elapsed * rate` when tracking is active and playback is Playing; `reconcile_timer_` (20 s interval, running only when `refcount > 0 && Playing`) issues a fresh Position property read and re-anchors `position_mark_*` every firing; position never drifts more than 500ms over 60 seconds per unit test.

- [x] T-009: Add pause-elapsed-duration tracking (paused_since_ QElapsedTimer) to MprisService
  - REQs: REQ-F-039, REQ-F-040, REQ-F-041, REQ-F-042, REQ-F-044
  - Check: `paused_since_` starts the instant `PlaybackStatus` transitions into "Paused"; `activePauseElapsedMs()` returns `paused_since_.isValid() ? paused_since_.elapsed() : 0`; is invalidated when status leaves "Paused" or active player changes; does not persist across restart (transient QElapsedTimer).

- [x] T-010: Wire Seeked signal handling in MprisPlayerPropWatcher and FakeMprisDBus
  - REQs: REQ-F-013, REQ-F-018, REQ-F-021
  - Check: `MprisPlayerPropWatcher` connects to player's `Seeked(x)` signal (or a new watcher class exists); calls `MprisService::onPlayerSeeked(...)` with position; `position_mark_us_` and `position_mark_elapsed_` are re-anchored on the next GUI event-loop turn; `activePositionChanged` is emitted once; `FakeMprisDBus::emitSeeked(service, positionUs)` exists for deterministic tests.

- [x] T-011: Unit tests for MprisService position tracking, extrapolation, reconciliation, and pause-elapsed
  - REQs: REQ-F-021, REQ-F-022, REQ-F-024, REQ-F-039, REQ-F-042, REQ-F-055, REQ-F-056, REQ-F-059
  - Check: Tests verify position extrapolates at 1x / 2x / 0.5x rates; Seeked corrections land within test tolerance; reconciliation reads occur at ~20 s intervals; active player change invalidates paused_since_; all state resets correctly on track change and active-player switch.

---

## Phase 4: MprisArtworkCache (Download, Decode, LRU Cache)

- [x] T-012: Create MprisArtworkCache class with basic structure, public API, and cache root initialization
  - REQs: REQ-F-026, REQ-F-027, REQ-F-028, REQ-NF-003
  - Check: URL-only `MprisArtworkCache::resolve(url, on_ready)` exists; exact raw URLs identify disk and shared in-flight work, while managers reject stale callbacks with captured URL plus a local generation; cache root is `GenericCacheLocation + "/holonight-shell/mpris-artwork/"`.

- [x] T-013: Implement file:// URL resolution in MprisArtworkCache
  - REQs: REQ-F-027, REQ-F-031
  - Check: `file:///path/to/image.jpg` is read via `QFile` on background thread; passed through decode/scale/cache pipeline; on_ready callback is invoked on GUI thread with local cache file path or empty string on error.

- [x] T-014: Implement http(s):// URL fetching with 10-second timeout and size limits
  - REQs: REQ-F-027, REQ-F-029, REQ-F-033, REQ-F-034
  - Check: Remote URLs are fetched via `QNetworkAccessManager` with `QNetworkRequest::setTransferTimeout(10000)`; download is aborted if file size exceeds 5 MB (`kMaxFileBytes`); timeout or HTTP error invokes callback with empty string; callback runs on GUI thread.

- [x] T-015: Implement image decode at display size and cache write in MprisArtworkCache
  - REQs: REQ-F-031, REQ-NF-003
  - Check: `QImageReader::setScaledSize(512, 512)` is set before `read()` to avoid full-resolution decode; aspect ratio is preserved; decoded image is saved as PNG to cache file; file is written atomically (temp + rename) to prevent partial-file corruption on crash.

- [x] T-015a: Implement bounded base64 `data:` artwork decoding
  - REQs: REQ-F-027, REQ-F-029, REQ-F-031, REQ-F-051, REQ-NF-003
  - Check: `data:image/<type>;base64,<payload>` is validated and decoded off the GUI thread; an encoded-length precheck and decoded-byte limit prevent allocation beyond the 5 MB cap; malformed, non-base64, oversized, and non-image payloads resolve to the bundled media placeholder; tests cover every success and rejection path.

- [x] T-016: Implement URL-only SHA-256 key, cache lookup, and LRU eviction in MprisArtworkCache
  - REQs: REQ-F-028, REQ-F-030, REQ-F-035
  - Check: Cache key is `QCryptographicHash::hash(url.toUtf8(), Sha256)` hex string; filename is `<hash>.png`; different tracks using the exact same URL share one entry; cache hit is ≤50 ms; mtime is updated on every hit; LRU eviction deletes oldest-mtime files until total size ≤50 MB.

- [x] T-017: Unit tests for MprisArtworkCache resolve, cache-hit, eviction, and error handling
  - REQs: REQ-F-026, REQ-F-030, REQ-F-034, REQ-F-057
  - Check: Tests verify URL-only cache reuse within 50 ms across different tracks; LRU eviction respects the 50 MB budget; corrupt entries are rejected and removed; valid base64 `data:` artwork resolves successfully; malformed `data:` and unsupported custom schemes select `assets/media/media-placeholder.svg`; concurrent resolves of the same URL share one in-flight operation; stale callback generations are ignored.

---

## Phase 5: WidgetSurfacePolicy Size Configuration

- [x] T-018: Add four-argument overload to widgetSurfacePlacement() for MPRIS-specific widget dimensions
  - REQs: REQ-F-050
  - Check: `widgetSurfacePlacement(WidgetPosition, int margin, int width, int height)` overload exists and is backward-compatible; `kMprisWidgetWidth = 320`, `kMprisWidgetHeight = 420` constants are defined; two-argument version delegates to four-argument with Clock/TimeToEvent's original constants (460×200).

---

## Phase 6: MprisWidgetManager (Surface Manager with Occupancy and Position Tracking)

- [x] T-019: Create MprisWidgetManager class as PerMonitorLayerManager subclass with base plumbing
  - REQs: REQ-F-001, REQ-F-046
  - Check: `MprisWidgetManager` subclasses `PerMonitorLayerManager`; constructor takes `LayerShell&, WidgetDefinition, int margin, int index, QList<QStringList> position_blockers, MonitorOccupancyService*, MprisService*, QObject*`; `layerConfig()` returns `layer_bottom`, namespace "widget", `Qt::WindowTransparentForInput`; `configureSurface()` uses new MPRIS size constants.

- [x] T-020: Implement occupancy handling and visibility toggling in MprisWidgetManager
  - REQs: REQ-F-046, REQ-F-047, REQ-F-048
  - Check: `onOccupancyChanged(monitorName, is_empty)` pushes `contentVisible` without calling `QQuickItem::setVisible()` or `QQuickView::hide()`; QML remains the sole owner of `visible`; `content_visible_` tracks per-monitor presentation.

- [x] T-021: Implement 2 Hz position-tick timer and pause-hide threshold comparison in MprisWidgetManager
  - REQs: REQ-F-023, REQ-F-039, REQ-F-040
  - Check: `position_tick_timer_` fires every 500 ms (2 Hz) only while any surface is visible (`anySurfaceVisible()`); `onPositionTick()` reads `activePosition()` and `activePauseElapsedMs()` from `MprisService`; pushes `positionUs` to all visible surfaces; compares pause-elapsed against each monitor's configured `pause_hide_minutes * 60'000` to drive Paused-hidden-by-timeout presentation state.

- [x] T-022: Implement resyncSurface() for occupancy-reveal state resynchronization (REQ-F-062)
  - REQs: REQ-F-062
  - Check: On reveal, `resyncSurface(monitorName)` reads all fresh MPRIS state from `MprisService` (metadata, status, position, length, canSeek) and pushes to that monitor's QML root in a single frame; if `activePauseElapsedMs()` is already past threshold at reveal time, surface reveals already in hidden-by-timeout state (not Paused-visible first).

- [x] T-023: Implement metadata change handling and artwork cache integration in MprisWidgetManager
  - REQs: REQ-F-025, REQ-F-032, REQ-F-035, REQ-NF-002
  - Check: The manager connects once to `activeSnapshotChanged` for coherent metadata resync and separately to `activePositionChanged` for immediate corrections; `MprisArtworkCache::resolve(new_url, callback)` shares URL-keyed work; callbacks apply only while captured URL and manager generation remain current.

- [x] T-024: Integrate manager-lifetime PositionTrackingHandle separately from visibility timer state
  - REQs: REQ-F-061
  - Check: `position_tracking_handle_` is acquired once during manager initialization and released only on manager destruction/config teardown; occupancy transitions start/stop only `position_tick_timer_`; there is at most one 20-second reconciliation timer regardless of manager/monitor count; with all surfaces hidden, reconciliation remains active while configured and no 2 Hz presentation work runs.

- [x] T-025: Unit tests for MprisWidgetManager position sampling, occupancy gating, reveal resync, and pause-hide
  - REQs: REQ-F-045, REQ-F-046, REQ-F-047, REQ-F-048, REQ-F-049, REQ-F-061, REQ-F-062
  - Check: Tests verify manager construction/destruction owns exactly one tracking reference and metadata snapshots remain separate from immediate position corrections. Live compositor coverage remains required for occupancy timer gating, reveal resync, and multi-monitor independence.

---

## Phase 7: QML Components (Display, Artwork, Progress Bar)

- [x] T-026: Create MprisWidgetSurface.qml root item with state-driven opacity/visibility
  - REQs: REQ-F-001, REQ-F-003, REQ-F-004, REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009, REQ-F-036, REQ-F-037, REQ-F-038, REQ-F-043, REQ-U-001, REQ-U-002, REQ-U-003
  - Check: Root declares plain properties including QML `real` `positionUs`/`lengthUs`, `contentVisible`, and `pausedTimedOut`; `focusPolicy: Qt.NoFocus`; contains no interactive handlers; explicit Playing, Paused, Stopped, occupancy-hidden, and pause-timeout states preserve the required transitions.

- [x] T-027: Create MprisArtwork.qml with artwork/icon display and opacity
  - REQs: REQ-F-002, REQ-F-051
  - Check: `Image` uses `artworkPath` if non-empty, else `qrc:/HolonightShell/media/media-placeholder.svg`; size ≥256px on both axes; `opacity: 0.5`; `fillMode: Image.PreserveAspectFit`; clipped to layout bounds. A separate 32–48px identity icon uses `image://icon/<desktopEntry>`.

- [x] T-028: Create MprisProgressBar.qml with fractional width and seekability indication
  - REQs: REQ-F-005, REQ-F-010, REQ-F-053, REQ-F-060
  - Check: Thin (2–4px) `Rectangle` bar; `visible: lengthUs > 0`; width fraction is `positionUs / lengthUs`; opacity/color changes when `canSeek == false` (dimmed/lightened variant); no interactive elements; no height reservation when `lengthUs == 0`.

- [x] T-029: Implement graceful fallback displays for missing metadata in QML
  - REQs: REQ-F-051, REQ-F-052, REQ-F-053, REQ-F-054, REQ-F-058
  - Check: Missing `title` falls back to `identity`; missing `album` hides album row (no blank space reserved); missing `length` hides progress bar; bare player icon is always displayed (REQ-F-058); layout never collapses to invisible state; all text elements are at legible font size (≥11pt).

---

## Phase 8: ShellApplication Integration

- [x] T-030: Add MprisArtworkCache member and initialization to ShellApplication
  - REQs: REQ-F-026
  - Check: `ShellApplication` declares `MprisArtworkCache mpris_artwork_cache_;` member; constructed inline (no async dependency at construction time); not registered as a QML singleton (no `reg(...)` call).

- [x] T-031: Modify ShellApplication::rebuildWidgets() to dispatch on WidgetType and construct MprisWidgetManager
  - REQs: REQ-F-001
  - Check: Per-definition loop contains `switch (def.type)` that constructs `WidgetManager` for Clock/TimeToEvent and `MprisWidgetManager` for Mpris; `widget_managers_` container type is `std::vector<std::unique_ptr<PerMonitorLayerManager>>`; MprisWidgetManager is passed `mpris_` and `&mpris_artwork_cache_`.

- [x] T-032: Wire MprisService injection and construction order in ShellApplication
  - REQs: REQ-F-001
  - Check: `mpris_` is constructed before `rebuildWidgets()`; each configured `MprisWidgetManager` acquires its tracking handle during manager initialization and releases it during teardown; member destruction order guarantees managers are destroyed before `mpris_`.

- [x] T-033: Verify and document that MprisArtworkCache is not QML-registered (flag risk in code/comments)
  - REQs: REQ-F-032
  - Check: Code inspection confirms no `reg(mpris_artwork_cache_, "...")` call in `registerQmlTypes()`; a code comment flags this as intentional and refs project memory of the MPRIS pill registration gotcha if future changes expose the class to QML.

---

## Phase 9: Build Verification and Integration Tests

- [x] T-034: Run task build to verify all new code compiles cleanly without warnings
  - REQs: REQ-F-001 through REQ-F-060, REQ-C-001 through REQ-C-003, REQ-U-001 through REQ-U-003, REQ-NF-001 through REQ-NF-003
  - Check: `task build` completes with exit code 0; no new C++ or linker errors; clang-format passes on all new files.

- [x] T-035: Run task qml-lint to verify QML structure and no interactive handlers
  - REQs: REQ-U-002, REQ-U-003
  - Check: `task qml-lint` reports zero errors in `MprisWidgetSurface.qml`, `MprisArtwork.qml`, `MprisProgressBar.qml`; no `MouseArea`, `TapHandler`, `WheelHandler`, or focus-policy warnings.

- [x] T-036: Run task qmltypes-check to verify metatype generation includes new singletons
  - REQs: REQ-F-001
  - Check: `task qmltypes-check` passes; generated qmltypes file contains `MprisService` singleton (already existed, verify still present); no `Module {}` only result.

- [x] T-037: Run task test to execute all C++ and QML unit tests (1119/1122 pass; 3 pre-existing
      failures in ThemeConfigIntegrationTest/SettingsPortalBackendTest — accent-color mismatches
      unrelated to this feature, in files this feature never touched)
  - REQs: REQ-F-021, REQ-F-022, REQ-F-039, REQ-F-042, REQ-F-026, REQ-F-030
  - Check: MprisService position/pause tests, MprisArtworkCache eviction/resolve tests, MprisWidgetManager tests, and QML widget tests all pass; the full suite retains only the three documented unrelated theme/settings failures.

- [x] T-038: Run task architecture-check to verify dependency boundaries
  - REQs: REQ-F-001 through REQ-F-062, REQ-C-001 through REQ-C-003, REQ-U-001 through REQ-U-003, REQ-NF-001 through REQ-NF-003
  - Check: `task architecture-check` passes; no unexpected cross-boundary includes; `holonight_surfaces` correctly depends on `holonight_services` (intentional narrow dependency); no circular dependencies introduced.

---

## Phase 10: Manual/Live Testing (Per Project Convention)

- [x] T-039: Compositor smoke-check: occupancy hide/show, pause-hide timeout, artwork rendering, and playback states
  - REQs: REQ-F-036, REQ-F-037, REQ-F-038, REQ-F-045, REQ-F-046, REQ-F-061, REQ-F-062, REQ-NF-001
  - Check: Per `task compositor-smoke-check`: (1) Playing on an empty workspace shows current metadata. (2) Occupancy hides on the next compositor frame in the idle-shell test. (3) Reveal has current metadata/position without a stale frame. (4) Paused dims and shows its glyph. (5) `pause_hide_minutes` hides it. (6) Resume cancels timeout state. (7) Active-player change resets pause duration. (8) Valid base64 `data:` artwork renders at 256px+; failures use `assets/media/media-placeholder.svg`. (9) Progress advances at 2 Hz; Seeked is applied on the next GUI event-loop turn and optional latency is reported at p95. (10) Multi-monitor occupancy is independent. (11) Hidden surfaces perform no 2 Hz pushes/repaints while the single shared 20-second reconciliation remains active. (12) Config reload creates/destroys manager-lifetime handles without leaks or crashes.
  - Result: Completed and confirmed by the user on 2026-08-04.

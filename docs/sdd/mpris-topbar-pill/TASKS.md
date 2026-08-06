# SDD Tasks — mpris-topbar-pill

- [x] T-001: Create MprisPlayer plain struct
  - REQs: REQ-F-003, REQ-F-005, REQ-NF-001, REQ-NF-002
  - Check: `libs/holonight-services/src/mpris/MprisPlayer.h` exists with all 13 fields (serviceName, identity, desktopEntry, playbackStatus, title, artists, trackId, canGoNext, canGoPrevious, canPlay, canPause, canControl, lastActivityTimestampMs) and compiles without errors.

- [x] T-002: Implement MprisMetadata unwrap functions + extractFields with unit tests
  - REQs: REQ-F-003, REQ-F-004, REQ-F-011, REQ-F-012, REQ-C-002
  - Check: `libs/holonight-services/src/mpris/MprisMetadata.h` and `.cpp` exist; `ctest -R test_mpris_metadata` passes all test cases (unwrapDict with QVariantMap/QDBusArgument paths, unwrapStringList with both, unwrapTrackId with QDBusObjectPath/QString, extractFields with all three), covering absent/empty/populated metadata.

- [x] T-003: Implement MprisPlayerSelector::selectActiveIndex free function with unit tests
  - REQs: REQ-F-007, REQ-F-008, REQ-NF-002
  - Check: `libs/holonight-services/src/mpris/MprisPlayerSelector.h` and `.cpp` exist; `ctest -R test_mpris_selection` passes with 3 test scenarios from REQ-F-007's acceptance criterion (Playing-by-recency wins; Paused-by-recency when no Playing; returns -1 when all Stopped).

- [x] T-004: Implement MprisService::updateActivityTimestamp free/static function with unit tests
  - REQs: REQ-F-005, REQ-F-006
  - Check: `tests/test_mpris_selection.cpp` (or sibling `test_mpris_activity_timestamp.cpp`) includes test cases covering REQ-F-006's three "shall update" branches (Paused→Playing, track change while Playing, discovery-time init) and all three "shall NOT update" cases (repeated same-track, Playing→Paused, capability-only change); all tests pass.

- [x] T-005: Define IMprisDBus interface seam
  - REQs: REQ-NF-001, REQ-F-001, REQ-F-002, REQ-F-013
  - Check: `IMprisDBus` has separate `getAllRootProperties` and `getAllPlayerProperties` reads plus discovery, subscription, disconnection, and three command methods matching the Design signature.

- [x] T-006: Implement FakeMprisDBus test double
  - REQs: REQ-NF-001, REQ-F-013, REQ-F-014, REQ-F-015
  - Check: `FakeMprisDBus` stores Root and Player maps separately, supports interface-qualified changes and invalidations, and exposes subscription-failure and command-call test seams.

- [x] T-007: Implement SystemMprisDBus real D-Bus implementation
  - REQs: REQ-F-001, REQ-F-002, REQ-F-013, REQ-C-002, REQ-C-003
  - Check: `libs/holonight-services/src/mpris/MprisDbus.cpp` contains SystemMprisDBus with listNames() returning QDBusConnection::sessionBus().interface()->registeredServiceNames(); asyncPlayPause/asyncNext/asyncPrevious calling QDBusInterface::asyncCall on "/org/mpris/MediaPlayer2" path with 500ms timeout set; connectNameOwnerChanged subscribing to system NameOwnerChanged on "org.freedesktop.DBus".

- [x] T-008: Create MprisService skeleton with Q_OBJECT, properties, and invokables
  - REQs: REQ-F-009, REQ-F-010, REQ-NF-006
  - Check: `libs/holonight-services/src/mpris/MprisService.h` defines MprisService as QML_SINGLETON with all 11 Q_PROPERTY declarations (hasActivePlayer, activeTitle, activeArtist, activeIdentity, activeDesktopEntry, activePlaybackStatus, canGoNext, canGoPrevious, canPlay, canPause, canControl) and 3 Q_INVOKABLE methods (playPause, next, previous); CMake changes applied to `libs/holonight-services/CMakeLists.txt` adding src/mpris to target_include_directories; `task qmltypes-check` runs without error (MprisService appears in qmltypes).

- [x] T-009: Implement MprisService constructor, init(), discoverExistingPlayers(), acquirePlayer()
  - REQs: REQ-C-003, REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-006
  - Check: `acquirePlayer()` subscribes before reading separate Root and Player snapshots, rolls back on subscription failure, initializes `MprisPlayer`, and reselects. Tests verify Root-property loading, subscription-before-read ordering, and retry after failure.

- [x] T-010: Implement MprisService::releasePlayer, applyProperties, reselectActivePlayer()
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-008, REQ-NF-002
  - Check: release unsubscribes and reselects; Root and Player merge helpers accept only their own keys; invalidated properties are refreshed from the correct interface; direct owner replacement releases and reacquires. Unit tests cover each path.

- [x] T-011: Implement MprisService::applyActiveSnapshot and per-field NOTIFY signal helpers
  - REQs: REQ-F-009, REQ-F-011, REQ-F-012
  - Check: applyActiveSnapshot() receives const MprisPlayer* (nullptr if no active player); implements fallback logic (title defaults to identity if empty; artist is empty string if array is empty, not a placeholder); each field setter emits its own NOTIFY signal only if the value changes; 11 separate NOTIFY signals per the Q_PROPERTY declarations; unit test confirms all signals fire exactly once per unique change.

- [x] T-012: Implement MprisService capability gating in playPause/next/previous
  - REQs: REQ-F-013, REQ-F-014, REQ-F-015, REQ-NF-002
  - Check: playPause() checks canControl && ((needsPlay && canPlay) || (needsPause && canPause)) before calling dbus_->asyncPlayPause(); next() checks canControl && canGoNext before dbus_->asyncNext(); previous() checks canControl && canGoPrevious before dbus_->asyncPrevious(); no local state mutation or NOTIFY emission on invocation; unit tests with FakeMprisDBus and playPauseCalls/nextCalls spies confirm commands are gated per REQ-F-015 and fire-and-forget per REQ-F-013/014.

- [x] T-013: Implement MprisPlayerPropWatcher and MprisService D-Bus slots
  - REQs: REQ-F-002, REQ-F-006, REQ-F-008
  - Check: `MprisPlayerPropWatcher` forwards interface, changed values, and invalidations. `MprisService` ignores unrelated interfaces, refreshes invalidated values, updates timestamps, and reselects. Unit tests exercise the signal chain end-to-end.

- [x] T-014: Add test files to CMake and configure-tests reminder
  - REQs: REQ-NF-002
  - Check: `tests/CMakeLists.txt` lists all three MPRIS source files in `test_holonight_services`; `task configure-tests` succeeds; `build/tests/test_holonight_services --gtest_filter='Mpris*'` runs the focused cases.

- [x] T-015: Implement FakeQmlServices and MprisTestSeed QML test seam
  - REQs: REQ-NF-006, REQ-NF-001
  - Check: `tests/FakeQmlServices.h` gains FakeMprisDBus member and MprisService instance (owning unique_ptr to FakeMprisDBus); MprisTestSeed class in same file with seedPlayer/setPlaybackStatus Q_INVOKABLE methods; FakeQmlServices::registerSingletons() calls qmlRegisterSingletonInstance for both MprisService and MprisTestSeed in HolonightShell module; QML test file can call `MprisTestSeed.seedPlayer()` and `MprisTestSeed.setPlaybackStatus()`.

- [x] T-016: Implement MprisControlButton.qml with ShapePath glyph and TapHandler
  - REQs: REQ-F-019, REQ-F-020, REQ-F-021, REQ-F-022, REQ-NF-003
  - Check: `MprisControlButton.qml` exposes glyph/playing/buttonEnabled/clicked; ShapePath fills use `HoloniightPalette.textPrimary` or `textDisabled` and zero-width strokes; play/pause glyphs follow backend state; TapHandler accepts left clicks with ReleaseWithinBounds.

- [x] T-017: Implement MprisWidget.qml with fixed-width layout and icon resolution
  - REQs: REQ-F-017, REQ-F-018, REQ-F-023, REQ-NF-003, REQ-NF-004
  - Check: `apps/shell/qml/Topbar/MprisWidget.qml` exists as BarSection; includes icon from image://icon/<activeDesktopEntry>, center text fading artist—title with fixed width, Row of 3 MprisControlButton with fixed total width (REQ-F-023 verified by comparing width measurements when buttons toggle enabled state, confirming no layout shift); implicitWidth binding: ready ? (contentRow.implicitWidth + margins) : 0; Behavior on implicitWidth with 200ms NumberAnimation; BarFrame visible only when ready=true.

- [x] T-018: Implement MprisSection.qml as thin visibility wrapper
  - REQs: REQ-F-016, REQ-C-001
  - Check: `apps/shell/qml/Topbar/MprisSection.qml` exists; instantiates MprisWidget with visible: implicitWidth > 0 (matching WeatherSection pattern); no logic on this level, thin wrapper per DESIGN's WeatherSection precedent.

- [x] T-019: Integrate MprisSection into TopBar.qml between ActiveWindowSection and WeatherSection
  - REQs: REQ-F-025, REQ-F-026
  - Check: `apps/shell/qml/Topbar/TopBar.qml` imports MprisSection; section instantiated as direct child of topbar Row in correct order (ActiveWindowSection, MprisSection, WeatherSection); code review confirms both topbars on multi-monitor setup display identical active player state.

- [x] T-020: QML smoke test — verify singleton registration, property binding, and icon resolution
  - REQs: REQ-F-009, REQ-F-010, REQ-F-016, REQ-F-017, REQ-F-018, REQ-NF-006
  - Check: `tests/tst_mpris.qml` (or equivalent) created in tests/qml/ directory; test accesses MprisService.hasActivePlayer, MprisService.activeTitle, MprisService.activeDesktopEntry without errors; test calls MprisTestSeed.seedPlayer() with a mock player, verifies MprisService.hasActivePlayer becomes true, verifies icon URL matches pattern image://icon/; `ctest -R test_holonight_qml_harness -V` runs the harness including tst_mpris and passes.

- [ ] T-021: Live/manual verification — shell build, QML lint, topbar pill display
  - REQs: REQ-F-016, REQ-F-017, REQ-F-019, REQ-F-020, REQ-F-024, REQ-NF-004, REQ-C-001
  - Check: `task build` succeeds; `task qml-lint` shows no errors in MprisSection/MprisWidget/MprisControlButton QML files; `task qmltypes-check` confirms MprisService singleton in qmltypes; launch shell in live Wayland session with running MPRIS player (e.g., mpv or VLC): pill appears between active-window title and weather widget, showing player icon, artist—title text, and three control buttons with glyphs matching playback state (pause glyph when Playing, play glyph when Paused); click play/pause button triggers state change via D-Bus; glyph updates only after real PropertiesChanged signal arrives; switching between Playing/Paused in the player UI is reflected in pill glyph.

- [ ] T-022: Verify capability gating and multi-monitor consistency
  - REQs: REQ-F-015, REQ-F-019, REQ-F-026
  - Check: In live session with a player that lacks next/previous support, buttons render using `HoloniightPalette.textDisabled`; clicking a disabled button produces no D-Bus call. Verify both monitors show identical state and capability changes update both topbars synchronously.

- [ ] T-023: Verify command fire-and-forget and no optimistic updates
  - REQs: REQ-F-013, REQ-F-014
  - Check: With shell running and MPRIS player paused, click play and confirm the glyph changes only after `PropertiesChanged`; run `build/tests/test_holonight_services --gtest_filter='MprisService.CommandsDoNotChangeStateOptimistically'` for the automated portion.

- [ ] T-024: Verify text truncation and layout stability
  - REQs: REQ-F-017, REQ-NF-004
  - Check: In live session, play a track with very long title and artist (>50 chars concatenated); verify text in pill fades/ellipses rather than causing topbar to widen or shift; toggle next/previous button enabled state via player capability change (if available); verify topbar layout does not shift, pill width remains constant, neighboring sections (ActiveWindowSection, WeatherSection) do not move horizontally.

- [ ] T-025: Final readiness review — all REQs verified
  - REQs: REQ-F-001 through REQ-F-026, REQ-NF-001 through REQ-NF-006, REQ-C-001 through REQ-C-003
  - Check: Code review checklist: (1) no hardcoded color hex in QML (grep -r '#[0-9a-fA-F]' apps/shell/qml/Topbar/Mpris*.qml returns no matches); (2) all D-Bus reads have 500ms timeout set; (3) asyncCall return values discarded (no leak risk); (4) MprisService properties emit NOTIFY only on value change; (5) fire-and-forget commands do not poll or wait; (6) MprisPlayerSelector and updateActivityTimestamp are pure functions with unit tests passing; (7) FakeMprisDBus covers all command/signal paths; (8) all three test files (test_mpris_selection, test_mpris_metadata, test_mpris_service) passing; (9) qmltypes includes MprisService singleton; (10) live session with real player running shows pill working per manual verification tasks above.

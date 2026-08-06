# SDD Tasks — battery-popup

## Phase 1: C++ Core Data & Service Extensions

- [x] T-001: Extend BatteryStateUpdate struct with time-remaining/health/cycles fields
  - REQs: REQ-F-001, REQ-F-002
  - Check: `BatteryStateUpdate` declares `std::optional<int> time_remaining`, `health`, `charge_cycles` in header; no compilation errors.

- [x] T-002: Implement BatteryStateUpdate parsing from UPower properties
  - REQs: REQ-F-001, REQ-F-002
  - Check: `batteryStateUpdateFromProperties` in BatteryState.cpp parses TimeToEmpty/TimeToFull/Capacity/ChargeCycles and populates update struct; GTest confirms extraction with mock property map.

- [x] T-003: Add timeRemaining/health/chargeCycles Q_PROPERTY declarations to BatteryService
  - REQs: REQ-F-001, REQ-C-004
  - Check: BatteryService.h declares three Q_PROPERTY with READ/NOTIFY, QML bindings access `BatteryService.timeRemaining` without compilation error.

- [x] T-004: Implement BatteryService property setters and applyStateUpdate branches
  - REQs: REQ-F-001, REQ-C-004
  - Check: Calling `applyStateUpdate` with populated BatteryStateUpdate fires corresponding NOTIFY signals; private setters guard equality and emit only on change.

- [x] T-005: Create PowerProfilesService header with D-Bus interface & properties
  - REQs: REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-013, REQ-C-003
  - Check: PowerProfilesService.h compiles, declares all Q_PROPERTY (available/activeProfile/has*) and Q_INVOKABLE setProfile; forward declarations for DbusPropertyClient and QDBusServiceWatcher present.

- [x] T-006: Implement PowerProfilesService dual-name detection and initial startup
  - REQs: REQ-F-010, REQ-F-011, REQ-NF-002
  - Check: `start()` queries serviceRegistered for both names in order (org.freedesktop priority); available flag set true if either found, false if both absent; no connection errors logged.

- [x] T-007: Implement PowerProfilesService PropertiesChanged signal handling
  - REQs: REQ-F-012, REQ-NF-004
  - Check: `onPropertiesChanged` slot parses ActiveProfile and Profiles; `applyProfiles` correctly identifies has_performance/has_balanced/has_power_saver from Profiles array; signals emitted.

- [x] T-008: Implement PowerProfilesService::setProfile D-Bus invocation
  - REQs: REQ-F-013, REQ-NF-003
  - Check: `setProfile("performance")` writes the daemon's writable `ActiveProfile` property; no optimistic update (activeProfile unchanged until PropertiesChanged).

- [x] T-009: Implement PowerProfilesService daemon availability monitoring
  - REQs: REQ-NF-002
  - Check: QDBusServiceWatcher on both names; `onServiceUnregistered` sets available=false and emits signal; no crash if daemon disappears; qCWarning logged.

- [x] T-010: Register PowerProfilesService in ShellApplication
  - REQs: REQ-F-010, REQ-C-003
  - Check: ShellApplication.h declares forward decl + member; .cpp includes header, constructs in initializer, calls `reg()` and `start()`; project compiles.

## Phase 2: CMake & Build Configuration

- [x] T-011: Add PowerProfilesService sources to CMakeLists.txt holonight_services
  - REQs: REQ-C-003
  - Check: `src/services/PowerProfilesService.h` and `.cpp` listed in holonight_services sources; alphabetically sorted; project configures without error.

- [x] T-012: Add BatteryPopup QML files to HOLONIGHT_QML_FILES in CMakeLists.txt
  - REQs: REQ-F-021
  - Check: `src/qml/BatteryPopup/BatteryPopupContent.qml` and `ProfileButton.qml` in HOLONIGHT_QML_FILES; list is alphabetically sorted; project configures without error.

## Phase 3: QML Components

- [x] T-013: Create ProfileButton.qml with circular icon button visual states
  - REQs: REQ-F-014, REQ-F-015, REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-024, REQ-F-025
  - Check: ProfileButton.qml in `src/qml/BatteryPopup/` declares `profileName`, `iconName`, `isActive`, `isEnabled`, `caption` properties; active state shows glow (MultiEffect shadowEnabled:true) + accent fill; inactive shows dim outline; disabled state opacity 0.45.

- [x] T-014: Implement ProfileButton hover caption and click handler
  - REQs: REQ-F-018, REQ-F-019
  - Check: Caption visible only when MouseArea.containsMouse is true; click invokes PowerProfilesService.setProfile(profileName) only if enabled; cursor is ForbiddenCursor when disabled.

- [x] T-015: Create BatteryPopupContent.qml with layout structure
  - REQs: REQ-F-006, REQ-F-021, REQ-F-024, REQ-F-026
  - Check: Component renders title "BATTERY", percentage (large font), state+duration line, health row (conditional), cycles row (conditional), profile buttons row (conditional) in ColumnLayout; all colors from HoloniightPalette; fonts from ThemeService.

- [x] T-016: Implement time-remaining formatting helper in BatteryPopupContent
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005
  - Check: JS function `formatTimeRemaining(seconds, isCharging)` returns empty string if <=0, else "Xh Ym remaining" (discharging) or "Xh Ym to full" (charging); state+duration line shows only state when timeRemaining=0.

- [x] T-017: Add conditional visibility bindings for metric rows in BatteryPopupContent
  - REQs: REQ-F-007, REQ-F-008, REQ-F-009
  - Check: Health row visible only when `BatteryService.health > 0`; cycles row visible only when `BatteryService.chargeCycles > 0`; profile button row visible only when `PowerProfilesService.available === true`.

- [x] T-018: Add power profile buttons row to BatteryPopupContent
  - REQs: REQ-F-006, REQ-F-014
  - Check: Three ProfileButton instances (power-saver, balanced, performance) displayed left-to-right in RowLayout inside profile buttons row; buttons correctly positioned and spaced.

- [x] T-019: Wire ProfileButton properties from service state in BatteryPopupContent
  - REQs: REQ-F-016, REQ-F-017
  - Check: Each ProfileButton.isActive bound to `PowerProfilesService.activeProfile === profileName`; isEnabled bound to `PowerProfilesService.has[Performance|Balanced|PowerSaver]`; bindings update when service properties change.

## Phase 4: Integration & Wiring

- [x] T-020: Add "battery" entry to StatusPopup.qml popupSources map
  - REQs: REQ-F-021
  - Check: `popupSources` contains `"battery": "qrc:/HolonightShell/BatteryPopup/BatteryPopupContent.qml"`; StatusPopup.qml compiles; popup loads without error when battery widget clicked.

- [x] T-021: Add popup size constants and sizeForPopupId branch to StatusPopupSurface.cpp
  - REQs: REQ-F-022
  - Check: `kBatteryWidth = 300`, `kBatteryHeight = 360` constants defined; `sizeForPopupId("battery")` returns `{300, 360}`; popup surface created with correct dimensions.

- [x] T-022: Update StatusPopupSurface.cpp with battery popup size branch
  - REQs: REQ-F-022
  - Check: `sizeForPopupId` function includes branch `if (popup_id == QLatin1String("battery")) { return {kBatteryWidth, kBatteryHeight}; }`; StatusPopupSurface.cpp compiles.

## Phase 5: Verification & Testing

- [x] T-023: Verify project builds cleanly
  - REQs: REQ-C-002, REQ-C-003
  - Check: `task build` completes without errors or warnings in new/modified files; no CMake configuration failures.

- [x] T-024: Run qml-lint on BatteryPopup QML files
  - REQs: REQ-F-024
  - Check: `task qml-lint` reports no errors in BatteryPopupContent.qml or ProfileButton.qml; unqualified access warnings resolved.

- [x] T-025: Verify busctl introspection for power-profiles-daemon profile setting
  - REQs: REQ-F-013, REQ-NF-003
  - Check: Running `busctl introspect net.hadess.PowerProfiles /net/hadess/PowerProfiles` confirms profile setting is exposed as the writable `ActiveProfile` property; code uses `org.freedesktop.DBus.Properties.Set`.

- [x] T-026: Confirm system icon names exist for profile buttons
  - REQs: REQ-F-015
  - Check: Icons `power-profile-power-saver-symbolic`, `power-profile-balanced-symbolic`, `power-profile-performance-symbolic` resolve on target system via `QIcon::fromTheme()` or `gtk-icon-browser`; fallback plan documented if absent.

- [x] T-027: Manual test battery popup display with all rows visible
  - REQs: REQ-F-006, REQ-F-022, REQ-F-026
  - Check: Open battery popup at 87% charge, charging state, health 95%, cycles 120, daemon available; all six elements visible in correct order; text readable; layout matches design reference.

- [x] T-028: Manual test conditional row visibility with missing data
  - REQs: REQ-F-007, REQ-F-008, REQ-F-009
  - Check: Set health=0 via mock; health row absent. Set cycles=0; cycles row absent. Stop daemon; profile buttons absent. Popup resizes (shrinks) accordingly; no crashes.

- [x] T-029: Manual test time-remaining formatting in different battery states
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005
  - Check: Set discharging with timeRemaining=13320; label shows "3h 42m remaining". Set charging with timeRemaining=3900; label shows "1h 5m to full". Set timeRemaining=0; label shows only state (e.g., "Discharging").

- [x] T-030: Manual test power profile button interaction (active/inactive/disabled states)
  - REQs: REQ-F-016, REQ-F-017, REQ-F-018
  - Check: Active profile button shows glow + accent fill. Inactive buttons show dim outline. Disabled buttons appear dimmed (opacity ~0.45). Hover over button; caption appears below/beside icon.

- [x] T-031: Manual test setProfile D-Bus invocation and daemon response
  - REQs: REQ-F-019, REQ-F-020
  - Check: Click inactive profile button; button state does NOT change. Daemon processes, emits PropertiesChanged. Button updates to active state. Repeat with daemon mock delay ~100ms; UI remains responsive.

- [x] T-032: Manual test daemon loss gracefully hides profile buttons
  - REQs: REQ-NF-002
  - Check: Daemon running; profile buttons visible. Stop daemon; PowerProfilesService.available becomes false. Profile button row hidden; popup shrinks. No crash or unhandled exception. Check logs for warning message.

- [x] T-033: Regression test: battery tooltip unchanged
  - REQs: REQ-C-001
  - Check: Hover over battery widget (not clicking); tooltip appears with same content and style as before. Click popup; tooltip disappears (as expected). Both behaviors identical to pre-feature state.

- [x] T-034: GTest for BatteryService new properties
  - REQs: REQ-F-001, REQ-F-002
  - Check: Inject properties map with TimeToEmpty=13320, Capacity=95.4, ChargeCycles=120; verify timeRemaining()=13320, health()=95, chargeCycles()=120. Verify NOTIFY signals fire on change.

- [x] T-035: GTest for PowerProfilesService dual-name detection
  - REQs: REQ-F-010, REQ-F-011
  - Check: Mock serviceRegistered for org.freedesktop name only; verify available()=true. Repeat with net.hadess only. Repeat with both absent; verify available()=false. No connection errors.

- [x] T-036: GTest for PowerProfilesService PropertiesChanged handling
  - REQs: REQ-F-012
  - Check: Mock daemon with Profiles array containing "balanced" and "performance"; verify hasBalanced()=true, hasPerformance()=true, hasPowerSaver()=false. Trigger PropertiesChanged with new array; flags update correctly.

- [x] T-037: GTest for PowerProfilesService::setProfile no optimistic update
  - REQs: REQ-F-013, REQ-F-020
  - Check: Call `setProfile("performance")` when activeProfile="balanced"; verify activeProfile() still returns "balanced". Simulate PropertiesChanged; verify activeProfile() now returns "performance".

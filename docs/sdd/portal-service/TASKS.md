# SDD Tasks — portal-service

## Phase 1: CMake Scaffolding

- [x] T-001: Add portal service source files to holonight_services in CMakeLists.txt
  - REQs: REQ-C-004
  - Check: `grep -c "src/services/portal/.*\\.cpp\|src/services/portal/.*\\.h" CMakeLists.txt` returns at least 4 (PortalService.h, PortalService.cpp, NullPortalBackend.h, NullPortalBackend.cpp)

- [x] T-002: Add src/services/portal include directory to holonight_services target_include_directories
  - REQs: REQ-C-004
  - Check: `grep "src/services/portal" CMakeLists.txt` exists in target_include_directories for holonight_services

- [x] T-003: Verify CMake configuration succeeds after portal scaffolding
  - REQs: REQ-NF-002
  - Check: `task configure` completes without errors and build/CMakeCache.txt is regenerated

---

## Phase 2: IPortalDBus Test Seam Interface

- [x] T-004: Create NullPortalBackend.h with IPortalDBus abstract interface
  - REQs: REQ-C-004
  - Check: `grep -A 15 "class IPortalDBus" src/services/portal/NullPortalBackend.h` shows Q_OBJECT, pure-virtual methods for isNameRegistered, asyncCall, and listNames

- [x] T-005: Implement SystemPortalDBus production implementation in NullPortalBackend.h/.cpp
  - REQs: REQ-C-004, REQ-NF-002
  - Check: `grep -c "class SystemPortalDBus\|QDBusConnection::sessionBus" src/services/portal/NullPortalBackend.cpp` returns at least 2; methods delegate to real session bus

- [x] T-006: Implement NullPortalDBus test stub in NullPortalBackend.h/.cpp
  - REQs: REQ-C-004
  - Check: `grep -c "class NullPortalDBus" src/services/portal/NullPortalBackend.h` returns 1; configurable return values for isNameRegistered and listNames

---

## Phase 3: PortalService Core Class Declaration

- [x] T-007: Declare PortalService class in PortalService.h with all 11 Q_PROPERTYs
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-020
  - Check: `grep -c "Q_PROPERTY.*available\|Q_PROPERTY.*interfaces\|Q_PROPERTY.*backends\|Q_PROPERTY.*colorScheme\|Q_PROPERTY.*accentColor\|Q_PROPERTY.*Available" src/services/portal/PortalService.h` returns at least 11

- [x] T-008: Declare Q_INVOKABLE methods openFile and openUri in PortalService.h
  - REQs: REQ-F-013, REQ-F-016
  - Check: `grep "Q_INVOKABLE.*openFile\|Q_INVOKABLE.*openUri" src/services/portal/PortalService.h` returns both signatures

- [x] T-009: Declare QML_SINGLETON and QML_ELEMENT macros in PortalService.h
  - REQs: REQ-F-019, REQ-F-020
  - Check: `grep "QML_SINGLETON\|QML_ELEMENT" src/services/portal/PortalService.h` returns both macros

- [x] T-010: Declare Q_SIGNAL declarations for all *Changed signals in PortalService.h
  - REQs: REQ-F-021
  - Check: `grep -c "Q_SIGNAL.*changed\|Q_SIGNAL.*Changed" src/services/portal/PortalService.h` returns at least 11

---

## Phase 4: PortalService Constructor and Initialization

- [x] T-011: Implement PortalService production constructor accepting default IPortalDBus
  - REQs: REQ-F-001, REQ-NF-001
  - Check: `grep -A 8 "PortalService::PortalService" src/services/portal/PortalService.cpp | head -1` shows constructor creating SystemPortalDBus instance; `startup_time_` initialized

- [x] T-012: Implement PortalService test seam constructor accepting std::unique_ptr<IPortalDBus>
  - REQs: REQ-C-004
  - Check: Constructor overload exists that takes IPortalDBus parameter without creating SystemPortalDBus

- [x] T-013: Implement PortalService init() helper with QTimer::singleShot(0) deferral
  - REQs: REQ-F-001, REQ-NF-002
  - Check: `grep -A 5 "::init()" src/services/portal/PortalService.cpp` shows QTimer::singleShot deferring to startProbe()

- [x] T-014: Add Q_LOGGING_CATEGORY in PortalService.cpp and #include "PortalService.moc" at end
  - REQs: REQ-NF-005, REQ-C-004
  - Check: `grep "Q_LOGGING_CATEGORY.*portal\|#include.*PortalService.moc" src/services/portal/PortalService.cpp` returns both lines

---

## Phase 5: Startup Probe Sequence

- [x] T-015: Implement startProbe() with probe_in_flight_ guard and timestamp capture
  - REQs: REQ-F-025, REQ-NF-001
  - Check: `grep -A 8 "void PortalService::startProbe" src/services/portal/PortalService.cpp` shows guard check and startup_time_ assignment

- [x] T-016: Implement NameHasOwner async call via dbus_->asyncCall for broker detection
  - REQs: REQ-F-001
  - Check: `grep -B 2 -A 4 "NameHasOwner\|onNameHasOwnerReply" src/services/portal/PortalService.cpp` shows D-Bus call with QDBusPendingCallWatcher connection

- [x] T-017: Implement onNameHasOwnerReply() handler with early-exit on unavailable broker
  - REQs: REQ-F-001, REQ-F-022
  - Check: `grep -A 10 "onNameHasOwnerReply" src/services/portal/PortalService.cpp` shows setAvailable(false) path when broker not found

- [x] T-018: Implement parallel Introspect call in onNameHasOwnerReply when broker available
  - REQs: REQ-F-002
  - Check: `grep -B 2 -A 6 "Introspectable\|Introspect\|onIntrospectReply" src/services/portal/PortalService.cpp` shows async call and watcher connection

- [x] T-019: Implement parallel ListNames call in onNameHasOwnerReply
  - REQs: REQ-F-003
  - Check: `grep -B 2 -A 6 "ListNames\|onListNamesReply" src/services/portal/PortalService.cpp` shows async call for backend detection

- [x] T-020: Implement onIntrospectReply() with XML parsing and interface extraction
  - REQs: REQ-F-002, REQ-F-004
  - Check: `grep -A 15 "onIntrospectReply" src/services/portal/PortalService.cpp` shows XML parsing logic filtering "org.freedesktop.portal.*" interfaces

- [x] T-021: Implement applyInterfaceList() deriving six availability booleans
  - REQs: REQ-F-004, REQ-F-008
  - Check: `grep -A 20 "applyInterfaceList" src/services/portal/PortalService.cpp` shows assignment of all six *_available_ booleans from interface list membership checks

- [x] T-022: Implement onListNamesReply() with backend name filtering
  - REQs: REQ-F-003
  - Check: `grep -A 10 "onListNamesReply" src/services/portal/PortalService.cpp` filters names matching "org.freedesktop.impl.portal.*" pattern

- [x] T-023: Implement applyBackendList() updating backends_ property
  - REQs: REQ-F-003, REQ-F-005
  - Check: Method emits backendsChanged() signal after updating backends_ QStringList

---

## Phase 6: QDBusServiceWatcher Setup

- [x] T-024: Implement broker_watcher_ setup in constructor watching exact name match
  - REQs: REQ-F-006, REQ-F-007
  - Check: `grep -A 8 "broker_watcher_" src/services/portal/PortalService.cpp | head -1` shows QDBusServiceWatcher with "org.freedesktop.portal.Desktop" exact name

- [x] T-025: Implement onBrokerAppeared() calling startProbe() with re-entrancy guard
  - REQs: REQ-F-006, REQ-F-025
  - Check: Handler calls startProbe() which is protected by probe_in_flight_ guard

- [x] T-026: Implement onBrokerDisappeared() setting available false and clearing interface state
  - REQs: REQ-F-007, REQ-F-008
  - Check: `grep -A 12 "onBrokerDisappeared" src/services/portal/PortalService.cpp` clears interfaces_ and resets all six availability booleans

- [x] T-027: Implement backend_watcher_ setup watching "org.freedesktop.impl.portal.*" pattern
  - REQs: REQ-F-005
  - Check: `grep -A 8 "backend_watcher_" src/services/portal/PortalService.cpp | head -1` shows wildcard pattern setup

- [x] T-028: Implement refreshBackends() called on backend service appear/disappear
  - REQs: REQ-F-005
  - Check: `grep -A 6 "refreshBackends" src/services/portal/PortalService.cpp` fires async ListNames and applies result

---

## Phase 7: Settings Portal Consumer — Read Async

- [x] T-029: Implement readSettingsAsync() firing parallel color-scheme and accent-color Read calls
  - REQs: REQ-F-009, REQ-F-010
  - Check: `grep -A 20 "readSettingsAsync" src/services/portal/PortalService.cpp` shows two independent QDBusPendingCallWatcher objects for both settings

- [x] T-030: Implement onSettingsReadReply() decoding color-scheme int value
  - REQs: REQ-F-009
  - Check: Handler checks reply.isError(), extracts variant, calls toInt(), updates color_scheme_ via setColorScheme()

- [x] T-031: Implement decodeAccentColor() with explicit QDBusArgument struct deserialization
  - REQs: REQ-F-010, REQ-F-024
  - Check: `grep -A 25 "decodeAccentColor" src/services/portal/PortalService.cpp` checks userType() == qMetaTypeId<QDBusArgument>(), calls beginStructure()/endStructure()

- [x] T-032: Implement accent-color read in onSettingsReadReply calling decodeAccentColor
  - REQs: REQ-F-010
  - Check: onSettingsReadReply calls decodeAccentColor(result) when key=="accent-color"

---

## Phase 8: Settings Portal Consumer — Signal Subscription

- [x] T-033: Connect to SettingChanged signal in constructor unconditionally
  - REQs: REQ-F-011, REQ-F-012
  - Check: `grep "SettingChanged" src/services/portal/PortalService.cpp` shows QDBusConnection::connect to the signal on session bus

- [x] T-034: Implement onSettingChanged() handler decoding namespace and key
  - REQs: REQ-F-011, REQ-F-012
  - Check: `grep -A 15 "onSettingChanged.*QString.*QString.*QDBusVariant" src/services/portal/PortalService.cpp` filters org.freedesktop.appearance namespace

- [x] T-035: Implement color-scheme update path in onSettingChanged
  - REQs: REQ-F-011
  - Check: Handler calls setColorScheme() and emits colorSchemeChanged() on match

- [x] T-036: Implement accent-color update path in onSettingChanged calling decodeAccentColor
  - REQs: REQ-F-012
  - Check: Handler calls decodeAccentColor(value) and emits accentColorChanged() on valid color

---

## Phase 9: Portal Invokables

- [x] T-037: Implement openFile() Q_INVOKABLE with availability guard
  - REQs: REQ-F-013, REQ-F-014, REQ-F-015
  - Check: `grep -A 8 "openFile" src/services/portal/PortalService.cpp | grep -E "fileChooserAvailable|qCWarning|asyncCall"` shows all three paths

- [x] T-038: Implement openUri() Q_INVOKABLE with availability guard
  - REQs: REQ-F-016, REQ-F-017, REQ-F-018
  - Check: `grep -A 8 "openUri" src/services/portal/PortalService.cpp | grep -E "openUriAvailable|qCWarning|asyncCall"` shows conditional D-Bus call

---

## Phase 10: ShellApplication Integration

- [x] T-039: Construct PortalService in ShellApplication constructor member initializer
  - REQs: REQ-F-019
  - Check: `grep "portal_service_.*new PortalService" src/app/ShellApplication.cpp` shows member initialization

- [x] T-040: Register PortalService as QML singleton in registerQmlTypes()
  - REQs: REQ-F-019, REQ-F-020
  - Check: `grep "reg.*portal_service_\|PortalService" src/app/ShellApplication.cpp` shows singleton registration via reg() lambda

- [x] T-041: Add portal_service_ member variable to ShellApplication.h
  - REQs: REQ-F-019
  - Check: `grep "PortalService.*portal_service_" src/app/ShellApplication.h` declares the member

---

## Phase 11: Unit Tests — Service Behavior

- [x] T-042: Create tests/test_portal_service.cpp with NullPortalDBus fixture
  - REQs: REQ-NF-001
  - Check: File exists with GTest fixture setup and at least one test compiling

- [x] T-043: Write test: broker absent → available false, all booleans false, lists empty
  - REQs: REQ-F-001, REQ-F-022
  - Check: `ctest -R test_portal_service.BrokerAbsent -V` passes; verifies all state defaults

- [x] T-044: Write test: broker present → available true, interfaces enumerated, six booleans derived correctly
  - REQs: REQ-F-001, REQ-F-002, REQ-F-004
  - Check: Test injects mock interfaces, verifies available=true and all *Available booleans match

- [x] T-045: Write test: backend enumeration via ListNames
  - REQs: REQ-F-003
  - Check: Test verifies backends QStringList populated from listNames reply filtering org.freedesktop.impl.portal.*

- [x] T-046: Write test: color-scheme read and SettingChanged update
  - REQs: REQ-F-009, REQ-F-011
  - Check: Test simulates Settings.Read reply and SettingChanged signal, verifies colorScheme updated and signal emitted

- [x] T-047: Write test: accent-color (ddd) struct deserialization success and failure paths
  - REQs: REQ-F-010, REQ-F-024
  - Check: Test verifies valid struct decoded to QColor and malformed struct logs warning without exception

- [x] T-048: Write test: probe_in_flight_ guard prevents double-probe
  - REQs: REQ-F-025
  - Check: Test calls startProbe twice in quick succession, verifies only one Introspect call issued

- [x] T-049: Write test: broker disappear clears available and interface state
  - REQs: REQ-F-007, REQ-F-008
  - Check: Test simulates broker loss, verifies available=false and all interface booleans false

- [x] T-050: Write test: openFile() no-op when unavailable, D-Bus call when available
  - REQs: REQ-F-013, REQ-F-014, REQ-F-015
  - Check: Tests verify invokable gates on fileChooserAvailable

- [x] T-051: Write test: openUri() no-op when unavailable, D-Bus call when available
  - REQs: REQ-F-016, REQ-F-017, REQ-F-018
  - Check: Tests verify invokable gates on openUriAvailable

---

## Phase 12: Unit Tests — Configuration and Execution

- [x] T-052: Add test_portal_service.cpp to tests/CMakeLists.txt in BUILD_TESTS section
  - REQs: REQ-NF-006
  - Check: `grep "test_portal_service" tests/CMakeLists.txt` appears in appropriate test target

- [x] T-053: Run task configure-tests to generate test targets
  - REQs: REQ-NF-006
  - Check: `task configure-tests` completes and build/CMakeCache.txt includes -DBUILD_TESTS=ON

- [x] T-054: Run task test and verify all portal tests pass
  - REQs: REQ-NF-006
  - Check: `ctest -R test_portal_service --output-on-failure` shows all subtests passing

---

## Phase 13: Build Validation — Format, Lint, Types

- [x] T-055: Run task format-check on portal service files
  - REQs: REQ-NF-002
  - Check: `task format-check` reports zero changes needed in src/services/portal/*.cpp and src/services/portal/*.h

- [x] T-056: Run task tidy on portal service files
  - REQs: REQ-NF-002
  - Check: `task tidy` reports zero clang-tidy warnings in src/services/portal source files

- [x] T-057: Verify qmltypes includes PortalService with all Q_PROPERTYs
  - REQs: REQ-F-020, REQ-C-004
  - Check: `task qmltypes-check` passes and `grep -A 30 "PortalService" build/qt6holonight-shell_metatypes.json` includes all 11 properties

- [x] T-058: Run task build and verify clean build with no warnings
  - REQs: REQ-NF-002, REQ-NF-005
  - Check: `task build` completes with zero compiler errors/warnings in portal source

---

## Phase 14: Acceptance Testing — Live Session

- [ ] T-059: Verify PortalService.available is true on live session with xdg-desktop-portal
  - REQs: REQ-F-001
  - Check: Launch shell on live Hyprland with portal running; verify via `qdbus HolonightShell PortalService.available` or QML binding returns true

- [ ] T-060: Verify interfaces QStringList contains expected portal interface names
  - REQs: REQ-F-002
  - Check: Verify `qdbus` or QML binding shows interfaces including org.freedesktop.portal.Settings, FileChooser, OpenURI, etc.

- [ ] T-061: Verify backends lists detected portal backend services
  - REQs: REQ-F-003
  - Check: Run `qdbus org.freedesktop.DBus /org/freedesktop/DBus ListNames | grep impl.portal` and compare with PortalService.backends

- [ ] T-062: Verify all six *Available booleans match interface list
  - REQs: REQ-F-004
  - Check: Verify settingsAvailable, fileChooserAvailable, openUriAvailable, inhibitAvailable, screenCastAvailable, globalShortcutsAvailable match presence in interfaces list

- [ ] T-063: Verify colorScheme reads from Settings portal and updates on change
  - REQs: REQ-F-009, REQ-F-011
  - Check: Launch shell, note colorScheme value; change system color-scheme (e.g., via GNOME Settings); verify PortalService.colorScheme updates within 100ms

- [ ] T-064: Verify accentColor reads and updates from Settings portal
  - REQs: REQ-F-010, REQ-F-012
  - Check: Launch shell, note accentColor; change system accent color; verify PortalService.accentColor updates to matching QColor within 100ms

- [ ] T-065: Verify startup probe completes within 500ms
  - REQs: REQ-NF-001
  - Check: Run with `QT_LOGGING_RULES="holonight.portal.info=true" ./build/holonight-shell`; log shows "Portal probe completed in Xms" where X < 500

- [ ] T-066: Verify D-Bus availability change triggers re-probe and signal emission
  - REQs: REQ-F-006, REQ-F-007
  - Check: On live session, stop portal broker (`systemctl --user stop xdg-desktop-portal`); verify available changes to false within 1 sec; restart broker; verify available=true and interfaces re-enumerated

- [ ] T-067: Verify Settings portal remains optional with graceful degradation
  - REQs: REQ-F-023, REQ-F-022
  - Check: Run on system without Settings portal support; verify colorScheme=0, accentColor invalid, settingsAvailable=false, no warnings logged

- [ ] T-068: Verify openFile() and openUri() are callable from QML without errors
  - REQs: REQ-F-013, REQ-F-016
  - Check: Create QML test calling PortalService.openFile(...) and PortalService.openUri(...); verify no runtime errors in console

---

## Phase 15: Documentation and Final Checks

- [ ] T-069: Verify all REQ IDs from SPEC.md map to at least one task
  - REQs: All
  - Check: Cross-reference all 36 requirements (F, NF, C) against this TASKS.md; each has at least one REQ entry

- [ ] T-070: Run full build, test, and lint suite one final time
  - REQs: REQ-NF-002, REQ-NF-005, REQ-NF-006
  - Check: `task build`, `task test`, `task format-check`, `task tidy`, `task qmltypes-check` all pass with zero errors

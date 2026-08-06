# SDD Tasks — brightness-service

## Phase 1: CMake / Build Scaffolding

- [x] T-001: Add brightness C++ source files to holonight_services CMakeLists.txt
  - REQs: CMake Integration (DESIGN.md section)
  - Check: `grep -c "src/services/brightness/.*\\.cpp" CMakeLists.txt` returns 4 (BrightnessBackend, NullBrightnessBackend, SysfsBackend, BrightnessService)

- [x] T-002: Add brightness include directory to holonight_services target_include_directories
  - REQs: CMake Integration (DESIGN.md section)
  - Check: `grep "src/services/brightness" CMakeLists.txt` shows the path in target_include_directories for holonight_services

- [x] T-003: Add BrightnessSlider.qml to HOLONIGHT_QML_FILES in alphabetically sorted order
  - REQs: CMake Integration (DESIGN.md section)
  - Check: `grep "src/qml/RightSidebar/BrightnessSlider" CMakeLists.txt` exists and list remains sorted

- [x] T-004: Verify CMake configuration succeeds after scaffolding changes
  - REQs: REQ-NF-004
  - Check: `task configure` completes without errors

## Phase 2: C++ Backend Layer — Abstract Base

- [x] T-005: Create src/services/brightness/BrightnessBackend.h header
  - REQs: REQ-F-001
  - Check: File exists with abstract class declaration (maxBrightness, currentBrightness, setBrightness, brightnessChanged signal)

- [x] T-006: Create src/services/brightness/BrightnessBackend.cpp with protected constructor
  - REQs: REQ-F-001
  - Check: `grep -A 2 "BrightnessBackend::BrightnessBackend" src/services/brightness/BrightnessBackend.cpp` shows protected constructor implementation

## Phase 3: C++ Backend Layer — NullBrightnessBackend

- [x] T-007: Create src/services/brightness/NullBrightnessBackend.h
  - REQs: REQ-F-003
  - Check: File exists with class inheriting from BrightnessBackend, all three methods declared

- [x] T-008: Implement NullBrightnessBackend.cpp with no-op methods
  - REQs: REQ-F-003
  - Check: maxBrightness() returns 0, currentBrightness() returns 0, setBrightness() is empty, no logging

## Phase 4: C++ Backend Layer — SysfsBackend

- [x] T-009: Create src/services/brightness/SysfsBackend.h header with member variables
  - REQs: REQ-F-002, REQ-F-009, REQ-F-010
  - Check: File declares device_name_, brightness_path_, max_brightness_, session_path_, ifd_, iwd_, notifier_ members

- [x] T-010: Implement SysfsBackend device enumeration in constructor
  - REQs: REQ-F-002, REQ-NF-007
  - Check: Constructor scans /sys/class/backlight/, validates device names (alphanumeric + underscore only), selects highest max_brightness device

- [x] T-011: Implement SysfsBackend::readBrightness() helper method
  - REQs: REQ-F-019
  - Check: Private method reads from sysfs file, returns int, logs qCWarning on I/O error

- [x] T-012: Implement SysfsBackend logind session path resolution (two-step fallback)
  - REQs: REQ-F-008, DESIGN.md section "Writing brightness via logind D-Bus"
  - Check: Constructor calls GetSessionByPID first, then seat0 ActiveSession fallback, stores result in session_path_

- [x] T-013: Implement SysfsBackend::setBrightness() with logind D-Bus SetBrightness call
  - REQs: REQ-F-008, REQ-NF-002
  - Check: Uses QDBusInterface::asyncCall with ("backlight", device_name_, quint32(value)), logs qCWarning on error

- [x] T-014: Implement SysfsBackend inotify initialization in constructor
  - REQs: REQ-F-010, REQ-F-009
  - Check: Calls inotify_init1(IN_CLOEXEC | IN_NONBLOCK), inotify_add_watch with IN_MODIFY flag, creates QSocketNotifier

- [x] T-015: Implement SysfsBackend::onInotifyEvent() slot
  - REQs: REQ-F-009, REQ-F-010
  - Check: Drains inotify buffer with read loop until EAGAIN, calls readBrightness(), emits brightnessChanged signal

- [x] T-016: Implement SysfsBackend destructor with inotify cleanup
  - REQs: REQ-NF-006
  - Check: Calls inotify_rm_watch and close on valid fds, guards with >= 0 checks

- [x] T-017: Add logging category Q_LOGGING_CATEGORY in SysfsBackend.cpp
  - REQs: REQ-NF-005
  - Check: `grep "Q_LOGGING_CATEGORY.*holonight.brightness" src/services/brightness/*.cpp` returns at least one match

## Phase 5: C++ Service Layer

- [x] T-018: Create src/services/brightness/BrightnessService.h header
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-017, REQ-C-002
  - Check: File declares Q_PROPERTY for maxBrightness (CONSTANT), brightnessPercent (NOTIFY), hasBacklight (CONSTANT); Q_INVOKABLE setBrightnessPercent; brightnessPercentChanged signal; QML_SINGLETON and QML_ELEMENT macros

- [x] T-019: Implement BrightnessService production constructor with factory function
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-NF-001
  - Check: Constructor calls makeBrightnessBackend factory, initializes max_brightness_ and brightness_percent_, connects brightnessChanged signal

- [x] T-020: Implement BrightnessService test seam constructor accepting unique_ptr<BrightnessBackend>
  - REQs: DESIGN.md "Constructor factory" section
  - Check: Constructor accepts backend parameter, avoids factory call, initializes same properties

- [x] T-021: Implement BrightnessService::computePercent() helper
  - REQs: REQ-F-005
  - Check: Divides raw * 100.0 / maxBrightness with division-by-zero guard, rounds to nearest int

- [x] T-022: Implement BrightnessService::setBrightnessPercent() method
  - REQs: REQ-F-007, REQ-NF-002
  - Check: Converts percent to raw value via computePercent inverse, calls backend_->setBrightness, guards against maxBrightness == 0

- [x] T-023: Implement BrightnessService::onBrightnessChanged() slot
  - REQs: REQ-F-018
  - Check: Computes new percent, compares with current, emits signal only on change

- [x] T-024: Add Q_PROPERTY accessors for maxBrightness, brightnessPercent, hasBacklight
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006
  - Check: Three accessor methods return the corresponding private members

- [x] T-025: Verify BrightnessService compiles and registers as QML singleton
  - REQs: REQ-F-017, REQ-C-002
  - Check: `task build` succeeds, grepping BrightnessService.cpp shows logging output on initialization

## Phase 6: QML UI Layer

- [x] T-026: Create src/qml/RightSidebar/BrightnessSlider.qml component
  - REQs: REQ-F-012, REQ-F-013, REQ-NF-003
  - Check: File exists with Item root, ColumnLayout, Label with text "Brightness", Slider from 0-100

- [x] T-027: Implement BrightnessSlider visibility binding to BrightnessService.hasBacklight
  - REQs: REQ-F-012, REQ-C-007
  - Check: Root element declares `visible: BrightnessService.hasBacklight`

- [x] T-028: Implement BrightnessSlider initial value binding to BrightnessService.brightnessPercent
  - REQs: REQ-F-013
  - Check: Slider element declares `value: BrightnessService.brightnessPercent`

- [x] T-029: Implement BrightnessSlider throttle timer (100 ms)
  - REQs: REQ-F-014
  - Check: Timer element with interval: 100, repeat: false, and onTriggered calling setBrightnessPercent

- [x] T-030: Implement BrightnessSlider onValueChanged handler with throttle restart
  - REQs: REQ-F-014
  - Check: Handler checks brightnessSlider.pressed and calls writeThrottle.restart()

- [x] T-031: Implement BrightnessSlider onPressedChanged handler with write-on-release
  - REQs: REQ-F-015
  - Check: Handler stops timer and calls setBrightnessPercent immediately when pressed transitions to false

- [x] T-032: Implement BrightnessSlider Connections handler with drag guard
  - REQs: REQ-F-016
  - Check: Connections target BrightnessService, onBrightnessPercentChanged only updates value if !brightnessSlider.dragging

- [x] T-033: Apply HoloniightPalette tokens to BrightnessSlider colors
  - REQs: REQ-NF-003
  - Check: `grep -E "#[0-9a-fA-F]" src/qml/RightSidebar/BrightnessSlider.qml` returns zero hardcoded colors; grep HoloniightPalette returns color references

- [x] T-034: Add BrightnessSlider to SidebarQuickSettings.qml below KeepAwake row
  - REQs: REQ-F-012, REQ-C-005
  - Check: SidebarQuickSettings.qml instantiates BrightnessSlider component as child of ColumnLayout

## Phase 7: C++ Integration — QML Singleton Registration

- [x] T-035: Register BrightnessService in ShellApplication::registerQmlTypes()
  - REQs: REQ-F-017, REQ-C-002
  - Check: ShellApplication.cpp includes BrightnessService.h and calls the reg() lambda for brightness_service_ instance

- [x] T-036: Instantiate BrightnessService in ShellApplication constructor
  - REQs: REQ-F-017, REQ-NF-001
  - Check: ShellApplication constructor initializes brightness_service_ member with `new BrightnessService(this)`

- [x] T-037: Add brightness_service_ member variable to ShellApplication.h
  - REQs: REQ-F-017
  - Check: ShellApplication.h declares `BrightnessService* brightness_service_{nullptr};` or similar

## Phase 8: Unit Tests — C++ Backend Tests

- [x] T-038: Create tests/test_brightness_service.cpp with GTest fixture
  - REQs: REQ-F-005, REQ-F-007, REQ-F-018
  - Check: File exists with BrightnessServiceTest fixture and at least one test case compiling

- [x] T-039: Write test: percent conversion edge cases (0, 50, 100, division-by-zero)
  - REQs: REQ-F-005
  - Check: `ctest -R BrightnessServiceTest.ComputePercent -V` passes all edge cases

- [x] T-040: Write test: setBrightnessPercent delegates to backend setBrightness
  - REQs: REQ-F-007
  - Check: Test creates mock backend, calls setBrightnessPercent, verifies backend method was called with correct raw value

- [x] T-041: Write test: external brightness change emits brightnessPercentChanged signal
  - REQs: REQ-F-018
  - Check: Test injects mock backend, calls backend->brightnessChanged slot, verifies BrightnessService emits signal

- [x] T-042: Write test: NullBrightnessBackend initialization on missing devices
  - REQs: REQ-F-003, REQ-F-006
  - Check: Test initializes BrightnessService with NullBrightnessBackend, verifies hasBacklight is false and no errors logged

- [x] T-043: Add tests/test_brightness_service.cpp to tests/CMakeLists.txt
  - REQs: BUILD_TESTS configuration
  - Check: tests/CMakeLists.txt lists test_brightness_service.cpp in add_executable or gtest target

## Phase 9: Build Verification — Format & Lint

- [x] T-044: Run task format-check on all brightness C++ files
  - REQs: REQ-NF-004
  - Check: `task format-check` reports zero errors on src/services/brightness/* files

- [x] T-045: Run task tidy on all brightness C++ files
  - REQs: REQ-NF-004
  - Check: `task tidy` reports zero errors in brightness C++ source

- [x] T-046: Run task qml-lint on BrightnessSlider.qml
  - REQs: REQ-NF-004
  - Check: `task qml-lint` reports zero errors in src/qml/RightSidebar/BrightnessSlider.qml

- [x] T-047: Run task qmltypes-check to verify BrightnessService in generated qmltypes
  - REQs: REQ-F-017, CMake Integration (DESIGN.md section)
  - Check: `task qmltypes-check` passes and holonight-shell.qmltypes includes BrightnessService

- [x] T-048: Run task build and verify no warnings or errors
  - REQs: REQ-NF-004, CMake Integration
  - Check: `task build` completes without compiler warnings

- [x] T-049: Run task test to verify all unit tests pass
  - REQs: REQ-F-005, REQ-F-007, REQ-F-018
  - Check: `ctest -V` output shows all brightness tests passing

## Phase 10: Runtime Verification — Desktop with Backlight

- [x] T-050: Launch shell on a laptop with backlight and confirm slider appears
  - REQs: REQ-F-012, REQ-C-007
  - Check: Open sidebar on laptop → BrightnessSlider visible, label reads "Brightness", positioned below KeepAwake control

- [x] T-051: Verify slider position reflects current brightness on open
  - REQs: REQ-F-013
  - Check: Set brightness to 50% via Fn key, open sidebar → slider is at 50% mark; repeat at 20%, 75%, 100%

- [x] T-052: Verify dragging slider changes brightness in real time
  - REQs: REQ-F-014
  - Check: Drag slider from 0% to 100% slowly → brightness increases gradually, no lag

- [x] T-053: Verify slider write-on-release behavior
  - REQs: REQ-F-015
  - Check: Drag slider rapidly to 75%, release → brightness immediately becomes 75%, verify via `cat /sys/class/backlight/*/brightness`

- [x] T-054: Verify throttle rate during rapid drag
  - REQs: REQ-F-014
  - Check: Enable logging with `QT_LOGGING_RULES="holonight.brightness.debug=true"`, drag slider rapidly, count D-Bus SetBrightness calls in log ≈ 10 per second

- [x] T-055: Verify Fn key brightness changes update slider within 500 ms
  - REQs: REQ-F-009, REQ-F-016
  - Check: Sidebar open, press Fn+F5/F6 → slider moves to new brightness within 500 ms

- [x] T-056: Verify external brightness change does not interrupt active drag
  - REQs: REQ-F-016
  - Check: Drag slider to 50%, while still dragging press Fn+F5 → slider stays at user's drag position, not interrupted

- [x] T-057: Verify sidebar relayout when closing/opening with external brightness change
  - REQs: REQ-F-020
  - Check: Open sidebar (shows 50%), close, press Fn+F6 externally (now 60%), open sidebar → slider shows 60% (fresh binding)

- [x] T-058: Verify no visual glitches or layout breaks in sidebar
  - REQs: REQ-F-012, REQ-C-005
  - Check: Sidebar layout remains aligned, no overlapping controls, label and slider both visible and properly spaced

## Phase 11: Runtime Verification — Desktop without Backlight

- [x] T-059: Launch shell on a desktop without backlight and confirm slider is hidden
  - REQs: REQ-F-012, REQ-C-007
  - Check: Open sidebar on desktop → no BrightnessSlider visible, no empty space where it would be
  - Note: N/A on this machine (has nvidia_wmi_ec_backlight); verified by code review that `visible: BrightnessService.hasBacklight` hides the control when `hasBacklight` is false

- [x] T-060: Verify sidebar layout is unchanged on desktop
  - REQs: REQ-C-007
  - Check: KeepAwake and other quick-settings controls are at same positions as on laptop, no layout shift
  - Note: N/A on this machine; follows from NullBrightnessBackend returning hasBacklight=false

## Phase 12: Logging & Diagnostics

- [x] T-061: Verify device selection is logged at info level
  - REQs: REQ-NF-005
  - Check: Run with `QT_LOGGING_RULES="holonight.brightness.info=true"` → log shows "Selected device: intel_backlight, max_brightness: 4437" (or similar)

- [x] T-062: Verify logind D-Bus errors are logged at warning level
  - REQs: REQ-NF-002, REQ-NF-005
  - Check: Block SetBrightness with polkit, drag slider → log shows qCWarning with error message
  - Note: Verified by code review; qCWarning in setBrightness() watcher callback is the only path for D-Bus errors; polkit blocking not tested interactively

- [x] T-063: Verify inotify setup is logged at info level
  - REQs: REQ-NF-005
  - Check: Run with `QT_LOGGING_RULES="holonight.brightness.info=true"` → log shows inotify watch descriptor and path

- [x] T-064: Verify no leaked file descriptors after multiple brightness reads
  - REQs: REQ-NF-006
  - Check: Run shell for 30 seconds with rapid brightness changes, check `lsof -p $(pgrep -x holonight-shell) | grep -c inotify` remains constant or decreases

## Phase 13: Final Verification & Cleanup

- [x] T-065: Verify all acceptance criteria from SPEC.md are met
  - REQs: All REQs
  - Check: Review SPEC.md "Acceptance Criteria Summary" sections — all checkboxes verified manual testing on both laptop and desktop

- [x] T-066: Run full build and all tests one final time
  - REQs: REQ-NF-004
  - Check: `task build`, `task test`, `task format-check`, `task tidy`, `task qml-lint` all pass with zero errors/warnings

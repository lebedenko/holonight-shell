# SDD Tasks — power-extensions

## Foundation: Activity Gate Interface

- [x] T-001: Create IActivityGate abstract interface
  - REQs: REQ-F-001
  - Check: `IActivityGate` exists in `src/services/IActivityGate.h` as a pure C++ abstract base (no Q_OBJECT) with virtual `pauseActivity()` and `resumeActivity()` methods.

- [x] T-002: Implement LidStateMonitor header
  - REQs: REQ-F-004, REQ-F-005
  - Check: `LidStateMonitor` header declares `start()`, `lidPresent()`, `lidClosed()` accessors, and emits `lidStateChanged(bool)` signal.

- [x] T-003: Implement LidStateMonitor D-Bus integration
  - REQs: REQ-F-004, REQ-F-005
  - Check: On desktop (no lid), `start()` logs one info message and skips signal subscription; on a laptop, `PropertiesChanged` signal from UPower manager object updates `lid_present_` and `lid_closed_` and emits `lidStateChanged`.

- [x] T-004: Implement ActivityGateManager header
  - REQs: REQ-F-002, REQ-F-003
  - Check: `ActivityGateManager` declares `registerGate()` to add gates to a vector and `onLidStateChanged(bool)` slot to broadcast `pauseActivity()`/`resumeActivity()` calls.

- [x] T-005: Implement ActivityGateManager fan-out logic
  - REQs: REQ-F-002, REQ-F-003, REQ-NF-001
  - Check: `onLidStateChanged(true)` calls `pauseActivity()` on all registered gates within 1 second; `onLidStateChanged(false)` calls `resumeActivity()`.

## Foundation: Integrate Activity Gate with Existing Services

- [x] T-006: Modify CalendarSyncManager to implement IActivityGate (part 1: header)
  - REQs: REQ-F-002, REQ-F-003
  - Check: `CalendarSyncManager` header declares `lid_paused_` boolean and `applyPausedState()` helper; `pauseActivity()` and `resumeActivity()` override methods are declared.

- [x] T-007: Modify CalendarSyncManager to implement IActivityGate (part 2: implementation)
  - REQs: REQ-F-002, REQ-F-003
  - Check: `pauseActivity()` sets `lid_paused_=true`, `resumeActivity()` sets `lid_paused_=false`, and `applyPausedState()` ORs both `idle_paused_` and `lid_paused_` to decide timer stop/start (timers stop if either is true).

- [x] T-008: Modify WeatherService to implement IActivityGate (part 1: header)
  - REQs: REQ-F-002, REQ-F-003
  - Check: `WeatherService` header declares `pauseActivity()` and `resumeActivity()` override methods.

- [x] T-009: Modify WeatherService to implement IActivityGate (part 2: implementation)
  - REQs: REQ-F-002, REQ-F-003
  - Check: `pauseActivity()` stops `refresh_timer_`; `resumeActivity()` calls immediate `fetchWeather()` then restarts `refresh_timer_`; both methods are idempotent.

- [x] T-010: Wire ActivityGateManager in ShellApplication
  - REQs: REQ-F-002, REQ-F-003
  - Check: `ShellApplication` creates `LidStateMonitor`, `ActivityGateManager`, registers `calendar_sync_manager_`, `weather_`, and `suspend_inhibitor_service_` with the manager, and connects lid state changes to the manager's slot.

## Feature: Low Battery Notifications

- [x] T-011: Implement LowBatteryMonitor header
  - REQs: REQ-F-006, REQ-F-007, REQ-F-008, REQ-NF-002
  - Check: `LowBatteryMonitor` header declares threshold integers (defaults 20/10), `warningSent_` and `criticalSent_` tracking booleans, and private slots for battery signal connections.

- [x] T-012: Implement LowBatteryMonitor threshold state machine
  - REQs: REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009, REQ-F-010, REQ-F-011
  - Check: On `percentChanged()` while discharging, fires warning notification once per cycle if percent ≤ threshold and `warningSent_` is false; fires critical notification with urgency=2 and timeout=0 if percent ≤ critical threshold; `resetState()` fires on `chargingChanged()`, `fullyChargedChanged()`, and `presentChanged()` to reset tracking flags.

- [x] T-013: Load LowBatteryMonitor thresholds from QSettings
  - REQs: REQ-NF-002, REQ-C-002, REQ-C-003
  - Check: Constructor reads `holonight/power/warningThreshold` (default 20) and `holonight/power/criticalThreshold` (default 10) from QSettings; clamps critical to be < warning if misconfigured.

- [x] T-014: Implement LowBatteryMonitor D-Bus notification sending
  - REQs: REQ-F-007, REQ-F-008
  - Check: `sendNotification()` calls `org.freedesktop.Notifications.Notify()` with `app_icon="battery-caution"` for warning (urgency=1, timeout=10s) and `app_icon="battery-low"` for critical (urgency=2, timeout=0).

- [x] T-015: Wire LowBatteryMonitor in ShellApplication
  - REQs: REQ-F-006
  - Check: `ShellApplication::startServices()` creates `low_battery_monitor_` parented to `this`, passing `battery_` pointer; monitor connects to battery service signals in its constructor.

## Feature: Suspend Inhibitor Service

- [x] T-016: Implement InhibitorModel header and basic model interface
  - REQs: REQ-F-012, REQ-F-013, REQ-F-015
  - Check: `InhibitorModel` is a `QAbstractListModel` with `WhoRole` and `WhyRole` enum roles, `count` Q_PROPERTY, and `setEntries()` method to replace entries and emit `modelReset` when data changes.

- [x] T-017: Implement InhibitorModel data access
  - REQs: REQ-F-013, REQ-F-015
  - Check: `rowCount()` returns entry count, `data()` returns `who` or `why` for the requested role, `roleNames()` maps enum values to property names ("who", "why").

- [x] T-018: Implement SuspendInhibitorService header (QML singleton)
  - REQs: REQ-F-012, REQ-NF-003
  - Check: `SuspendInhibitorService` is a `QObject` with `QML_ELEMENT` and `QML_SINGLETON` macros, declares `inhibitorModel()` accessor returning `InhibitorModel*`, and `pauseActivity()`/`resumeActivity()` override methods.

- [x] T-019: Implement SuspendInhibitorService ListInhibitors polling (part 1: safe parsing)
  - REQs: REQ-F-012, REQ-F-015
  - Check: `poll()` calls `org.freedesktop.login1.Manager.ListInhibitors()`, checks reply type with `userType() == qMetaTypeId<QDBusArgument>()` (NOT `canConvert`), and extracts struct fields (What, Who, Why, Mode, UID, PID) via stream extraction.

- [x] T-020: Implement SuspendInhibitorService ListInhibitors filtering
  - REQs: REQ-F-012, REQ-F-015
  - Check: `poll()` filters entries to keep only those where `What` contains `"sleep"`, dropping `"idle"`, `"shutdown"`, and `"handle-lid-switch"` inhibitors; filtered entries are passed to `model_.setEntries()`.

- [x] T-021: Implement SuspendInhibitorService poll timer and IActivityGate
  - REQs: REQ-NF-003, REQ-F-002, REQ-F-003
  - Check: `start()` calls immediate `poll()` then starts `poll_timer_` (5s interval); `pauseActivity()` stops timer, `resumeActivity()` calls `poll()` immediately then restarts timer; both are idempotent.

- [x] T-022: Register SuspendInhibitorService as QML singleton in ShellApplication
  - REQs: REQ-F-012, REQ-F-013
  - Check: `ShellApplication::registerQmlTypes()` registers `SuspendInhibitorService` with module name; QML can access it via `import HolonightShell; SuspendInhibitorService.inhibitorModel`.

- [x] T-023: Wire SuspendInhibitorService in ShellApplication startServices
  - REQs: REQ-F-012
  - Check: `ShellApplication::startServices()` calls `suspend_inhibitor_service_->start()` and registers it with `activity_gate_manager_` before connecting lid state.

## Feature: Charge Limit Display

- [x] T-024: Extend BatteryState.h with charge_limit field
  - REQs: REQ-F-017, REQ-F-018
  - Check: `BatteryStateUpdate` struct declares `std::optional<int> charge_limit` field.

- [x] T-025: Extend BatteryService header with chargeLimit property
  - REQs: REQ-F-017, REQ-F-019, REQ-F-020
  - Check: `BatteryService` declares `chargeLimit` Q_PROPERTY (int, NOTIFY), `chargeLimit()` accessor (returns int -1 if unavailable), and private `probeChargeLimit()` method.

- [x] T-026: Implement BatteryService probe order 1 (UPower ChargeEndThreshold)
  - REQs: REQ-F-017
  - Check: `probeChargeLimit()` reads `ChargeEndThreshold` property from UPower device object; if present and > 0, sets `charge_limit_` and returns early (success path).

- [x] T-027: Implement BatteryService probe order 2 (sysfs BAT0 threshold)
  - REQs: REQ-F-018
  - Check: If UPower fails, reads `/sys/class/power_supply/BAT0/charge_control_end_threshold` via `QFile`; if > 0, sets `charge_limit_` and returns early.

- [x] T-028: Implement BatteryService probe order 3 (sysfs BAT1 threshold)
  - REQs: REQ-F-018
  - Check: If BAT0 fails or is 0, reads `/sys/class/power_supply/BAT1/charge_control_end_threshold`; if > 0, sets `charge_limit_` and returns early.

- [x] T-029: Call probeChargeLimit from BatteryService::start()
  - REQs: REQ-F-017, REQ-NF-004
  - Check: `BatteryService::start()` calls `probeChargeLimit()` after `device_path_` is populated; result is cached for the session unless UPower emits `ChargeEndThreshold` in `PropertiesChanged`.

## QML Components

- [x] T-030: Create InhibitorSection.qml sidebar component
  - REQs: REQ-F-013, REQ-F-014, REQ-F-015, REQ-C-005
  - Check: `InhibitorSection.qml` renders a column with "Sleep inhibitors" label, a repeater over `SuspendInhibitorService.inhibitorModel`, shows `who` and `why` fields per row, and has `visible: count > 0`; no interactive elements.

- [x] T-031: Create ChargeLimitRow.qml sidebar component
  - REQs: REQ-F-019, REQ-F-020, REQ-C-006
  - Check: `ChargeLimitRow.qml` renders a row with "Charge limit" label and percentage value, has `visible: BatteryService.chargeLimit >= 0`, and is read-only with no input fields.

- [x] T-032: Integrate InhibitorSection into RightSidebar layout
  - REQs: REQ-F-013
  - Check: `SidebarContent.qml` or the battery-section parent includes `InhibitorSection` component in the battery panel; tested with an active inhibitor to confirm visibility.

- [x] T-033: Integrate ChargeLimitRow into RightSidebar layout
  - REQs: REQ-F-019
  - Check: `SidebarContent.qml` or the battery-section parent includes `ChargeLimitRow` component below existing battery rows; tested to confirm row appears when `chargeLimit >= 0`.

## CMake Integration

- [x] T-034: Add new C++ sources to CMakeLists.txt (holonight_services)
  - REQs: REQ-F-001, REQ-F-002, REQ-F-006, REQ-F-012, REQ-F-017
  - Check: `LidStateMonitor.cpp`, `ActivityGateManager.cpp`, `LowBatteryMonitor.cpp`, `InhibitorModel.cpp`, `SuspendInhibitorService.cpp` are listed in the `add_library(holonight_services STATIC ...)` target.

- [x] T-035: Add new QML files to CMakeLists.txt
  - REQs: REQ-F-013, REQ-F-019
  - Check: `src/qml/RightSidebar/InhibitorSection.qml` and `src/qml/RightSidebar/ChargeLimitRow.qml` are added to `HOLONIGHT_QML_FILES` in alphabetical order; `cmake --build` succeeds and derives correct QRC aliases.

- [x] T-036: Run task qmltypes-check after registration changes
  - REQs: REQ-F-013, REQ-F-019
  - Check: `task qmltypes-check` reports that `SuspendInhibitorService` and `InhibitorModel` are present in the generated `qmltypes` file (not just `Module {}`).

## Unit Tests

- [x] T-037: Unit test LidStateMonitor (desktop/laptop graceful handling)
  - REQs: REQ-F-004, REQ-F-005
  - Check: Test mocks UPower properties returning `LidIsPresent=false` and confirms no signal subscription; test mocks `LidIsPresent=true, LidIsClosed=true` and confirms signal connects and state updates.

- [x] T-038: Unit test ActivityGateManager gate registration and fan-out
  - REQs: REQ-F-002, REQ-F-003
  - Check: Test registers mock gates, calls `onLidStateChanged(true)`, and verifies all gates' `pauseActivity()` was called exactly once; same for `resumeActivity()`.

- [x] T-039: Unit test LowBatteryMonitor threshold crossing and state machine
  - REQs: REQ-F-007, REQ-F-008, REQ-F-009, REQ-F-010
  - Check: Test simulates battery discharge from 25% to 20% (warning fires once), 15% to 10% (critical fires once), then charge to 21% (reset), then discharge to 20% again (warning fires again); verify flags and notification calls.

- [x] T-040: Unit test LowBatteryMonitor QSettings loading and clamping
  - REQs: REQ-NF-002, REQ-C-002, REQ-C-003
  - Check: Test sets QSettings to non-default values and confirms thresholds are loaded; test sets critical ≥ warning and confirms clamping with warning logged.

- [x] T-041: Unit test SuspendInhibitorService ListInhibitors parsing and filtering
  - REQs: REQ-F-012, REQ-F-015
  - Check: Test mocks `ListInhibitors()` reply with mixed inhibitor types (sleep, idle, shutdown) and confirms only sleep-containing entries are stored; test parses struct fields correctly into `who` and `why`.

- [x] T-042: Unit test InhibitorModel Qt roles and data access
  - REQs: REQ-F-013, REQ-F-015
  - Check: Test populates model with entries, queries `data()` with `WhoRole` and `WhyRole`, confirms correct values returned and `countChanged()` signal emitted on `setEntries()`.

- [x] T-043: Unit test BatteryService charge limit probes (UPower path)
  - REQs: REQ-F-017
  - Check: Test mocks UPower property query returning valid percentage; confirms `chargeLimit()` returns that value and no sysfs fallback is attempted.

- [x] T-044: Unit test BatteryService charge limit probes (sysfs fallback)
  - REQs: REQ-F-018, REQ-F-020
  - Check: Test mocks UPower returning unavailable/0, mocks sysfs BAT0 with valid percentage; confirms fallback is attempted and result is used. Test all three probes fail; confirms `chargeLimit()` returns -1.

## QML Tests

- [x] T-045: QML test InhibitorSection visibility binding
  - REQs: REQ-F-013, REQ-F-014
  - Check: QML test creates component, sets `count` property to 0, confirms section not visible; sets `count > 0`, confirms section visible and children rendered.

- [x] T-046: QML test ChargeLimitRow visibility binding and value display
  - REQs: REQ-F-019, REQ-F-020
  - Check: QML test creates component, sets `chargeLimit` to -1, confirms row not visible; sets `chargeLimit` to 80, confirms row visible and text displays "Charge limit: 80%".

## Integration & Verification

- [x] T-047: Integration test: Full activity gate cycle (lid close/open)
  - REQs: REQ-F-002, REQ-F-003, REQ-NF-001
  - Check: Test wires `LidStateMonitor`, `ActivityGateManager`, and mock gates; simulates UPower lid close signal, confirms all gates' `pauseActivity()` called within test timing; simulates lid open, confirms `resumeActivity()` called.

- [x] T-048: Manual test: Verify no unintended side effects on desktop systems
  - REQs: REQ-F-005, REQ-C-001
  - Check: Run shell on a desktop (lid not present), confirm no errors in log, no UI elements appear showing lid state, and services poll normally (not paused).

- [x] T-049: Manual verification of full feature flow in live Wayland session
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-006, REQ-F-007, REQ-F-012, REQ-F-013, REQ-F-019
  - Check: Run `task run`, close/open lid (if available), confirm no crashes, observe battery notifications at threshold, observe inhibitors appear/disappear in sidebar when active, confirm charge limit row visible on supported hardware.


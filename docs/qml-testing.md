# QML Unit Testing

This project supports writing and running native QML unit tests using the `QtQuickTest` framework (`TestCase` components). These tests run offscreen (`QT_QPA_PLATFORM=offscreen`) as part of the normal CTest suite.

## How it Works

The QML unit testing infrastructure consists of:

1. **The Test Harness ([tests/test_qml_harness.cpp](file:///home/andrii/Projects/pet/holonight/holonight-shell/tests/test_qml_harness.cpp))**:
   - Uses the `QUICK_TEST_MAIN_WITH_SETUP` macro to define the entry point.
   - Instantiates `FakeQmlServices` globally so that all mock C++ singletons (`BatteryService`, `AudioService`, `NetworkService`, etc.) are registered on the test engines.
   - Generates a temporary QML module directory with a `qmldir` mapping `HolonightShell` types to the source files (`apps/shell/qml/...`).
   - Hooks into `qmlEngineAvailable` to inject the temporary directory into the engine's import path.
2. **QML Test Files (`tests/qml/tst_*.qml`)**:
   - Any file matching `tst_*.qml` under `tests/qml/` is recursively discovered and run by the test harness.
   - Imports `QtQuick`, `QtTest`, and `HolonightShell`.

---

## Writing a QML Test

To add a new QML test, simply create a file named `tst_yourtest.qml` inside `tests/qml/`. 

Here is an example test case showing how to test mock services and instantiate shell components:

```qml
import QtQuick
import QtTest
import HolonightShell

TestCase {
    name: "MyWidgetTests"

    // Test C++ mock singleton properties
    function test_battery_service() {
        compare(BatteryService.percent, 74, "Initial battery percent should match FakeQmlServices setup")
    }

    // Declare a component to test
    Component {
        id: batteryIndicatorComponent
        BatteryIndicator {
            percent: 90
            charging: false
            discharging: true
            fullyCharged: false
        }
    }

    // Instantiation and property test
    function test_battery_indicator_creation() {
        var indicator = createTemporaryObject(batteryIndicatorComponent, null)
        verify(indicator !== null, "BatteryIndicator instance should be successfully created")
        compare(indicator.percent, 90, "percent property should match")
        compare(indicator.low, false, "90% is not low battery")
    }
}
```

---

## Running QML Tests

QML tests are integrated with the build target and run automatically via CTest.

### 1. Run via Task
You can run all tests (including C++ and QML tests) using the standard task:
```bash
task test
```

### 2. Run Verbose QML Tests Directly
To run only the QML test harness with verbose/detailed assertion outputs:
```bash
ctest -R test_holonight_qml_harness -V
```

---

## Key Benefits & Best Practices

- **Offscreen Execution**: Tests use the `offscreen` QPA platform, meaning they require no active display server/Wayland compositor and can run in CI pipelines.
- **Service Isolation**: Always use the fake singletons provided by `FakeQmlServices` (e.g. `BatteryService`, `AudioService`) to control state during test assertions rather than communicating with real D-Bus or system services.
- **Dynamic Lifecycle**: Prefer declaring components inside a `Component` block and instantiating them dynamically with `createTemporaryObject(component, parent)` inside the test functions. This ensures objects are cleaned up automatically after each test case.

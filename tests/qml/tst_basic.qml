import QtQuick
import QtTest
import HolonightShell

TestCase {
    name: "BasicQmlTests"

    function test_battery_service() {
        compare(BatteryService.percent, 74, "Initial battery percent should be 74")
    }

    Component {
        id: batteryIndicatorComponent
        BatteryIndicator {
            percent: 90
            charging: false
            discharging: true
            fullyCharged: false
        }
    }

    function test_battery_indicator_creation() {
        var indicator = createTemporaryObject(batteryIndicatorComponent, null)
        verify(indicator !== null, "BatteryIndicator instance should be successfully created")
        compare(indicator.percent, 90, "percent property should match what we set")
        compare(indicator.low, false, "90% is not low battery")
    }
}

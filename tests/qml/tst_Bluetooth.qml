import QtQuick
import QtTest
import HolonightShell

TestCase {
    id: testCase
    name: "Bluetooth"
    when: windowShown
    visible: true
    width: 500
    height: 700
    Component {
        id: popupComponent
        BluetoothPopupContent {
            width: 400
            height: 600
        }
    }
    Component {
        id: widgetComponent
        BluetoothWidget {
            barMonitorName: "test"
            height: 32
        }
    }

    function init() {
        BluetoothTestSeed.seed();
    }
    function cleanup() {
        BluetoothService.setActive(false);
        BluetoothTestSeed.clear();
    }
    function test_sectionsAndPendingError() {
        const popup = createTemporaryObject(popupComponent, testCase);
        verify(popup !== null);
        compare(BluetoothService.devices.length, 3);
        compare(BluetoothService.devices[0].section, "Connected");
        compare(BluetoothService.devices[1].section, "Paired");
        compare(BluetoothService.devices[2].section, "Available");
        BluetoothService.action("keyboard", "connect");
        tryVerify(() => BluetoothService.devices[1].busy);
        wait(1);
        BluetoothTestSeed.fail();
        tryVerify(() => BluetoothService.errorMessage.length > 0);
    }
    function test_widgetAndPairing() {
        const widget = createTemporaryObject(widgetComponent, testCase);
        verify(widget !== null);
        compare(widget.visible, true);
        compare(BluetoothService.connectedCount, 1);
        verify(BluetoothService.tooltip.indexOf("72%") >= 0);
        const popup = createTemporaryObject(popupComponent, testCase);
        verify(popup !== null);
        BluetoothTestSeed.pairing();
        compare(BluetoothService.interaction.code, "000042");
        BluetoothService.cancelPairing();
        verify(!BluetoothService.interaction.id);
        BluetoothTestSeed.clear();
        tryCompare(widget, "visible", false);
    }
}

import HolonightShell
import Holonight.Core
import QtQuick
import QtTest

TestCase {
    id: testCase

    Rectangle {
        anchors.fill: parent
        z: -1
        color: HoloniightPalette.background
    }

    function init() {
        BluetoothTestSeed.seed();
    }

    function cleanup() {
        BluetoothService.setActive(false);
        BluetoothTestSeed.clear();
    }

    function test_rowBatteryAndKeyboard() {
        const row = createTemporaryObject(rowComponent, testCase, {
            "record": {
                "id": "test",
                "name": "A very long device name that should elide within the row width",
                "connected": true,
                "paired": true,
                "busy": false
            }
        });
        verify(row !== null);
        compare(findChild(row, "battery-test").visible, false);
        row.record = {
            "id": "test",
            "name": "Headphones",
            "connected": true,
            "paired": true,
            "busy": false,
            "battery": 0
        };
        compare(findChild(row, "battery-test").visible, true);
        const button = findChild(row, "device-test");
        const spy = createTemporaryObject(spyComponent, testCase, {
            "target": row,
            "signalName": "expansionRequested"
        });
        button.forceActiveFocus();
        keyClick(Qt.Key_Space);
        compare(spy.count, 1);
        row.record = {
            "id": "test",
            "name": "Headphones",
            "connected": true,
            "paired": true,
            "busy": true
        };
        compare(button.enabled, false);
    }

    function test_expansionActionsAndAdapterChange() {
        const popup = createTemporaryObject(popupComponent, testCase);
        verify(popup !== null);
        waitForRendering(popup);
        const headphones = findChild(popup, "device-headphones");
        mouseClick(headphones);
        compare(popup.expandedId, "headphones");
        verify(findChild(popup, "expandedAction-headphones").visible);
        mouseClick(findChild(popup, "device-keyboard"));
        compare(popup.expandedId, "keyboard");
        verify(!findChild(popup, "expandedAction-headphones").visible);
        mouseClick(findChild(popup, "action-keyboard"));
        tryCompare(findChild(popup, "action-keyboard"), "enabled", false);
        compare(popup.expandedId, "keyboard");
        BluetoothTestSeed.fail();
        BluetoothService.selectAdapter("other");
        compare(popup.expandedId, "");
        compare(findChild(popup, "section-Connected").visible, false);
        compare(findChild(popup, "bluetoothSummary").count, 0);
        BluetoothService.selectAdapter("adapter");
        popup.expandedId = "headphones";
        BluetoothTestSeed.clear();
        compare(popup.expandedId, "");
    }

    function test_scanAndPairingVisibility() {
        const popup = createTemporaryObject(popupComponent, testCase);
        verify(popup !== null);
        waitForRendering(popup);
        compare(findChild(popup, "section-Available").visible, false);
        BluetoothService.setActive(true);
        tryCompare(findChild(popup, "section-Available"), "visible", true);
        BluetoothTestSeed.pairing();
        verify(findChild(popup, "bluetoothPairing").visible);
        BluetoothTestSeed.complete();
        tryCompare(findChild(popup, "bluetoothScan"), "enabled", true);
        mouseClick(findChild(popup, "bluetoothScan"));
        BluetoothTestSeed.complete();
        compare(findChild(popup, "section-Available").visible, false);
        verify(findChild(popup, "bluetoothPairing").visible);
        tryCompare(findChild(popup, "bluetoothScan"), "enabled", true);
        mouseClick(findChild(popup, "bluetoothScan"));
        compare(BluetoothService.scanning, true);
        BluetoothTestSeed.complete();
        tryCompare(findChild(popup, "bluetoothScan"), "enabled", true);
    }

    function test_pairAndDisconnectDispatch() {
        const popup = createTemporaryObject(popupComponent, testCase);
        BluetoothService.setActive(true);
        BluetoothTestSeed.complete();
        tryCompare(findChild(popup, "bluetoothScan"), "enabled", true);
        mouseClick(findChild(popup, "action-phone"));
        tryCompare(findChild(popup, "action-phone"), "enabled", false);
        compare(popup.expandedId, "");
        waitForRendering(popup);
        BluetoothTestSeed.fail();
        tryCompare(findChild(popup, "action-phone"), "enabled", true);
        mouseClick(findChild(popup, "device-headphones"));
        mouseClick(findChild(popup, "expandedAction-headphones"));
        tryCompare(findChild(popup, "expandedAction-headphones"), "enabled", false);
        waitForRendering(popup);
        BluetoothTestSeed.fail();
    }
    function test_settingsNavigationAndDismissal() {
        const popup = createTemporaryObject(popupComponent, testCase);
        const hides = StatusPopupSurface.hideCount;
        mouseClick(findChild(popup, "bluetoothSettings"));
        compare(SettingsNavigationService.lastOpenedPage, "bluetooth");
        compare(StatusPopupSurface.hideCount, hides + 1);
    }
    function test_sixDevicePreviewAndConstrainedHeight() {
        BluetoothTestSeed.preview();
        BluetoothService.setActive(true);
        BluetoothService.scan(true);
        tryVerify(() => BluetoothService.selectedAdapter.busy);
        BluetoothTestSeed.complete();
        tryVerify(() => !BluetoothService.selectedAdapter.busy);
        const popup = createTemporaryObject(popupComponent, testCase, {
            "height": 680
        });
        verify(popup !== null);
        waitForRendering(popup);
        compare(BluetoothService.devices.length, 6);
        compare(findChild(popup, "bluetoothSummary").count, 2);
        waitForRendering(popup);
        grabImage(popup).save(Qt.resolvedUrl("../../build/dev/bluetooth-popup-preview.png").toString().replace("file://", ""));
        popup.height = 260;
        tryVerify(() => findChild(popup, "devices-Available").interactive);
        verify(!popup.viewportItem.interactive);
        verify(findChild(popup, "devices-Connected").clip);
        verify(findChild(popup, "devices-Paired").clip);
        verify(popup.viewportItem.clip);
    }

    function test_availableRowsHaveIndependentFiveRowViewport() {
        BluetoothTestSeed.manyDevices();
        BluetoothService.setActive(true);
        BluetoothTestSeed.complete();
        tryVerify(() => !BluetoothService.selectedAdapter.busy);
        const popup = createTemporaryObject(popupComponent, testCase, {height: 900});
        waitForRendering(popup);
        const viewport = findChild(popup, "devices-Available");
        verify(viewport.contentHeight > viewport.height);
        verify(viewport.height <= 5 * 64 + 4 * 4);
        verify(!popup.viewportItem.interactive);
        viewport.contentY = viewport.contentHeight - viewport.height;
        verify(viewport.contentY > 0);
    }
    function test_wholeRowHover() {
        const row = createTemporaryObject(rowComponent, testCase, {
            record: {id: "hover", name: "Keyboard", icon: "input-keyboard", connected: true, battery: 50}
        });
        waitForRendering(row);
        const background = findChild(row, "rowBackground-hover");
        mouseMove(row, 390, 32);
        tryVerify(() => background.color.a > 0);
        mouseMove(row, 80, 32);
        verify(background.color.a > 0);
        mouseMove(testCase, 499, 699);
        tryCompare(background, "color", "#00000000");
    }
    function test_sectionsAndPendingError() {
        const popup = createTemporaryObject(popupComponent, testCase);
        verify(popup !== null);
        waitForRendering(popup);
        compare(BluetoothService.devices.length, 3);
        compare(BluetoothService.devices[0].section, "Connected");
        compare(BluetoothService.devices[1].section, "Paired");
        compare(BluetoothService.devices[2].section, "Available");
        BluetoothService.action("keyboard", "connect");
        tryVerify(() => {
            return BluetoothService.devices[1].busy;
        });
        wait(1);
        BluetoothTestSeed.fail();
        tryVerify(() => {
            return BluetoothService.errorMessage.length > 0;
        });
    }

    function test_widgetAndPairing() {
        const widget = createTemporaryObject(widgetComponent, testCase);
        verify(widget !== null);
        compare(widget.visible, true);
        compare(BluetoothService.connectedCount, 1);
        verify(BluetoothService.tooltip.indexOf("72%") >= 0);
        const popup = createTemporaryObject(popupComponent, testCase);
        verify(popup !== null);
        waitForRendering(popup);
        BluetoothTestSeed.pairing();
        compare(BluetoothService.interaction.code, "000042");
        BluetoothService.cancelPairing();
        verify(!BluetoothService.interaction.id);
        BluetoothTestSeed.clear();
        tryCompare(widget, "visible", false);
    }

    name: "Bluetooth"
    when: windowShown
    visible: true
    width: 500
    height: 700

    Component {
        id: popupComponent

        BluetoothPopupContent {
            width: 480
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

    Component {
        id: rowComponent

        BluetoothDeviceRow {
            width: 480
        }

    }

    Component {
        id: spyComponent

        SignalSpy {
        }

    }

}

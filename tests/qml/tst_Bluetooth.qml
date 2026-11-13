import HolonightShell
import Holonight.Core
import QtQuick
import QtQuick.Controls as Controls
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

    function test_initialFocusAndReopen() {
        const popup = createTemporaryObject(popupComponent, testCase);
        verify(popup !== null);
        const power = findChild(popup, "bluetoothPower");
        tryCompare(power, "activeFocus", true);
        keyClick(Qt.Key_Tab);
        verify(!power.activeFocus);
        keyClick(Qt.Key_Backtab);
        tryCompare(power, "activeFocus", true);
        popup.visible = false;
        popup.visible = true;
        tryCompare(power, "activeFocus", true);
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
        mouseClick(findChild(popup, "expandedAction-keyboard"));
        tryCompare(findChild(popup, "expandedAction-keyboard"), "enabled", false);
        compare(popup.expandedId, "keyboard");
        BluetoothTestSeed.fail();
        BluetoothService.selectAdapter("other");
        compare(popup.expandedId, "");
        compare(findChild(popup, "bluetoothDevices").count, 0);
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
        compare(BluetoothService.scanning, false);
        BluetoothService.setActive(true);
        tryCompare(BluetoothService, "scanning", true);
        BluetoothTestSeed.pairing();
        verify(findChild(popup, "bluetoothPairing").visible);
        BluetoothTestSeed.complete();
        tryCompare(findChild(popup, "bluetoothScan"), "enabled", true);
        mouseClick(findChild(popup, "bluetoothScan"));
        BluetoothTestSeed.complete();
        compare(BluetoothService.scanning, false);
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
        grabImage(popup).save(controlsEvidence.capturePath("bluetooth-popup-preview.png"));
        popup.expandedId = "headphones";
        waitForRendering(popup);
        grabImage(popup).save(controlsEvidence.capturePath("bluetooth-popup-expanded-preview.png"));
        popup.height = 260;
        tryVerify(() => findChild(popup, "bluetoothDevices").interactive);
        verify(!popup.viewportItem.interactive);
        verify(findChild(popup, "bluetoothDevices").clip);
        verify(popup.viewportItem.clip);
    }

    function test_deviceCollectionHasOneBoundedViewport() {
        BluetoothTestSeed.manyDevices();
        BluetoothService.setActive(true);
        BluetoothTestSeed.complete();
        tryVerify(() => !BluetoothService.selectedAdapter.busy);
        const popup = createTemporaryObject(popupComponent, testCase, {
            height: 900
        });
        waitForRendering(popup);
        const viewport = findChild(popup, "bluetoothDevices");
        verify(viewport.contentHeight > viewport.height);
        verify(viewport.height <= 5 * 68);
        verify(!popup.viewportItem.interactive);
        viewport.contentY = viewport.contentHeight - viewport.height;
        verify(viewport.contentY > 0);
    }
    function test_wholeRowHover() {
        const row = createTemporaryObject(rowComponent, testCase, {
            record: {
                id: "hover",
                name: "Keyboard",
                icon: "input-keyboard",
                connected: true,
                battery: 50
            }
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
    function test_tooltipRequiresTruncationAndMainHover() {
        const row = createTemporaryObject(rowComponent, testCase, {
            record: {
                id: "tooltip",
                name: "Short",
                connected: false,
                paired: false
            }
        });
        waitForRendering(row);
        const button = findChild(row, "device-tooltip");
        const label = findChild(row, "name-tooltip");
        button.hoverEnabled = true;
        mouseMove(button, 30, 30);
        tryCompare(button, "hovered", true);
        verify(!label.truncated);
        verify(!button.Controls.ToolTip.visible);
        row.record = {
            id: "tooltip",
            name: "A very long Bluetooth device name repeated to exceed the available label width",
            connected: false,
            paired: false
        };
        tryCompare(label, "truncated", true);
        tryCompare(button.Controls.ToolTip, "visible", true);
        mouseMove(findChild(row, "action-tooltip"), 50, 15);
        tryCompare(button.Controls.ToolTip, "visible", false);
    }

    function test_expandedCardPaddingAndSingleAction() {
        const row = createTemporaryObject(rowComponent, testCase, {
            record: {
                id: "card",
                name: "Keyboard",
                connected: false,
                paired: true
            }
        });
        waitForRendering(row);
        compare(row.implicitHeight, 64);
        verify(!findChild(row, "action-card").visible);
        verify(!findChild(row, "expandedAction-card").visible);
        row.expanded = true;
        row.height = Qt.binding(() => row.implicitHeight);
        const action = findChild(row, "expandedAction-card");
        const expand = findChild(row, "expand-card");
        const main = findChild(row, "device-card");
        const background = findChild(row, "rowBackground-card");
        tryVerify(() => action.visible && row.height > 64);
        waitForRendering(row);
        compare(main.mapToItem(row, 0, 0).x, 12);
        compare(expand.mapToItem(row, expand.width, 0).x, row.width - 12);
        compare(action.mapToItem(row, action.width, action.height).x, row.width - 12);
        compare(action.mapToItem(row, 0, action.height).y, row.height - 12);
        compare(background.height, row.height);
        compare(background.radius, 8);
        compare(background.border.width, 1);
    }

    function test_batteryColumnDoesNotShiftAtOneHundredPercent() {
        const row = createTemporaryObject(rowComponent, testCase, {
            record: {
                id: "battery",
                name: "Headphones",
                connected: true,
                paired: true,
                battery: 9
            }
        });
        waitForRendering(row);
        const battery = findChild(row, "battery-battery");
        const position = battery.mapToItem(row, 0, 0).x;
        const width = battery.width;
        row.record = {
            id: "battery",
            name: "Headphones",
            connected: true,
            paired: true,
            battery: 100
        };
        waitForRendering(row);
        compare(battery.mapToItem(row, 0, 0).x, position);
        compare(battery.width, width);
        compare(width, 84);
    }

    function test_collectionNavigationAcrossSectionsAndDiscovery() {
        BluetoothTestSeed.discovery(3);
        BluetoothService.setActive(true);
        BluetoothTestSeed.complete();
        const popup = createTemporaryObject(popupComponent, testCase);
        waitForRendering(popup);
        const list = findChild(popup, "bluetoothDevices");
        verify(list.count >= 3);
        list.forceActiveFocus();
        keyClick(Qt.Key_End);
        compare(list.currentIndex, list.count - 1);
        keyClick(Qt.Key_Home);
        compare(list.currentIndex, 0);
        keyClick(Qt.Key_Down);
        compare(list.currentIndex, 1);
        const identity = list.currentItem.record.id;
        BluetoothTestSeed.discovery(8);
        waitForRendering(popup);
        compare(list.currentItem.record.id, identity);
        keyClick(Qt.Key_PageDown);
        verify(list.currentIndex > 1);
        keyClick(Qt.Key_End);
        compare(list.currentIndex, list.count - 1);
        keyClick(Qt.Key_Down);
        compare(list.currentIndex, list.count - 1);
        // Navigation never dispatches a device action.
        verify(!BluetoothService.devices.some(device => device.busy));
    }

    function test_profileCountsAndModifierIsolation() {
        BluetoothTestSeed.discovery(8);
        BluetoothService.setActive(true);
        BluetoothTestSeed.complete();
        const popup = createTemporaryObject(popupComponent, testCase);
        waitForRendering(popup);
        const list = findChild(popup, "bluetoothDevices");
        list.forceActiveFocus();
        keyClick(Qt.Key_Home);
        keyClick(Qt.Key_J, Qt.AltModifier);
        compare(list.currentIndex, 0);
        keyClick(Qt.Key_J);
        compare(list.currentIndex, HnInputPreferences.vimNavigation ? 1 : 0);
        if (HnInputPreferences.vimNavigation) {
            keyClick(Qt.Key_2);
            keyClick(Qt.Key_J);
            compare(list.currentIndex, 3);
            keyClick(Qt.Key_G);
            keyClick(Qt.Key_G);
            compare(list.currentIndex, 0);
            keyClick(Qt.Key_D, Qt.ControlModifier);
            verify(list.currentIndex > 0);
            keyClick(Qt.Key_G, Qt.ShiftModifier);
            compare(list.currentIndex, list.count - 1);
            keyClick(Qt.Key_K, Qt.ControlModifier);
            verify(findChild(popup, "bluetoothPower").activeFocus);
            keyClick(Qt.Key_J, Qt.ControlModifier);
            verify(list.activeFocus);
            keyClick(Qt.Key_J, Qt.ControlModifier);
            verify(findChild(popup, "bluetoothScan").activeFocus);
            keyClick(Qt.Key_K, Qt.ControlModifier);
            verify(list.activeFocus);
            keyClick(Qt.Key_Home);
            keyClick(Qt.Key_L);
            const action = findChild(popup, "expandedAction-headphones");
            tryCompare(action, "activeFocus", true);
            const current = list.currentIndex;
            keyClick(Qt.Key_J);
            compare(list.currentIndex, current);
            keyClick(Qt.Key_H);
            verify(list.activeFocus);
        }
        verify(!BluetoothService.devices.some(device => device.busy));
    }

    function test_discoveryPreservesFocusedAction() {
        BluetoothTestSeed.discovery(1);
        BluetoothService.setActive(true);
        BluetoothTestSeed.complete();
        const popup = createTemporaryObject(popupComponent, testCase);
        waitForRendering(popup);
        popup.expandedId = "headphones";
        const action = findChild(popup, "expandedAction-headphones");
        action.forceActiveFocus();
        BluetoothTestSeed.discovery(4);
        waitForRendering(popup);
        compare(findChild(popup, "expandedAction-headphones"), action);
        verify(action.activeFocus);
        compare(popup.expandedId, "headphones");
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
        id: wrapperComponent
        StatusPopup {
            popupId: "bluetooth"
            maximumSurfaceHeight: 1200
            width: 560
            height: 600
        }
    }

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

        SignalSpy {}
    }
}

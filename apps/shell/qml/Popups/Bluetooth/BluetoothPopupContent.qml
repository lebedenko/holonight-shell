import Holonight.Components
import Holonight.Controls
import Holonight.Core
import HolonightShell
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts

PopupContentLayout {
    id: root

    property string expandedId: ""
    property string adapterId: BluetoothService.selectedId

    function syncDevices(): void {
        const desired = [];
        if (BluetoothService.selectedAdapter.powered) {
            for (const section of ["Connected", "Paired", "Available"])
                for (const device of BluetoothService.devices)
                    if (device.section === section && (section !== "Available" || BluetoothService.scanning))
                        desired.push(device);
        }
        for (let i = deviceRows.count - 1; i >= 0; --i)
            if (!desired.some(device => device.id === deviceRows.get(i).record.id))
                deviceRows.remove(i);
        for (let i = 0; i < desired.length; ++i) {
            const device = desired[i];
            let found = -1;
            for (let j = i; j < deviceRows.count; ++j)
                if (deviceRows.get(j).record.id === device.id) {
                    found = j;
                    break;
                }
            if (found < 0)
                deviceRows.insert(i, {
                    record: device,
                    group: device.section
                });
            else {
                if (found !== i)
                    deviceRows.move(found, i, 1);
                const old = deviceRows.get(i);
                if (old.group !== device.section || JSON.stringify(old.record) !== JSON.stringify(device))
                    deviceRows.set(i, {
                        record: device,
                        group: device.section
                    });
            }
        }
        navigation.reconcile(desired.map(device => device.id));
    }

    HnNavigation {
        id: navigation
        active: deviceList.collectionFocus && !BluetoothService.interaction.id
        vimNavigation: HnInputPreferences.vimNavigation
        onCursorChanged: {
            if (currentIndex >= 0)
                deviceList.positionViewAtIndex(currentIndex, ListView.Contain);
        }
    }
    HnNavigation {
        id: panelNavigation
        active: root.visible && !BluetoothService.interaction.id
        vimNavigation: HnInputPreferences.vimNavigation
    }
    function containsFocus(item: Item): bool {
        let focused = root.Window.window ? root.Window.window.activeFocusItem : null;
        while (focused) {
            if (focused === item)
                return true;
            focused = focused.parent;
        }
        return false;
    }
    function focusPanel(command: int): void {
        if (command === HnNavigation.PanelUp) {
            if (root.containsFocus(root.footerItem))
                deviceList.forceActiveFocus();
            else
                root.headerItem.focusControl();
        } else if (command === HnNavigation.PanelDown) {
            if (root.containsFocus(root.headerItem))
                deviceList.forceActiveFocus();
            else
                root.footerItem.focusControl();
        } else if (command === HnNavigation.PanelLeft) {
            if (deviceList.currentItem)
                (deviceList.currentItem as BluetoothDeviceRow).clearControlFocus();
            deviceList.forceActiveFocus();
        } else if (command === HnNavigation.PanelRight && deviceList.currentItem)
            (deviceList.currentItem as BluetoothDeviceRow).enterControls();
    }
    Keys.onPressed: event => {
        if (event.modifiers !== Qt.ControlModifier || !panelNavigation.active)
            return;
        const intent = panelNavigation.interpret(event.key, event.text, event.modifiers);
        if (intent.command >= HnNavigation.PanelLeft) {
            root.focusPanel(intent.command);
            event.accepted = true;
        }
    }
    ListModel {
        id: deviceRows
        dynamicRoles: true
    }
    function focusInitialControl(): void {
        if (root.visible && root.headerItem && !BluetoothService.interaction.id)
            root.headerItem.focusControl();
    }
    onVisibleChanged: if (visible)
        Qt.callLater(root.focusInitialControl)
    Component.onCompleted: {
        syncDevices();
        Qt.callLater(root.focusInitialControl);
    }

    naturalWidth: 480
    scrollBody: false
    onAdapterIdChanged: expandedId = ""

    Connections {
        function onChanged() {
            root.syncDevices();
            if (!BluetoothService.devices.some(device => {
                return device.id === root.expandedId;
            }))
                root.expandedId = "";
        }

        target: BluetoothService
    }

    Controls.ComboBox {
        Layout.fillWidth: true
        visible: BluetoothService.adapters.length > 1
        model: BluetoothService.adapters
        textRole: "alias"
        valueRole: "id"
        currentIndex: indexOfValue(BluetoothService.selectedId)
        onActivated: BluetoothService.selectAdapter(currentValue)
    }

    HnLabel {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        rawText: !BluetoothService.available ? qsTr("Bluetooth service unavailable") : BluetoothService.adapters.length === 0 ? qsTr("No Bluetooth adapter") : !BluetoothService.selectedAdapter.powered ? qsTr("Bluetooth is off") : ""
        visible: rawText.length > 0
    }

    HnLabel {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        visible: BluetoothService.errorMessage.length > 0
        color: HoloniightPalette.error
        rawText: BluetoothService.errorMessage
    }

    HnLabel {
        Layout.fillWidth: true
        visible: !!BluetoothService.selectedAdapter.powered && !BluetoothService.scanning && !BluetoothService.devices.some(device => device.paired || device.connected)
        rawText: qsTr("No paired devices. Start scanning to find nearby devices.")
        wrapMode: Text.WordWrap
        color: HoloniightPalette.textSecondary
    }

    BluetoothPairing {
        Layout.fillWidth: true
        objectName: "bluetoothPairing"
        controller: BluetoothService
    }

    ListView {
        id: deviceList
        objectName: "bluetoothDevices"
        readonly property bool collectionFocus: activeFocus && (!currentItem || !(currentItem as BluetoothDeviceRow).controlsFocused)
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 0
        Layout.preferredHeight: Math.min(contentHeight, 5 * 68)
        Layout.maximumHeight: Layout.preferredHeight
        onContentHeightChanged: Qt.callLater(returnToBounds)
        clip: true
        enabled: !BluetoothService.interaction.id
        activeFocusOnTab: true
        keyNavigationEnabled: false
        boundsBehavior: Flickable.StopAtBounds
        model: deviceRows
        currentIndex: navigation.currentIndex
        spacing: 4
        section.property: "group"
        section.criteria: ViewSection.FullString
        section.delegate: HnLabel {
            required property string section
            objectName: "section-" + section
            width: deviceList.width
            height: 28
            role: HnTypographyRole.MicroHeader
            color: HoloniightPalette.accentBlue
            rawText: section === "Connected" ? qsTr("CONNECTED") : section === "Paired" ? qsTr("PAIRED") : qsTr("AVAILABLE")
        }
        Controls.ScrollBar.vertical: Controls.ScrollBar {}

        function targetTops(): list<real> {
            let tops = [];
            let top = 0;
            let previous = "";
            for (let i = 0; i < deviceRows.count; ++i) {
                const row = deviceRows.get(i);
                if (row.group !== previous)
                    top += 28;
                previous = row.group;
                tops.push(top);
                top += 68;
                if (root.expandedId === row.record.id)
                    top += HnMetrics.controlHeight(HnControlSize.Normal) + 16;
            }
            return tops;
        }
        Keys.onPressed: event => {
            if (!navigation.active)
                return;
            const intent = navigation.interpret(event.key, event.text, event.modifiers);
            if (intent.consumed) {
                if (intent.command >= HnNavigation.PanelLeft)
                    root.focusPanel(intent.command);
                else if (intent.command === HnNavigation.Open && currentItem)
                    (currentItem as BluetoothDeviceRow).enterControls();
                else if (intent.command === HnNavigation.Parent)
                    root.expandedId = "";
                else if (intent.command < HnNavigation.Parent && intent.command !== HnNavigation.Pending)
                    navigation.currentIndex = navigation.target(currentIndex, intent, count, 1, height, 64, targetTops());
                event.accepted = true;
            } else if (event.modifiers === Qt.NoModifier && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) && currentItem) {
                (currentItem as BluetoothDeviceRow).enterControls();
                event.accepted = true;
            }
        }
        delegate: BluetoothDeviceRow {
            required property int index
            width: deviceList.width
            keyboardCursor: deviceList.collectionFocus && navigation.currentIndex === index
            expanded: root.expandedId === record.id
            adapterBusy: !!BluetoothService.selectedAdapter.busy
            onFocusEntered: {
                navigation.currentIndex = index;
                deviceList.positionViewAtIndex(index, ListView.Contain);
            }
            onLeaveControls: {
                root.expandedId = "";
                deviceList.forceActiveFocus();
            }
            onExpansionRequested: root.expandedId = expanded ? "" : record.id
            onActionRequested: action => BluetoothService.action(record.id, action)
        }
    }
    HnLabel {
        Layout.fillWidth: true
        visible: BluetoothService.scanning && deviceRows.count === 0
        rawText: qsTr("Looking for nearby devices…")
        color: HoloniightPalette.textSecondary
    }

    header: Component {
        ColumnLayout {
            function focusControl(): void {
                bluetoothHeader.focusControl();
            }
            spacing: 12
            BluetoothPopupHeader {
                id: bluetoothHeader
                Layout.fillWidth: true
            }
            HnSeparator {
                Layout.fillWidth: true
            }
        }
    }

    footer: Component {
        ColumnLayout {
            function focusControl(): void {
                scanButton.forceActiveFocus();
            }
            spacing: 12

            HnSeparator {
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true

                Controls.ToolButton {
                    id: scanButton
                    objectName: "bluetoothScan"
                    Layout.fillWidth: true
                    icon.name: "view-refresh-symbolic"
                    text: BluetoothService.scanning ? qsTr("Stop scanning") : qsTr("Start scanning")
                    enabled: !!BluetoothService.selectedAdapter.powered && !BluetoothService.selectedAdapter.busy
                    onClicked: BluetoothService.scan(!BluetoothService.scanning)
                }

                Rectangle {
                    implicitWidth: 1
                    implicitHeight: 32
                    color: HoloniightPalette.borderPassive
                }

                Controls.ToolButton {
                    Layout.fillWidth: true
                    icon.name: "preferences-system-symbolic"
                    objectName: "bluetoothSettings"
                    text: qsTr("Bluetooth Settings")
                    onClicked: {
                        SettingsNavigationService.openPage("bluetooth");
                        StatusPopupSurface.hide();
                    }
                }
            }
        }
    }
}

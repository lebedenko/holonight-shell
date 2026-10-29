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

    readonly property real protectedSectionHeight: {
        let total = 0;
        for (let i = 0; i < sections.count; ++i) {
            const section = sections.itemAt(i);
            if (section && section.visible)
                total += (section.modelData === "Available" ? section.headingHeight : section.implicitHeight) + root.contentSpacing;
        }
        return total;
    }

    naturalWidth: 480
    scrollBody: false
    onAdapterIdChanged: expandedId = ""

    Connections {
        function onChanged() {
            if (!BluetoothService.devices.some((device) => {
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

    Repeater {
        id: sections
        model: ["Connected", "Paired", "Available"]

        delegate: ColumnLayout {
            id: section

            required property string modelData

            function syncDevices() {
                const desired = BluetoothService.devices.filter((device) => {
                    return device.section === section.modelData;
                });
                for (let i = deviceRows.count - 1; i >= 0; --i) {
                    if (!desired.some((device) => {
                        return device.id === deviceRows.get(i).record.id;
                    }))
                        deviceRows.remove(i);

                }
                // Keep surviving rows in place even when discovery changes service sorting.
                for (const device of desired) {
                    let found = -1;
                    for (let j = 0; j < deviceRows.count; ++j) {
                        if (deviceRows.get(j).record.id === device.id) {
                            found = j;
                            break;
                        }
                    }
                    if (found < 0) {
                        deviceRows.append({"record": device});
                    } else {
                        const previous = deviceRows.get(found).record;
                        const fields = ["name", "icon", "connected", "paired", "busy", "battery", "rssi"];
                        if (fields.some(field => previous[field] !== device[field]))
                            deviceRows.setProperty(found, "record", device);
                    }
                }
            }

            readonly property real headingHeight: sectionHeading.implicitHeight + spacing
            implicitHeight: headingHeight + (emptyLabel.visible ? emptyLabel.implicitHeight + spacing : 0) + deviceViewport.Layout.preferredHeight
            spacing: 8
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: modelData !== "Available" && root.viewportItem.height >= root.protectedSectionHeight ? implicitHeight : headingHeight
            Layout.maximumHeight: implicitHeight
            objectName: "section-" + modelData
            visible: !!BluetoothService.selectedAdapter.powered && (modelData === "Available" ? BluetoothService.scanning : deviceRows.count > 0)
            Component.onCompleted: syncDevices()

            ListModel {
                id: deviceRows

                dynamicRoles: true
            }

            Connections {
                function onChanged() {
                    section.syncDevices();
                }

                target: BluetoothService
            }

            RowLayout {
                id: sectionHeading
                Layout.fillWidth: true

                HnLabel {
                    Layout.fillWidth: true
                    role: HnTypographyRole.MicroHeader
                    color: HoloniightPalette.accentBlue
                    font.letterSpacing: 2
                    rawText: section.modelData === "Connected" ? qsTr("CONNECTED") : section.modelData === "Paired" ? qsTr("PAIRED") : qsTr("AVAILABLE")
                }

                Controls.BusyIndicator {
                    visible: section.modelData === "Available"
                    running: visible && BluetoothService.scanning
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                }

                HnLabel {
                    visible: section.modelData === "Available"
                    role: HnTypographyRole.Caption
                    color: HoloniightPalette.textMuted
                    rawText: qsTr("Scanning…")
                }

            }

            HnLabel {
                id: emptyLabel
                Layout.fillWidth: true
                visible: section.modelData === "Available" && deviceRows.count === 0
                rawText: qsTr("Looking for nearby devices…")
                color: HoloniightPalette.textSecondary
            }

            Flickable {
                id: deviceViewport
                objectName: "devices-" + section.modelData
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 0
                Layout.preferredHeight: section.modelData === "Available" ? Math.min(contentHeight, 5 * 64 + 4 * deviceList.spacing) : contentHeight
                Layout.maximumHeight: Layout.preferredHeight
                contentWidth: width
                contentHeight: deviceList.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick
                interactive: contentHeight > height
                onContentHeightChanged: Qt.callLater(returnToBounds)
                Controls.ScrollBar.vertical: Controls.ScrollBar {}

                ColumnLayout {
                    id: deviceList
                    width: deviceViewport.width
                    spacing: 4
                    Repeater {
                        model: deviceRows
                        delegate: BluetoothDeviceRow {
                            Layout.fillWidth: true
                            expanded: root.expandedId === record.id
                            adapterBusy: !!BluetoothService.selectedAdapter.busy
                            onFocusEntered: {
                                if (y < deviceViewport.contentY)
                                    deviceViewport.contentY = y;
                                else if (y + height > deviceViewport.contentY + deviceViewport.height)
                                    deviceViewport.contentY = Math.min(deviceViewport.contentHeight - deviceViewport.height, y + height - deviceViewport.height);
                            }
                            onExpansionRequested: root.expandedId = expanded ? "" : record.id
                            onActionRequested: action => BluetoothService.action(record.id, action)
                        }
                    }
                }
            }

        }

    }

    header: Component {
        ColumnLayout {
            spacing: 12
            BluetoothPopupHeader { Layout.fillWidth: true }
            HnSeparator { Layout.fillWidth: true }
        }

    }

    footer: Component {
        ColumnLayout {
            spacing: 12

            HnSeparator {
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true

                Controls.ToolButton {
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

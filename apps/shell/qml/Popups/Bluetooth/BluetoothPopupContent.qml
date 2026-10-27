import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Components
import Holonight.Controls
import HolonightShell

PopupContentLayout {
    id: root
    naturalWidth: 360
    property string expandedId: ""
    header: Component {
        RowLayout {
            HnLabel {
                Layout.fillWidth: true
                role: HnTypographyRole.Title
                rawText: qsTr("Bluetooth")
            }
            Controls.Switch {
                objectName: "bluetoothPower"
                enabled: BluetoothService.adapters.length > 0 && !BluetoothService.selectedAdapter.busy
                checked: BluetoothService.selectedAdapter.powered || false
                onToggled: BluetoothService.power(checked)
            }
        }
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
        rawText: BluetoothService.errorMessage
    }
    BluetoothPairing {
        Layout.fillWidth: true
        controller: BluetoothService
    }
    Repeater {
        model: [qsTr("Connected"), qsTr("Paired"), qsTr("Available")]
        delegate: ColumnLayout {
            id: section
            required property string modelData
            Layout.fillWidth: true
            visible: BluetoothService.selectedAdapter.powered || false

            function syncDevices() {
                const desired = BluetoothService.devices.filter(device => device.section === section.modelData);
                for (let i = deviceRows.count - 1; i >= 0; --i) {
                    if (!desired.some(device => device.id === deviceRows.get(i).record.id))
                        deviceRows.remove(i);
                }
                for (let i = 0; i < desired.length; ++i) {
                    let found = -1;
                    for (let j = i; j < deviceRows.count; ++j)
                        if (deviceRows.get(j).record.id === desired[i].id) {
                            found = j;
                            break;
                        }
                    if (found < 0)
                        deviceRows.insert(i, {
                            record: desired[i]
                        });
                    else {
                        if (found !== i)
                            deviceRows.move(found, i, 1);
                        deviceRows.set(i, {
                            record: desired[i]
                        });
                    }
                }
            }
            ListModel {
                id: deviceRows
                dynamicRoles: true
            }
            Component.onCompleted: syncDevices()
            Connections {
                target: BluetoothService
                function onChanged() {
                    section.syncDevices();
                }
            }
            HnLabel {
                role: HnTypographyRole.Caption
                rawText: section.modelData
            }
            Repeater {
                model: deviceRows
                delegate: ColumnLayout {
                    id: row
                    required property var record
                    Layout.fillWidth: true
                    Controls.ItemDelegate {
                        Layout.fillWidth: true
                        icon.name: row.record.icon
                        text: row.record.name + (row.record.battery !== undefined && row.record.battery !== null ? " · " + row.record.battery + "%" : "")
                        onClicked: root.expandedId = root.expandedId === row.record.id ? "" : row.record.id
                    }
                    HnLabel {
                        Layout.fillWidth: true
                        role: HnTypographyRole.Caption
                        rawText: row.record.connected ? qsTr("Connected") : row.record.paired ? qsTr("Paired · Disconnected") : row.record.rssi !== undefined && row.record.rssi !== null ? qsTr("%1 dBm").arg(row.record.rssi) : qsTr("Signal unknown")
                    }
                    Controls.Button {
                        Layout.alignment: Qt.AlignRight
                        visible: !row.record.connected || root.expandedId === row.record.id
                        text: row.record.busy ? qsTr("Working…") : row.record.connected ? qsTr("Disconnect") : row.record.paired ? qsTr("Connect") : qsTr("Pair")
                        enabled: !row.record.busy
                        onClicked: BluetoothService.action(row.record.id, row.record.connected ? "disconnect" : row.record.paired ? "connect" : "pair")
                    }
                }
            }
        }
    }
    footer: Component {
        RowLayout {
            Controls.Button {
                text: BluetoothService.scanning ? qsTr("Stop scan") : qsTr("Start scan")
                enabled: BluetoothService.selectedAdapter.powered || false
                onClicked: BluetoothService.scan(!BluetoothService.scanning)
            }
            Controls.Button {
                text: qsTr("Bluetooth Settings")
                onClicked: {
                    SettingsNavigationService.openPage("bluetooth");
                    StatusPopupSurface.hide();
                }
            }
        }
    }
}

import Holonight.Core
import HolonightShell
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts

RowLayout {
    spacing: 16
    implicitHeight: 72

    Rectangle {
        Layout.preferredWidth: 56
        Layout.preferredHeight: 56
        radius: 28
        color: Qt.alpha(HoloniightPalette.accentBlue, 0.16)

        HnIcon {
            anchors.centerIn: parent
            name: "bluetooth-symbolic"
            size: 32
            normalColor: HoloniightPalette.accentBlue
        }

    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 4

        HnLabel {
            Layout.fillWidth: true
            role: HnTypographyRole.Heading
            font.weight: Font.Medium
            rawText: qsTr("Bluetooth")
        }

        HnLabel {
            property int count: BluetoothService.devices.filter((device) => {
                return device.connected;
            }).length

            objectName: "bluetoothSummary"
            Layout.fillWidth: true
            role: HnTypographyRole.Caption
            color: HoloniightPalette.textSecondary
            rawText: !BluetoothService.available ? qsTr("Service unavailable") : !BluetoothService.adapters.length ? qsTr("No adapter") : !BluetoothService.selectedAdapter.powered ? qsTr("Bluetooth is off") : count === 1 ? qsTr("1 device connected") : qsTr("%1 devices connected").arg(count)
        }

    }

    Controls.Switch {
        id: powerSwitch
        objectName: "bluetoothPower"
        Accessible.name: qsTr("Bluetooth power")
        enabled: BluetoothService.available && BluetoothService.adapters.length > 0 && !BluetoothService.selectedAdapter.busy
        checked: BluetoothService.selectedAdapter.powered || false
        onToggled: BluetoothService.power(checked)
        Binding {
            target: powerSwitch
            property: "sizeRole"
            value: HnControlSize.Hero
            when: "sizeRole" in powerSwitch
        }
    }

}

import QtQuick
import HolonightShell
import Holonight.Core
import Holonight.Controls
import Holonight.Components

BarSection {
    id: root
    required property string barMonitorName
    visible: BluetoothService.adapters.length > 0
    implicitWidth: visible ? 44 : 0
    enabled: visible
    function registerAnchor() {
        const position = root.mapToGlobal(0, 0);
        BluetoothService.registerAnchor(root.barMonitorName, Math.round(position.x), Math.round(root.width));
    }
    onWidthChanged: Qt.callLater(registerAnchor)
    Component.onCompleted: Qt.callLater(registerAnchor)
    Component.onDestruction: BluetoothService.unregisterAnchor(root.barMonitorName)
    Timer {
        interval: 1000
        repeat: true
        running: root.visible
        onTriggered: root.registerAnchor()
    }
    ExternalIcon {
        id: icon
        anchors.centerIn: parent
        iconSize: 22
        iconName: "bluetooth-symbolic"
        tintColor: BluetoothService.connectedCount > 0 ? HoloniightPalette.primary : BluetoothService.selectedAdapter.powered ? HoloniightPalette.textPrimary : HoloniightPalette.textMuted
    }
    Rectangle {
        visible: BluetoothService.connectedCount > 0
        anchors.right: icon.right
        anchors.top: icon.top
        width: BluetoothService.connectedCount > 1 ? 16 : 7
        height: width
        radius: width / 2
        color: HoloniightPalette.primary
        HnLabel {
            anchors.centerIn: parent
            visible: BluetoothService.connectedCount > 1
            rawText: BluetoothService.connectedCount.toString()
            role: HnTypographyRole.Caption
            color: HoloniightPalette.onPrimary
        }
    }
    BarTooltipArea {
        barMonitorName: root.barMonitorName
        title: qsTr("Bluetooth")
        description: BluetoothService.tooltip
    }
    StatusPopupTriggerArea {
        popupId: "bluetooth"
        barMonitorName: root.barMonitorName
    }
}

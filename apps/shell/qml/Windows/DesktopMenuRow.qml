import QtQuick
import Holonight.Core
import Holonight.Components

Rectangle {
    id: root
    required property string label
    required property string iconName
    property bool highlighted: false
    property bool submenu: false
    signal triggered()
    signal hovered()
    width: parent ? parent.width : 212
    height: 32
    radius: 4
    color: highlighted && enabled ? Qt.rgba(HoloniightPalette.textPrimary.r, HoloniightPalette.textPrimary.g, HoloniightPalette.textPrimary.b, 0.08) : "transparent"
    opacity: enabled ? 1 : 0.4
    Accessible.role: Accessible.MenuItem
    Accessible.name: label
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onEntered: root.hovered()
        onClicked: root.triggered()
    }
    HnIcon {
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        size: 16
        source: "qrc:/HolonightShell/bar-icons/" + root.iconName + ".svg"
        normalColor: HoloniightPalette.textPrimary
    }
    Text {
        anchors.left: parent.left
        anchors.leftMargin: 32
        anchors.verticalCenter: parent.verticalCenter
        text: root.label
        color: HoloniightPalette.textPrimary
        font.pixelSize: 13
    }
    Text {
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        visible: root.submenu
        text: "▸"
        color: HoloniightPalette.textPrimary
    }
}

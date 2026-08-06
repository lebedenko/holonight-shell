pragma ComponentBehavior: Bound
import QtQuick
import Holonight.Core

import Holonight.Components

Item {
    id: root

    required property int itemId
    required property string label
    required property string type
    required property string iconName
    required property bool itemEnabled
    required property bool itemVisible
    required property bool hasSubmenu
    required property string toggleType
    required property int toggleState

    readonly property bool isSeparator: root.type === "separator"
    readonly property bool isChecked: root.toggleState === 1

    implicitHeight: root.itemVisible ? (root.isSeparator ? 10 : 30) : 0
    implicitWidth: parent ? parent.width : 228
    visible: root.itemVisible
    Accessible.role: Accessible.MenuItem
    Accessible.name: root.label
    Accessible.description: root.itemEnabled ? root.label : root.label + " disabled"
    Accessible.ignored: !root.itemVisible || root.isSeparator

    // Separator
    Rectangle {
        anchors {
            left: parent.left
            right: parent.right
            leftMargin: 10
            rightMargin: 10
            verticalCenter: parent.verticalCenter
        }
        height: 1
        color: HoloniightPalette.borderPassive
        opacity: 0.45
        visible: root.isSeparator
    }

    // Normal item background
    Rectangle {
        anchors.fill: parent
        radius: 4
        color: itemHover.hovered && root.itemEnabled
            ? Qt.rgba(HoloniightPalette.textPrimary.r, HoloniightPalette.textPrimary.g,
                      HoloniightPalette.textPrimary.b, 0.06)
            : "transparent"
        visible: !root.isSeparator

        Behavior on color {
            ColorAnimation { duration: 80; easing.type: Easing.OutCubic }
        }
    }

    // Toggle indicator (checkbox or radio dot)
    Rectangle {
        id: toggleDot
        width: 6
        height: 6
        radius: root.toggleType === "radio" ? 3 : 1
        color: HoloniightPalette.accentCyan
        visible: !root.isSeparator && root.toggleType !== "" && root.isChecked
        anchors {
            left: parent.left
            leftMargin: 8
            verticalCenter: parent.verticalCenter
        }
    }

    // Icon
    // qmllint disable import unresolved-type
    HnIcon {
        id: itemIcon
        size: 16
        source: root.iconName
        tinted: true
        iconState: root.itemEnabled ? HnIcon.Normal : HnIcon.Disabled
        normalColor: HoloniightPalette.textPrimary
        disabledColor: HoloniightPalette.textSecondary
        visible: !root.isSeparator && root.iconName !== ""
        opacity: root.itemEnabled ? 1.0 : 0.4
        anchors {
            left: parent.left
            leftMargin: 24
            verticalCenter: parent.verticalCenter
        }
    }
    // qmllint enable import unresolved-type

    // Label
    Text {
        id: itemLabel
        text: root.label
        color: root.itemEnabled ? HoloniightPalette.textPrimary : HoloniightPalette.textSecondary
        opacity: root.itemEnabled ? 1.0 : 0.58
        font.pixelSize: 13
        visible: !root.isSeparator
        elide: Text.ElideRight
        anchors {
            left: root.iconName !== "" ? itemIcon.right : parent.left
            leftMargin: root.iconName !== "" ? 8 : 30
            right: root.hasSubmenu ? submenuArrow.left : parent.right
            rightMargin: 8
            verticalCenter: parent.verticalCenter
        }
    }

    // Submenu arrow
    Text {
        id: submenuArrow
        text: "›"
        color: HoloniightPalette.textSecondary
        font.pixelSize: 16
        opacity: 0.8
        visible: !root.isSeparator && root.hasSubmenu
        anchors {
            right: parent.right
            rightMargin: 8
            verticalCenter: parent.verticalCenter
        }
    }

    HoverHandler { id: itemHover }

    signal clicked()
    signal submenuRequested(real anchorY)

    MouseArea {
        anchors.fill: parent
        enabled: !root.isSeparator && root.itemEnabled
        onClicked: {
            if (root.hasSubmenu) {
                root.submenuRequested(root.mapToItem(null, 0, 0).y)
            } else {
                root.clicked()
            }
        }
    }
}

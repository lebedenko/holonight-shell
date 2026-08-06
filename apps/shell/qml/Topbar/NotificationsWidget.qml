import QtQuick
import QtQuick.Effects
import HolonightShell
import Holonight.Core

import "../Controls"

BarSection {
    id: root

    required property string barMonitorName

    readonly property int unreadCount: NotificationService.unreadCount
    readonly property string tooltipAppNames: NotificationService.unreadAppNames

    implicitWidth: 40

    scale: tapHandler.pressed ? 0.92 : (hoverHandler.hovered ? 1.04 : 1.0)
    transformOrigin: Item.Center

    Behavior on scale {
        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }

    opacity: root.unreadCount > 0 ? 1.0 : 0.55

    Behavior on opacity {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    Rectangle {
        anchors.centerIn: parent
        width: parent.width + 16
        height: 34
        radius: 7
        color: Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                       HoloniightPalette.surface.b,
                       hoverHandler.hovered ? 0.38 : 0.0)
        border.color: hoverHandler.hovered
            ? Qt.rgba(HoloniightPalette.borderActive.r, HoloniightPalette.borderActive.g,
                      HoloniightPalette.borderActive.b, 0.06)
            : "transparent"
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
        }

        Behavior on border.color {
            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    BarIcon {
        id: bellIcon
        anchors.centerIn: parent
        width: 20
        height: 20
        name: "notification"
    }

    // Glow declared BEFORE the dot so the dot renders on top.
    MultiEffect {
        source: badgeDot
        anchors.fill: badgeDot
        visible: root.unreadCount > 0
        shadowEnabled: true
        shadowColor: HoloniightPalette.accentViolet
        shadowBlur: 0.7
        shadowOpacity: badgePulse.value
        shadowScale: 1.6
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    Rectangle {
        id: badgeDot
        width: 6
        height: 6
        radius: 3
        color: HoloniightPalette.accentViolet
        visible: root.unreadCount > 0
        anchors {
            right: bellIcon.right
            bottom: bellIcon.bottom
        }
    }

    SequentialAnimation {
        id: badgePulse
        running: root.unreadCount > 0
        loops: Animation.Infinite
        property real value: 0.25
        NumberAnimation {
            target: badgePulse; property: "value"
            to: 0.9; duration: 900; easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: badgePulse; property: "value"
            to: 0.25; duration: 900; easing.type: Easing.InOutSine
        }
    }

    HoverHandler { id: hoverHandler }

    TapHandler {
        id: tapHandler
        onTapped: SidebarManager.toggle(root.barMonitorName)
    }

    BarTooltipArea {
        barMonitorName: root.barMonitorName
        title: root.unreadCount > 0 ? root.unreadCount + " unread" : "Notifications"
        description: root.tooltipAppNames
        iconName: "notification"
    }
}

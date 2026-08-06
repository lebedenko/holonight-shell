import QtQuick
import QtQuick.Effects
import HolonightShell
import Holonight.Core

import "../Controls"

BarSection {
    id: root

    required property string barMonitorName
    readonly property int iconSize: 32

    readonly property string primaryIconName:
        !NetworkService.available || !NetworkService.online || NetworkService.type === 0
            ? "wifi_offline"
        : NetworkService.type === 2
            ? "network-wired-symbolic"
        : "wifi_online"
    readonly property bool alertState: !NetworkService.available || !NetworkService.online || NetworkService.type === 0
    readonly property bool activeState: NetworkService.vpnActive
    readonly property real hoverOutlineOpacity: 0.088
    readonly property real hoverShadowOpacity: 0.23

    implicitWidth: root.iconSize + 16
    scale: hoverHandler.hovered ? 1.04 : 1.0
    transformOrigin: Item.Center

    Behavior on scale {
        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }

    MultiEffect {
        source: hoverFrame
        anchors.fill: hoverFrame
        visible: hoverHandler.hovered || popupTrigger.isActivePopup
        shadowEnabled: true
        shadowColor: HoloniightPalette.accentCyan
        shadowBlur: 0.58
        shadowOpacity: popupTrigger.isActivePopup ? 0.26 : root.hoverShadowOpacity
        shadowScale: 1.02
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    Rectangle {
        id: hoverFrame
        anchors.centerIn: parent
        width: parent.width + 16
        height: 34
        radius: 7
        color: Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                       HoloniightPalette.surface.b,
                       popupTrigger.isActivePopup ? 0.6 : (hoverHandler.hovered ? 0.5 : 0.0))
        border.color: popupTrigger.isActivePopup
            ? HoloniightPalette.borderActive
            : hoverHandler.hovered
                ? Qt.rgba(HoloniightPalette.borderActive.r, HoloniightPalette.borderActive.g,
                          HoloniightPalette.borderActive.b, root.hoverOutlineOpacity)
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
        id: networkIcon
        anchors.verticalCenter: parent.verticalCenter
        width: root.iconSize
        height: root.iconSize
        name: root.primaryIconName
        glowEnabled: root.alertState || root.activeState || hoverHandler.hovered
        glowOpacity: root.alertState ? 0.28 : hoverHandler.hovered ? 0.2 : 0.22
        glowColor: root.alertState ? HoloniightPalette.error : HoloniightPalette.accentCyan
        signalStrength: NetworkService.strength
        opacity: hoverHandler.hovered ? 1.0 : 0.92

        Behavior on opacity {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    Item {
        anchors {
            right: networkIcon.right
            bottom: networkIcon.bottom
            rightMargin: -1
            bottomMargin: 0
        }
        width: 16
        height: 18
        visible: NetworkService.vpnActive
        opacity: NetworkService.vpnActive ? (hoverHandler.hovered ? 1.0 : 0.9) : 0.0
        scale: NetworkService.vpnActive ? 1.0 : 0.92

        Canvas {
            anchors.fill: parent

            readonly property color shieldColor: HoloniightPalette.accentCyan
            readonly property color innerColor: HoloniightPalette.surface

            onShieldColorChanged: requestPaint()
            onInnerColorChanged: requestPaint()

            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.lineCap = "round"
                ctx.lineJoin = "round"

                ctx.beginPath()
                ctx.moveTo(width / 2, 1.5)
                ctx.lineTo(width - 2.0, 4.0)
                ctx.lineTo(width - 2.4, 9.0)
                ctx.quadraticCurveTo(width - 2.8, 13.2, width / 2, height - 1.5)
                ctx.quadraticCurveTo(2.8, 13.2, 2.0, 9.0)
                ctx.lineTo(2.0, 4.0)
                ctx.closePath()
                ctx.fillStyle = shieldColor
                ctx.fill()

                ctx.beginPath()
                ctx.arc(width / 2, 8.0, 2.0, Math.PI, 0)
                ctx.lineTo(width / 2 + 2.0, 12.3)
                ctx.lineTo(width / 2 - 2.0, 12.3)
                ctx.closePath()
                ctx.fillStyle = innerColor
                ctx.globalAlpha = 0.86
                ctx.fill()
                ctx.globalAlpha = 1.0
            }
        }

        Behavior on opacity {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }

        Behavior on scale {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    HoverHandler { id: hoverHandler }

    BarTooltipArea {
        barMonitorName: root.barMonitorName
        title: NetworkService.online
            ? (NetworkService.type === 2 ? "Wired network" : "Wi-Fi network")
            : "Network offline"
        description: {
            if (!NetworkService.available) return "NetworkManager is unavailable."
            if (!NetworkService.online) return "No active network connection."
            if (NetworkService.type === 2) return NetworkService.vpnActive ? "Connected over Ethernet. VPN active." : "Connected over Ethernet."
            const name = NetworkService.ssid.length > 0 ? NetworkService.ssid : "Wi-Fi"
            const vpn = NetworkService.vpnActive ? " VPN active." : ""
            return name + " signal " + NetworkService.strength + "%." + vpn
        }
        iconName: root.primaryIconName
        signalStrength: NetworkService.strength
    }

    StatusPopupTriggerArea {
        id: popupTrigger
        popupId: "network"
        barMonitorName: root.barMonitorName
    }
}

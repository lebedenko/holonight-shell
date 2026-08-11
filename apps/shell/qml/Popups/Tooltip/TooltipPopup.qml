pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Effects
import HolonightShell
import Holonight.Core
import Holonight.Controls
import "../../Controls"
import "../../Topbar"

import "../../Utility" as Utility

Item {
    id: root

    Utility.AppearanceReloadBridge {}

    readonly property bool hasNamedIcon: TooltipSurface.iconName.length > 0
    readonly property bool hasAudioIcon: TooltipSurface.iconName.startsWith("audio-volume-")
    readonly property bool hasBatteryIcon: TooltipSurface.iconName.startsWith("battery_")
                                           || TooltipSurface.iconName === "battery"
    readonly property color tooltipBorderColor: Qt.rgba(HoloniightPalette.textPrimary.r,
                                                         HoloniightPalette.textPrimary.g,
                                                         HoloniightPalette.textPrimary.b, 0.10)
    readonly property color tooltipTitleColor: Qt.tint(HoloniightPalette.accentCyan,
                                                       Qt.rgba(HoloniightPalette.textPrimary.r,
                                                               HoloniightPalette.textPrimary.g,
                                                               HoloniightPalette.textPrimary.b, 0.08))
    readonly property bool hasShellIcon: root.hasAudioIcon
                                     || TooltipSurface.iconName.startsWith("wifi_")
                                     || TooltipSurface.iconName === "network-wired-symbolic"
                                     || TooltipSurface.iconName.startsWith("battery_")
                                     || TooltipSurface.iconName === "battery"
                                     || TooltipSurface.iconName === "clock"
                                     || TooltipSurface.iconName === "keyboard"
                                     || TooltipSurface.iconName === "window"
                                     || TooltipSurface.iconName === "workspace"
                                     || TooltipSurface.iconName === "system"
                                     || TooltipSurface.iconName.startsWith("system-")
                                     || TooltipSurface.iconName === "notification"

    opacity: 0
    y: 7

    Component.onCompleted: {
        opacity = 1
        y = 0
    }

    Behavior on opacity {
        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
    }

    Behavior on y {
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: "red"
        border.width: 1
        visible: AppearanceService.debugOverlays
        z: 99999
    }

    // HoloNight philosophy: geometry first, light second. The border carries the shape;
    // the glow is a soft ambient accent. Matches StatusPopup.qml glow tuning.
    MultiEffect {
        source: panel
        anchors.fill: panel
        shadowEnabled: true
        shadowColor: HoloniightPalette.accentCyan
        shadowBlur: 0.5
        shadowOpacity: 0.16
        shadowScale: 1.01
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    HnSurfaceFrame {
        id: panel
        anchors.fill: parent
        anchors.margins: 16
        surfaceRole: HnSurfaceRole.Tooltip
        fillColor: HoloniightPalette.surfaceRaised
        borderColor: root.tooltipBorderColor
        borderWidth: HnMetrics.borderWidth
    }

    Row {
        anchors {
            left: panel.left
            right: panel.right
            top: panel.top
            bottom: panel.bottom
            leftMargin: 12
            rightMargin: 16
            topMargin: 14
            bottomMargin: 16
        }
        spacing: 10

        Item {
            id: iconFrame

            width: 28
            height: 28
            anchors.verticalCenter: parent.verticalCenter

            BarIcon {
                objectName: "tooltipShellIcon"
                anchors.centerIn: parent
                width: parent.width
                height: root.hasBatteryIcon ? 14 : parent.height
                name: TooltipSurface.iconName
                glyphScale: root.hasAudioIcon ? 1.08 : 1.0
                glowEnabled: true
                glowOpacity: root.hasAudioIcon ? 0.5 : 0.42
                batteryPercent: TooltipSurface.batteryPercent
                charging: TooltipSurface.charging
                signalStrength: TooltipSurface.signalStrength
                visible: root.hasNamedIcon && root.hasShellIcon
            }

            Image {
                id: trayIconImage

                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                smooth: true
                source: root.hasNamedIcon && !root.hasShellIcon ? "image://icon/" + TooltipSurface.iconName : ""
                visible: status === Image.Ready
            }

            AppWindowIcon {
                width: 24
                height: 24
                anchors.centerIn: parent
                visible: !root.hasNamedIcon || (root.hasNamedIcon && !root.hasShellIcon
                         && trayIconImage.status === Image.Error)
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - iconFrame.width - parent.spacing
            spacing: 3

            Controls.Label {
                width: parent.width
                text: TooltipSurface.title
                color: root.tooltipTitleColor
                elide: Text.ElideRight
                maximumLineCount: 1
                font.weight: Font.DemiBold
            }

            Controls.Label {
                width: parent.width
                text: TooltipSurface.description
                color: HoloniightPalette.textSecondary
                opacity: 0.78
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                font.pointSize: 9
                lineHeight: 0.92
            }
        }
    }
}

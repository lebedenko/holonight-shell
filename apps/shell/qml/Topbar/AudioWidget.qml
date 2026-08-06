import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Effects
import HolonightShell
import Holonight.Core

import "../Controls"

BarSection {
    id: root

    required property string barMonitorName
    readonly property int stepSize: 5
    readonly property int iconSize: 32
    readonly property int iconTextSpacing: 6
    readonly property font labelFont: themeFont.font
    readonly property string volumeIconName: AudioService.muted
        ? "audio-volume-muted"
        : AudioService.volume >= 67
            ? "audio-volume-high"
        : AudioService.volume >= 34
            ? "audio-volume-medium"
            : "audio-volume-low"

    Controls.Control {
        id: themeFont
        visible: false
    }

    TextMetrics {
        id: labelMetrics
        font: root.labelFont
        text: "100%"
    }

    implicitWidth: AudioService.available
        ? iconSize + iconTextSpacing + labelMetrics.width + 16
        : 0
    visible: implicitWidth > 0
    enabled: AudioService.available

    Behavior on implicitWidth {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

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
        shadowOpacity: 0.26
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
                          HoloniightPalette.borderActive.b, 0.1)
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
        id: volumeIcon
        anchors.verticalCenter: parent.verticalCenter
        width: root.iconSize
        height: root.iconSize
        name: root.volumeIconName
        glowEnabled: AudioService.muted || hoverHandler.hovered
        glowOpacity: AudioService.muted ? 0.28 : 0.2
        glowColor: AudioService.muted ? HoloniightPalette.error : HoloniightPalette.accentCyan
        bodyOpacity: AudioService.muted
            ? (hoverHandler.hovered ? 0.6 : 0.52)
            : 1.0
        opacity: hoverHandler.hovered || AudioService.muted ? 1.0 : 0.92

        Behavior on bodyOpacity {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }

        Behavior on opacity {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    Text {
        anchors {
            left: volumeIcon.right
            leftMargin: root.iconTextSpacing
            verticalCenter: parent.verticalCenter
        }
        width: labelMetrics.width
        text: AudioService.muted ? "" : displayVolume + "%"
        color: HoloniightPalette.textPrimary
        font: root.labelFont

        property int displayVolume: AudioService.volume
        Behavior on displayVolume {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        opacity: hoverHandler.hovered ? 1.0 : 0.88

        Behavior on opacity {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    HoverHandler { id: hoverHandler }

    onWheeled: (delta) => {
        if (delta > 0)
            AudioService.setVolume(Math.min(100, AudioService.volume + stepSize))
        else
            AudioService.setVolume(Math.max(0, AudioService.volume - stepSize))
    }

    BarTooltipArea {
        barMonitorName: root.barMonitorName
        title: AudioService.muted ? "Audio muted" : "Audio"
        description: AudioService.muted
            ? "Output is muted. Scroll to adjust volume in " + root.stepSize + "% steps."
            : "Output volume " + AudioService.volume + "%. Scroll to adjust in " + root.stepSize + "% steps."
        iconName: root.volumeIconName
    }

    StatusPopupTriggerArea {
        id: popupTrigger
        popupId: "audio"
        barMonitorName: root.barMonitorName
    }
}

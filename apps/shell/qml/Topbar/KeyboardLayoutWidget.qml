import QtQuick
import QtQuick.Controls as Controls
import HolonightShell
import Holonight.Core

BarSection {
    id: root

    required property string barMonitorName
    readonly property font labelFont: themeFont.font

    Controls.Control {
        id: themeFont
        visible: false
    }

    TextMetrics {
        id: labelMetrics
        font: root.labelFont
        text: "WW"
    }

    implicitWidth: KeyboardLayoutService.layoutCode.length > 0 ? labelMetrics.width + 16 : 0
    visible: implicitWidth > 0
    enabled: KeyboardLayoutService.layoutCode.length > 0

    Behavior on implicitWidth {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    Rectangle {
        anchors.centerIn: parent
        width: parent.width + 16
        height: 34
        radius: 7
        color: Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                       HoloniightPalette.surface.b,
                       popupTrigger.isActivePopup ? 0.6 : (hoverHandler.hovered ? 0.38 : 0.0))
        border.color: popupTrigger.isActivePopup
            ? HoloniightPalette.borderActive
            : hoverHandler.hovered
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

    Text {
        anchors.centerIn: parent
        width: labelMetrics.width
        horizontalAlignment: Text.AlignHCenter
        text: KeyboardLayoutService.layoutCode
        color: HoloniightPalette.textPrimary
        font: root.labelFont
        opacity: hoverHandler.hovered ? 1.0 : 0.9

        Behavior on opacity {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    HoverHandler { id: hoverHandler }

    BarTooltipArea {
        barMonitorName: root.barMonitorName
        title: "Keyboard layout"
        description: "Current input layout: " + KeyboardLayoutService.layoutCode.toUpperCase() + "."
        iconName: "keyboard"
    }

    StatusPopupTriggerArea {
        id: popupTrigger
        popupId: "keyboard-layout"
        barMonitorName: root.barMonitorName
    }
}

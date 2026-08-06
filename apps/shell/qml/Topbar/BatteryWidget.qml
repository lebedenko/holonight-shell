import QtQuick
import QtQuick.Effects
import HolonightShell
import Holonight.Core

BarSection {
    id: root

    required property string barMonitorName
    readonly property int iconWidth: 40
    readonly property int iconHeight: 24
    readonly property bool low: BatteryService.percent < 20 && BatteryService.discharging
    readonly property string tooltipTitle: BatteryService.charging
        ? "Charging"
        : BatteryService.fullyCharged
            ? "Charged"
            : root.low
                ? "Low battery"
                : "Battery"
    readonly property string tooltipDescription: BatteryService.charging || BatteryService.fullyCharged
        ? BatteryService.percent + "% charged"
        : BatteryService.discharging
            ? BatteryService.percent + "% remaining"
            : BatteryService.percent + "% charge"

    implicitWidth: BatteryService.present ? iconWidth + 16 : 0
    visible: implicitWidth > 0
    enabled: BatteryService.present

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

    BatteryIndicator {
        anchors.verticalCenter: parent.verticalCenter
        width: root.iconWidth
        height: root.iconHeight
        percent: BatteryService.percent
        charging: BatteryService.charging
        discharging: BatteryService.discharging
        fullyCharged: BatteryService.fullyCharged
        present: BatteryService.present
        opacity: hoverHandler.hovered ? 1.0 : 0.92

        Behavior on opacity {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    HoverHandler { id: hoverHandler }

    BarTooltipArea {
        barMonitorName: root.barMonitorName
        title: root.tooltipTitle
        description: root.tooltipDescription
        iconName: root.low ? "battery_low" : "battery"
        batteryPercent: BatteryService.percent
        charging: BatteryService.charging
    }

    StatusPopupTriggerArea {
        id: popupTrigger
        popupId: "battery"
        barMonitorName: root.barMonitorName
    }
}

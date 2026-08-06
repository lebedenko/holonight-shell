pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell

Item {
    id: root

    required property string barMonitorName
    property string title
    property string description
    property string iconName
    property int batteryPercent: 100
    property bool charging: false
    property int signalStrength: 100
    property int showDelay: 450
    property bool anchorToPointerX: false
    property bool blockRetriggerUntilExit: true
    property bool tooltipTriggeredDuringHover: false

    anchors.fill: parent

    HoverHandler {
        id: hoverHandler
        onHoveredChanged: {
            if (hovered) {
                root.scheduleTooltip()
            } else {
                showTimer.stop()
                TooltipSurface.hide()
                root.tooltipTriggeredDuringHover = false
            }
        }
    }

    onEnabledChanged: {
        if (enabled && hoverHandler.hovered) {
            root.scheduleTooltip()
        } else {
            showTimer.stop()
            TooltipSurface.hide()
            root.tooltipTriggeredDuringHover = false
        }
    }

    onTitleChanged: refreshVisibleTooltip()
    onDescriptionChanged: refreshVisibleTooltip()
    onIconNameChanged: refreshVisibleTooltip()
    onBatteryPercentChanged: refreshVisibleTooltip()
    onChargingChanged: refreshVisibleTooltip()
    onSignalStrengthChanged: refreshVisibleTooltip()

    Timer {
        id: showTimer
        interval: root.showDelay
        repeat: false
        onTriggered: root.showTooltip()
    }

    Connections {
        target: StatusPopupSurface
        function onPopupVisibleChanged() { root.handleOtherPopupVisibilityChanged(StatusPopupSurface.popupVisible) }
    }

    Connections {
        target: TrayMenuSurface
        function onMenuVisibleChanged() { root.handleOtherPopupVisibilityChanged(TrayMenuSurface.menuVisible) }
    }

    function anyOtherPopupVisible() {
        return StatusPopupSurface.popupVisible || TrayMenuSurface.menuVisible
    }

    function scheduleTooltip() {
        showTimer.stop()
        if (!root.enabled || !hoverHandler.hovered) {
            return
        }
        if (root.blockRetriggerUntilExit && root.tooltipTriggeredDuringHover) {
            return
        }
        if (root.anyOtherPopupVisible()) {
            root.tooltipTriggeredDuringHover = true
            return
        }
        showTimer.restart()
    }

    function handleOtherPopupVisibilityChanged(visible) {
        if (!visible || !hoverHandler.hovered) {
            return
        }
        showTimer.stop()
        TooltipSurface.hide()
        if (root.blockRetriggerUntilExit) {
            root.tooltipTriggeredDuringHover = true
        }
    }

    function showTooltip() {
        if (root.blockRetriggerUntilExit && root.tooltipTriggeredDuringHover) {
            return
        }
        if (root.anyOtherPopupVisible()) {
            root.tooltipTriggeredDuringHover = true
            return
        }
        root.showTooltipNow()
    }

    function showTooltipNow() {
        if (!root.enabled || !hoverHandler.hovered || root.title.length === 0) {
            return
        }
        if (root.anyOtherPopupVisible()) {
            root.tooltipTriggeredDuringHover = true
            return
        }

        const pointerX = hoverHandler.point.position.x
        const globalPoint = root.mapToGlobal(root.anchorToPointerX ? pointerX : 0, 0)
        const anchorWidth = root.anchorToPointerX ? 0 : Math.round(root.width)
        TooltipSurface.show(barMonitorName, Math.round(globalPoint.x), anchorWidth,
                            root.title, root.description, root.iconName, root.batteryPercent,
                            root.charging, root.signalStrength)
        root.tooltipTriggeredDuringHover = true
    }

    function dismissForClick() {
        showTimer.stop()
        TooltipSurface.hide()
        if (root.blockRetriggerUntilExit) {
            root.tooltipTriggeredDuringHover = true
        }
    }

    function refreshVisibleTooltip() {
        if (root.enabled && hoverHandler.hovered && TooltipSurface.tooltipVisible) {
            root.showTooltipNow()
        }
    }
}

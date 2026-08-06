import QtQuick
import Holonight.Core

Item {
    id: root

    property string name: ""
    property real bodyOpacity: 1.0
    property int batteryPercent: 100
    property bool charging: false
    property int signalStrength: 100

    readonly property color _cyan: HoloniightPalette.accentCyan
    readonly property color _violet: HoloniightPalette.accentViolet
    readonly property color _error: HoloniightPalette.error

    Canvas {
        id: iconCanvas

        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            root.drawBattery(ctx, width, height)
        }
    }

    onNameChanged: iconCanvas.requestPaint()
    onBatteryPercentChanged: iconCanvas.requestPaint()
    onChargingChanged: iconCanvas.requestPaint()
    on_CyanChanged: iconCanvas.requestPaint()
    on_VioletChanged: iconCanvas.requestPaint()
    on_ErrorChanged: iconCanvas.requestPaint()

    function applyGradientStroke(ctx) {
        const gradient = ctx.createLinearGradient(0, 0, 96, 48)
        gradient.addColorStop(0.0, root._cyan)
        gradient.addColorStop(1.0, root._violet)
        ctx.strokeStyle = gradient
    }

    function applyBatteryStroke(ctx, low) {
        if (low) {
            ctx.strokeStyle = root._error
        } else {
            root.applyGradientStroke(ctx)
        }
    }

    function applyBatteryFill(ctx, low) {
        if (low) {
            ctx.fillStyle = root._error
        } else {
            const gradient = ctx.createLinearGradient(14, 18, 70, 30)
            gradient.addColorStop(0.0, root._cyan)
            gradient.addColorStop(1.0, root._violet)
            ctx.fillStyle = gradient
        }
    }

    function drawBattery(ctx, width, height) {
        const low = root.name === "battery_low"
        const percent = Math.max(0, Math.min(100, root.batteryPercent))

        ctx.scale(width / 96, height / 48)

        root.roundedRectPath(ctx, 8, 12, 72, 24, 5)
        root.applyBatteryStroke(ctx, low)
        ctx.lineWidth = 2
        ctx.stroke()

        root.roundedRectPath(ctx, 82, 19, 6, 10, 2)
        ctx.fillStyle = low ? root._error : root._cyan
        ctx.fill()

        const fillWidth = Math.max(percent > 0 ? 4 : 0, 56 * percent / 100)
        if (fillWidth > 0) {
            root.roundedRectPath(ctx, 14, 18, fillWidth, 12, 3)
            root.applyBatteryFill(ctx, low)
            ctx.globalAlpha = low ? 0.9 : 0.85
            ctx.fill()
            ctx.globalAlpha = 1.0
        }

        if (low) {
            ctx.beginPath()
            ctx.moveTo(47, 16)
            ctx.lineTo(39, 28)
            ctx.lineTo(48, 28)
            ctx.lineTo(43, 38)
            ctx.lineTo(58, 22)
            ctx.lineTo(49, 22)
            ctx.closePath()
            ctx.fillStyle = root._error
            ctx.fill()
        } else if (root.charging) {
            ctx.beginPath()
            ctx.moveTo(49, 15)
            ctx.lineTo(40, 27)
            ctx.lineTo(49, 27)
            ctx.lineTo(44, 37)
            ctx.lineTo(60, 22)
            ctx.lineTo(51, 22)
            ctx.closePath()
            ctx.fillStyle = root._violet
            ctx.fill()
        }
    }

    function roundedRectPath(ctx, x, y, width, height, radius) {
        ctx.beginPath()
        ctx.moveTo(x + radius, y)
        ctx.lineTo(x + width - radius, y)
        ctx.quadraticCurveTo(x + width, y, x + width, y + radius)
        ctx.lineTo(x + width, y + height - radius)
        ctx.quadraticCurveTo(x + width, y + height, x + width - radius, y + height)
        ctx.lineTo(x + radius, y + height)
        ctx.quadraticCurveTo(x, y + height, x, y + height - radius)
        ctx.lineTo(x, y + radius)
        ctx.quadraticCurveTo(x, y, x + radius, y)
    }
}

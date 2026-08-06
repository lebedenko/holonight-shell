import QtQuick
import Holonight.Core

Item {
    id: root

    property string name: ""
    property real bodyOpacity: 1.0
    property int batteryPercent: 100
    property int signalStrength: 100

    readonly property color _cyan: HoloniightPalette.accentCyan
    readonly property color _violet: HoloniightPalette.accentViolet

    Canvas {
        id: iconCanvas

        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.scale(width / 24, height / 24)
            root.beginSessionStroke(ctx)
            root.drawSession(ctx)
        }
    }

    onNameChanged: iconCanvas.requestPaint()
    on_CyanChanged: iconCanvas.requestPaint()
    on_VioletChanged: iconCanvas.requestPaint()

    function applyGradientStroke(ctx) {
        const gradient = ctx.createLinearGradient(3, 3, 21, 21)
        gradient.addColorStop(0.0, root._cyan)
        gradient.addColorStop(1.0, root._violet)
        ctx.strokeStyle = gradient
    }

    function beginSessionStroke(ctx) {
        root.applyGradientStroke(ctx)
        ctx.lineWidth = 1.65
    }

    function drawSession(ctx) {
        if (root.name === "system-lock-screen") {
            root.roundedRectPath(ctx, 5, 10, 14, 10, 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(8, 10)
            ctx.lineTo(8, 7)
            ctx.bezierCurveTo(8, 4.8, 9.8, 3, 12, 3)
            ctx.bezierCurveTo(14.2, 3, 16, 4.8, 16, 7)
            ctx.lineTo(16, 10)
            ctx.stroke()
            return
        }

        if (root.name === "system-log-out") {
            ctx.beginPath()
            ctx.moveTo(10, 5)
            ctx.lineTo(6, 5)
            ctx.bezierCurveTo(4.9, 5, 4, 5.9, 4, 7)
            ctx.lineTo(4, 17)
            ctx.bezierCurveTo(4, 18.1, 4.9, 19, 6, 19)
            ctx.lineTo(10, 19)
            ctx.moveTo(14, 8)
            ctx.lineTo(18, 12)
            ctx.lineTo(14, 16)
            ctx.moveTo(18, 12)
            ctx.lineTo(9, 12)
            ctx.stroke()
            return
        }

        if (root.name === "system-reboot") {
            ctx.beginPath()
            ctx.arc(11.9805, 12.2693, 8, -0.7190, -5.9348, true)
            ctx.moveTo(18, 3)
            ctx.lineTo(18, 7)
            ctx.lineTo(14, 7)
            ctx.stroke()
            return
        }

        if (root.name === "system-shutdown") {
            ctx.beginPath()
            ctx.moveTo(12, 4)
            ctx.lineTo(12, 12)
            ctx.moveTo(7.5, 7.5)
            ctx.arc(12, 12.8619, 7, -2.2690, -7.1558, true)
            ctx.stroke()
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

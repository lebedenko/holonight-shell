import QtQuick
import Holonight.Core

Item {
    id: root

    property string name: ""
    property real bodyOpacity: 1.0
    property int batteryPercent: 100
    property int signalStrength: 100
    readonly property int signalBars: {
        if (root.name === "wifi_offline") {
            return 0
        }
        const strength = Math.max(0, Math.min(100, root.signalStrength))
        return strength === 0 ? 0 : Math.ceil(strength / 25)
    }

    readonly property color _cyan: HoloniightPalette.accentCyan
    readonly property color _violet: HoloniightPalette.accentViolet
    readonly property color _muted: HoloniightPalette.borderPassive
    readonly property color _error: HoloniightPalette.error

    Canvas {
        id: iconCanvas

        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            if (root.name === "network-wired-symbolic") {
                ctx.scale(width / 24, height / 24)
                root.drawWired(ctx)
                return
            }

            ctx.scale(width / 32, height / 32)
            root.drawWifi(ctx, root.name === "wifi_offline")
        }
    }

    onNameChanged: iconCanvas.requestPaint()
    onSignalStrengthChanged: iconCanvas.requestPaint()
    on_CyanChanged: iconCanvas.requestPaint()
    on_VioletChanged: iconCanvas.requestPaint()
    on_MutedChanged: iconCanvas.requestPaint()
    on_ErrorChanged: iconCanvas.requestPaint()

    function applyGradientStroke(ctx, fromX, fromY, toX, toY) {
        const gradient = ctx.createLinearGradient(fromX, fromY, toX, toY)
        gradient.addColorStop(0.0, root._cyan)
        gradient.addColorStop(1.0, root._violet)
        ctx.strokeStyle = gradient
    }

    function drawWifiArc(ctx, x1, y1, controlX1, controlY1, controlX2, controlY2, x2, y2, offline, active) {
        ctx.beginPath()
        ctx.moveTo(x1, y1)
        ctx.bezierCurveTo(controlX1, controlY1, controlX2, controlY2, x2, y2)
        if (offline) {
            ctx.strokeStyle = root._muted
        } else if (!active) {
            ctx.strokeStyle = root._muted
            ctx.globalAlpha = 0.2
        } else {
            root.applyGradientStroke(ctx, 0, 0, 32, 32)
        }
        ctx.lineWidth = 1.7
        ctx.stroke()
        ctx.globalAlpha = 1.0
    }

    function drawWifi(ctx, offline) {
        const bars = root.signalBars

        root.drawWifiArc(ctx, 6, 12, 11.8, 7.2, 20.2, 7.2, 26, 12, offline, bars >= 4)
        root.drawWifiArc(ctx, 10, 16, 13.4, 13.1, 18.6, 13.1, 22, 16, offline, bars >= 3)
        root.drawWifiArc(ctx, 14, 20, 15.1, 18.9, 16.9, 18.9, 18, 20, offline, bars >= 2)

        ctx.beginPath()
        ctx.arc(16, 24, 1.75, 0, Math.PI * 2)
        ctx.fillStyle = offline || bars < 1 ? root._muted : root._cyan
        ctx.globalAlpha = offline || bars < 1 ? 0.2 : 1.0
        ctx.fill()
        ctx.globalAlpha = 1.0

        if (offline) {
            ctx.beginPath()
            ctx.moveTo(7.5, 25)
            ctx.lineTo(25, 7.5)
            ctx.strokeStyle = root._error
            ctx.lineWidth = 2.1
            ctx.stroke()
        }
    }

    function drawWired(ctx) {
        root.applyGradientStroke(ctx, 3, 3, 21, 21)
        ctx.lineWidth = 1.65

        ctx.beginPath()
        ctx.moveTo(7, 8)
        ctx.lineTo(7, 5)
        ctx.lineTo(17, 5)
        ctx.lineTo(17, 8)
        ctx.moveTo(12, 8)
        ctx.lineTo(12, 13)
        ctx.stroke()

        root.roundedRectPath(ctx, 8, 13, 8, 6, 1.5)
        ctx.stroke()

        ctx.beginPath()
        ctx.moveTo(10, 16)
        ctx.lineTo(14, 16)
        ctx.stroke()
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

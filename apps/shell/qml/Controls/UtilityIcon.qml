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
            root.drawUtility(ctx)
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

    function drawUtility(ctx) {
        if (root.name === "clock") {
            ctx.beginPath()
            ctx.arc(12, 12, 8, 0, Math.PI * 2)
            ctx.moveTo(12, 7)
            ctx.lineTo(12, 12)
            ctx.lineTo(16, 14)
            ctx.stroke()
            return
        }

        if (root.name === "keyboard") {
            root.roundedRectPath(ctx, 3.5, 6.5, 17, 11, 2)
            ctx.stroke()
            ctx.lineWidth = 1.2
            for (let row = 0; row < 2; ++row) {
                for (let col = 0; col < 4; ++col) {
                    ctx.beginPath()
                    ctx.moveTo(6 + col * 3.2, 10 + row * 3)
                    ctx.lineTo(6.8 + col * 3.2, 10 + row * 3)
                    ctx.stroke()
                }
            }
            ctx.beginPath()
            ctx.moveTo(8, 16)
            ctx.lineTo(16, 16)
            ctx.stroke()
            return
        }

        if (root.name === "window") {
            root.roundedRectPath(ctx, 4, 5, 16, 14, 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(4, 9)
            ctx.lineTo(20, 9)
            ctx.stroke()
            return
        }

        if (root.name === "workspace") {
            root.roundedRectPath(ctx, 4, 6, 7, 5, 1.5)
            ctx.stroke()
            root.roundedRectPath(ctx, 13, 6, 7, 5, 1.5)
            ctx.stroke()
            root.roundedRectPath(ctx, 4, 13, 7, 5, 1.5)
            ctx.stroke()
            root.roundedRectPath(ctx, 13, 13, 7, 5, 1.5)
            ctx.stroke()
            return
        }

        if (root.name === "notification") {
            // Bell body: semicircle top + flared sides + flat bottom
            ctx.beginPath()
            ctx.moveTo(6, 10)
            ctx.arc(12, 10, 6, Math.PI, 0, false)
            ctx.lineTo(18, 14)
            ctx.lineTo(20, 17)
            ctx.lineTo(4, 17)
            ctx.lineTo(6, 14)
            ctx.closePath()
            ctx.stroke()
            // Clapper bar
            ctx.beginPath()
            ctx.moveTo(10, 20)
            ctx.lineTo(14, 20)
            ctx.stroke()
            return
        }

        if (root.name === "brightness") {
            // Sun: a stroked core plus eight rays, kept inside the same 24x24 box as every other
            // utility glyph so it lines up with them at any icon size.
            ctx.beginPath()
            ctx.arc(12, 12, 4.2, 0, Math.PI * 2)
            ctx.stroke()
            for (let ray = 0; ray < 8; ++ray) {
                const angle = ray * Math.PI / 4
                const cos = Math.cos(angle)
                const sin = Math.sin(angle)
                ctx.beginPath()
                ctx.moveTo(12 + cos * 6.8, 12 + sin * 6.8)
                ctx.lineTo(12 + cos * 9.2, 12 + sin * 9.2)
                ctx.stroke()
            }
            return
        }

        ctx.beginPath()
        ctx.arc(12, 12, 7.5, 0, Math.PI * 2)
        ctx.moveTo(12, 4.5)
        ctx.lineTo(12, 19.5)
        ctx.moveTo(4.5, 12)
        ctx.lineTo(19.5, 12)
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

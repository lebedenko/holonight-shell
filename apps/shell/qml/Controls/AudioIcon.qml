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
    readonly property color _error: HoloniightPalette.error

    Canvas {
        id: iconCanvas

        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.scale(width / 32, height / 32)

            root.drawSpeaker(ctx)

            if (root.name === "audio-volume-muted") {
                ctx.beginPath()
                ctx.moveTo(20.5, 12.5)
                ctx.lineTo(27, 19)
                ctx.moveTo(27, 12.5)
                ctx.lineTo(20.5, 19)
                ctx.strokeStyle = root._error
                ctx.lineWidth = 2.1
                ctx.stroke()
                return
            }

            if (root.name === "audio-volume-low") {
                root.drawAudioWave(ctx, 18.5, 13.2, 19.6, 14.5, 17.5, 18.8)
                return
            }

            if (root.name === "audio-volume-medium") {
                root.drawAudioWave(ctx, 18.2, 13.2, 19.4, 14.6, 17.4, 18.8)
                root.drawAudioWave(ctx, 21.6, 10.8, 24.1, 13.8, 18.2, 21.2)
                return
            }

            root.drawAudioWave(ctx, 18.1, 13.2, 19.3, 14.6, 17.4, 18.8)
            root.drawAudioWave(ctx, 21.4, 10.8, 24, 13.8, 18.2, 21.2)
            root.drawAudioWave(ctx, 24.7, 8.2, 28.6, 12.7, 19.3, 23.8)
        }
    }

    onNameChanged: iconCanvas.requestPaint()
    onBodyOpacityChanged: iconCanvas.requestPaint()
    on_CyanChanged: iconCanvas.requestPaint()
    on_VioletChanged: iconCanvas.requestPaint()
    on_ErrorChanged: iconCanvas.requestPaint()

    function applyGradientStroke(ctx, fromX, fromY, toX, toY) {
        const gradient = ctx.createLinearGradient(fromX, fromY, toX, toY)
        gradient.addColorStop(0.0, root._cyan)
        gradient.addColorStop(1.0, root._violet)
        ctx.strokeStyle = gradient
    }

    function drawSpeaker(ctx) {
        const previousAlpha = ctx.globalAlpha
        ctx.globalAlpha = previousAlpha * root.bodyOpacity

        ctx.beginPath()
        ctx.moveTo(4.5, 13.5)
        ctx.lineTo(8.2, 13.5)
        ctx.lineTo(13, 9.4)
        ctx.lineTo(13, 22.6)
        ctx.lineTo(8.2, 18.5)
        ctx.lineTo(4.5, 18.5)
        ctx.closePath()
        root.applyGradientStroke(ctx, 0, 0, 32, 32)
        ctx.lineWidth = 1.7
        ctx.stroke()

        ctx.globalAlpha = previousAlpha
    }

    function drawAudioWave(ctx, x, y1, controlX, controlY1, controlY2, y2) {
        ctx.beginPath()
        ctx.moveTo(x, y1)
        ctx.bezierCurveTo(controlX, controlY1, controlX, controlY2, x, y2)
        root.applyGradientStroke(ctx, 0, 0, 32, 32)
        ctx.lineWidth = 1.8
        ctx.stroke()
    }
}

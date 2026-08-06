import QtQuick
import Holonight.Core

Item {
    id: root

    property string category: ""

    readonly property color _stroke: HoloniightPalette.borderActive

    width: 15
    height: 15

    Canvas {
        id: iconCanvas

        anchors.fill: parent
        opacity: 0.72

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = root._stroke
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            if (root.category === "browser")       root.drawBrowser(ctx, width, height)
            else if (root.category === "editor")   root.drawEditor(ctx, width, height)
            else if (root.category === "terminal") root.drawTerminal(ctx, width, height)
            else if (root.category === "files")    root.drawFiles(ctx, width, height)
            else if (root.category === "chat")     root.drawChat(ctx, width, height)
            else if (root.category === "music")    root.drawMusic(ctx, width, height)
            else if (root.category === "video")    root.drawVideo(ctx, width, height)
            else if (root.category === "settings") root.drawSettings(ctx, width, height)
            else                                   root.drawWindow(ctx, width, height)
        }
    }

    onCategoryChanged: iconCanvas.requestPaint()
    on_StrokeChanged:  iconCanvas.requestPaint()

    function drawBrowser(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.05
        ctx.beginPath()
        ctx.arc(8, 8, 6, 0, Math.PI * 2)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(2.5, 5.5)
        ctx.bezierCurveTo(4.5, 7, 11.5, 7, 13.5, 5.5)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(2.5, 10.5)
        ctx.bezierCurveTo(4.5, 9, 11.5, 9, 13.5, 10.5)
        ctx.stroke()
    }

    function drawEditor(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.15
        ctx.beginPath()
        ctx.moveTo(5.5, 4)
        ctx.lineTo(2, 8)
        ctx.lineTo(5.5, 12)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(10.5, 4)
        ctx.lineTo(14, 8)
        ctx.lineTo(10.5, 12)
        ctx.stroke()
    }

    function drawTerminal(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.15
        ctx.beginPath()
        ctx.moveTo(3.5, 6.5)
        ctx.lineTo(6.5, 8)
        ctx.lineTo(3.5, 9.5)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(8.5, 10.5)
        ctx.lineTo(12.5, 10.5)
        ctx.stroke()
    }

    function drawFiles(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.05
        ctx.beginPath()
        ctx.moveTo(1.5, 5)
        ctx.lineTo(1.5, 13.5)
        ctx.lineTo(14.5, 13.5)
        ctx.lineTo(14.5, 5)
        ctx.lineTo(8, 5)
        ctx.lineTo(6.5, 3)
        ctx.lineTo(1.5, 3)
        ctx.closePath()
        ctx.stroke()
    }

    function drawChat(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.05
        ctx.beginPath()
        ctx.moveTo(2, 2.5)
        ctx.quadraticCurveTo(2, 1.5, 3, 1.5)
        ctx.lineTo(13, 1.5)
        ctx.quadraticCurveTo(14, 1.5, 14, 2.5)
        ctx.lineTo(14, 9.5)
        ctx.quadraticCurveTo(14, 10.5, 13, 10.5)
        ctx.lineTo(6, 10.5)
        ctx.lineTo(3.5, 13)
        ctx.lineTo(3.5, 10.5)
        ctx.lineTo(3, 10.5)
        ctx.quadraticCurveTo(2, 10.5, 2, 9.5)
        ctx.closePath()
        ctx.stroke()
    }

    function drawMusic(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.2
        ctx.beginPath()
        ctx.moveTo(3.5, 9)
        ctx.lineTo(3.5, 12)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(8, 5.5)
        ctx.lineTo(8, 12)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(12.5, 7.5)
        ctx.lineTo(12.5, 12)
        ctx.stroke()
    }

    function drawVideo(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.05
        ctx.beginPath()
        ctx.moveTo(1.5, 3.5)
        ctx.quadraticCurveTo(1.5, 2.5, 2.5, 2.5)
        ctx.lineTo(13.5, 2.5)
        ctx.quadraticCurveTo(14.5, 2.5, 14.5, 3.5)
        ctx.lineTo(14.5, 12.5)
        ctx.quadraticCurveTo(14.5, 13.5, 13.5, 13.5)
        ctx.lineTo(2.5, 13.5)
        ctx.quadraticCurveTo(1.5, 13.5, 1.5, 12.5)
        ctx.closePath()
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(6, 5.5)
        ctx.lineTo(11.5, 8)
        ctx.lineTo(6, 10.5)
        ctx.closePath()
        ctx.stroke()
    }

    function drawSettings(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.0
        ctx.beginPath()
        ctx.arc(8, 8, 2.5, 0, Math.PI * 2)
        ctx.stroke()
        const numTeeth = 6
        const innerR = 4.2
        const outerR = 6.5
        const toothHalf = 0.38
        ctx.beginPath()
        for (let i = 0; i < numTeeth; ++i) {
            const angle = (i / numTeeth) * Math.PI * 2
            const a1 = angle - toothHalf
            const a2 = angle + toothHalf
            ctx.moveTo(8 + innerR * Math.cos(a1), 8 + innerR * Math.sin(a1))
            ctx.lineTo(8 + outerR * Math.cos(a1), 8 + outerR * Math.sin(a1))
            ctx.lineTo(8 + outerR * Math.cos(a2), 8 + outerR * Math.sin(a2))
            ctx.lineTo(8 + innerR * Math.cos(a2), 8 + innerR * Math.sin(a2))
        }
        ctx.stroke()
    }

    function drawWindow(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.05
        ctx.beginPath()
        ctx.moveTo(2.5, 3.5)
        ctx.quadraticCurveTo(2.5, 2.5, 3.5, 2.5)
        ctx.lineTo(12.5, 2.5)
        ctx.quadraticCurveTo(13.5, 2.5, 13.5, 3.5)
        ctx.lineTo(13.5, 12.5)
        ctx.quadraticCurveTo(13.5, 13.5, 12.5, 13.5)
        ctx.lineTo(3.5, 13.5)
        ctx.quadraticCurveTo(2.5, 13.5, 2.5, 12.5)
        ctx.closePath()
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(2.5, 6)
        ctx.lineTo(13.5, 6)
        ctx.stroke()
    }
}

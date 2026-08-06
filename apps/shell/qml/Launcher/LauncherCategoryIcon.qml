import QtQuick
import Holonight.Core

Item {
    id: root

    property string category: ""
    property color iconColor: HoloniightPalette.borderActive

    width: 14
    height: 14

    Canvas {
        id: iconCanvas
        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = root.iconColor
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            const cat = root.category
            if      (cat === "All")         root.drawAll(ctx, iconCanvas.width, iconCanvas.height)
            else if (cat === "Development") root.drawDevelopment(ctx, iconCanvas.width, iconCanvas.height)
            else if (cat === "Education")   root.drawEducation(ctx, iconCanvas.width, iconCanvas.height)
            else if (cat === "Games")       root.drawGames(ctx, iconCanvas.width, iconCanvas.height)
            else if (cat === "Graphics")    root.drawGraphics(ctx, iconCanvas.width, iconCanvas.height)
            else if (cat === "Internet")    root.drawInternet(ctx, iconCanvas.width, iconCanvas.height)
            else if (cat === "Multimedia")  root.drawMultimedia(ctx, iconCanvas.width, iconCanvas.height)
            else if (cat === "Office")      root.drawOffice(ctx, iconCanvas.width, iconCanvas.height)
            else if (cat === "Other")       root.drawOther(ctx, iconCanvas.width, iconCanvas.height)
            else if (cat === "Science")     root.drawScience(ctx, iconCanvas.width, iconCanvas.height)
            else if (cat === "Settings")    root.drawSettings(ctx, iconCanvas.width, iconCanvas.height)
            else if (cat === "System")      root.drawSystem(ctx, iconCanvas.width, iconCanvas.height)
        }
    }

    onCategoryChanged: iconCanvas.requestPaint()
    onIconColorChanged: iconCanvas.requestPaint()

    // All — 2×2 grid of squares
    function drawAll(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.0
        ctx.strokeRect(2, 2, 5, 5)
        ctx.strokeRect(9, 2, 5, 5)
        ctx.strokeRect(2, 9, 5, 5)
        ctx.strokeRect(9, 9, 5, 5)
    }

    // Development — </> brackets
    function drawDevelopment(ctx, w, h) {
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
        ctx.beginPath()
        ctx.moveTo(9.5, 4)
        ctx.lineTo(6.5, 12)
        ctx.stroke()
    }

    // Education — open book
    function drawEducation(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.05
        ctx.beginPath()
        ctx.moveTo(8, 3.5)
        ctx.lineTo(8, 13.5)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(8, 3.5)
        ctx.lineTo(2.5, 5)
        ctx.lineTo(2.5, 13.5)
        ctx.lineTo(8, 13.5)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(8, 3.5)
        ctx.lineTo(13.5, 5)
        ctx.lineTo(13.5, 13.5)
        ctx.lineTo(8, 13.5)
        ctx.stroke()
    }

    // Games — gamepad
    function drawGames(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.0
        ctx.beginPath()
        ctx.moveTo(3.5, 6)
        ctx.quadraticCurveTo(1.5, 6, 1.5, 8.5)
        ctx.lineTo(2, 10.5)
        ctx.quadraticCurveTo(2.5, 12, 4.5, 12)
        ctx.lineTo(5.5, 12)
        ctx.lineTo(6.5, 10)
        ctx.lineTo(9.5, 10)
        ctx.lineTo(10.5, 12)
        ctx.lineTo(11.5, 12)
        ctx.quadraticCurveTo(13.5, 12, 14, 10.5)
        ctx.lineTo(14.5, 8.5)
        ctx.quadraticCurveTo(14.5, 6, 12.5, 6)
        ctx.closePath()
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(3.5, 9)
        ctx.lineTo(5.5, 9)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(4.5, 8)
        ctx.lineTo(4.5, 10)
        ctx.stroke()
        ctx.beginPath()
        ctx.arc(11.5, 8.5, 1, 0, Math.PI * 2)
        ctx.stroke()
    }

    // Graphics — pencil
    function drawGraphics(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.05
        ctx.beginPath()
        ctx.moveTo(11, 1.5)
        ctx.lineTo(13.5, 4)
        ctx.lineTo(6, 11.5)
        ctx.lineTo(3.5, 11.5)
        ctx.lineTo(3.5, 9)
        ctx.closePath()
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(11, 1.5)
        ctx.lineTo(12.5, 3)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(3.5, 11.5)
        ctx.lineTo(2.5, 13.5)
        ctx.lineTo(4.5, 13)
        ctx.closePath()
        ctx.stroke()
    }

    // Internet — globe
    function drawInternet(ctx, w, h) {
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
        ctx.beginPath()
        ctx.moveTo(8, 2)
        ctx.lineTo(8, 14)
        ctx.stroke()
    }

    // Multimedia — music note
    function drawMultimedia(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.1
        ctx.beginPath()
        ctx.moveTo(11, 10.5)
        ctx.lineTo(11, 3.5)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(11, 3.5)
        ctx.quadraticCurveTo(14, 4.5, 13, 7.5)
        ctx.stroke()
        ctx.beginPath()
        ctx.arc(9, 11.5, 2, 0, Math.PI * 2)
        ctx.stroke()
    }

    // Office — document with folded corner
    function drawOffice(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.0
        ctx.beginPath()
        ctx.moveTo(3.5, 2)
        ctx.lineTo(9.5, 2)
        ctx.lineTo(12.5, 5)
        ctx.lineTo(12.5, 14)
        ctx.lineTo(3.5, 14)
        ctx.closePath()
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(9.5, 2)
        ctx.lineTo(9.5, 5)
        ctx.lineTo(12.5, 5)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(5.5, 7.5)
        ctx.lineTo(10.5, 7.5)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(5.5, 10)
        ctx.lineTo(10.5, 10)
        ctx.stroke()
    }

    // Other — three dots
    function drawOther(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.0
        ctx.beginPath()
        ctx.arc(3.5, 8, 1.2, 0, Math.PI * 2)
        ctx.stroke()
        ctx.beginPath()
        ctx.arc(8, 8, 1.2, 0, Math.PI * 2)
        ctx.stroke()
        ctx.beginPath()
        ctx.arc(12.5, 8, 1.2, 0, Math.PI * 2)
        ctx.stroke()
    }

    // Science — flask
    function drawScience(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.0
        ctx.beginPath()
        ctx.moveTo(6, 2)
        ctx.lineTo(10, 2)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(7, 2)
        ctx.lineTo(7, 7)
        ctx.lineTo(2.5, 13)
        ctx.quadraticCurveTo(2, 14, 3.5, 14)
        ctx.lineTo(12.5, 14)
        ctx.quadraticCurveTo(14, 14, 13.5, 13)
        ctx.lineTo(9, 7)
        ctx.lineTo(9, 2)
        ctx.stroke()
        ctx.beginPath()
        ctx.arc(7.5, 11, 0.9, 0, Math.PI * 2)
        ctx.stroke()
    }

    // Settings — gear
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
        for (let idx = 0; idx < numTeeth; ++idx) {
            const angle = (idx / numTeeth) * Math.PI * 2
            const a1 = angle - toothHalf
            const a2 = angle + toothHalf
            ctx.moveTo(8 + innerR * Math.cos(a1), 8 + innerR * Math.sin(a1))
            ctx.lineTo(8 + outerR * Math.cos(a1), 8 + outerR * Math.sin(a1))
            ctx.lineTo(8 + outerR * Math.cos(a2), 8 + outerR * Math.sin(a2))
            ctx.lineTo(8 + innerR * Math.cos(a2), 8 + innerR * Math.sin(a2))
        }
        ctx.stroke()
    }

    // System — monitor
    function drawSystem(ctx, w, h) {
        ctx.scale(w / 16, h / 16)
        ctx.lineWidth = 1.0
        ctx.beginPath()
        ctx.moveTo(1.5, 3)
        ctx.quadraticCurveTo(1.5, 2, 2.5, 2)
        ctx.lineTo(13.5, 2)
        ctx.quadraticCurveTo(14.5, 2, 14.5, 3)
        ctx.lineTo(14.5, 10)
        ctx.quadraticCurveTo(14.5, 11, 13.5, 11)
        ctx.lineTo(2.5, 11)
        ctx.quadraticCurveTo(1.5, 11, 1.5, 10)
        ctx.closePath()
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(6, 11)
        ctx.lineTo(5, 14)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(10, 11)
        ctx.lineTo(11, 14)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(4.5, 14)
        ctx.lineTo(11.5, 14)
        ctx.stroke()
    }
}

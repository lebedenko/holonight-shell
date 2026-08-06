import QtQuick
import Holonight.Core

Item {
    id: root

    property string connection: "straight"
    property int gap: 4
    property int slant: 12
    property int straightStub: 4
    property int thickness: 2
    property color leftFrameFill: Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                                          HoloniightPalette.surface.b, 1.0)
    property color rightFrameFill: Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                                           HoloniightPalette.surface.b, 1.0)
    property color leftFrameStroke: HoloniightPalette.borderPassive
    property color rightFrameStroke: HoloniightPalette.borderPassive
    property real leftFrameFillOpacity: 1.0
    property real rightFrameFillOpacity: 1.0
    property real leftFrameOpacity: 1.0
    property real rightFrameOpacity: 1.0

    implicitWidth: root.connection === "straight"
        ? root.gap + root.straightStub * 2
        : root.gap + root.slant + root.thickness * 2
    implicitHeight: 64

    Canvas {
        id: separatorCanvas

        anchors {
            fill: parent
        }
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            const top = 0
            // const bleed = 0.75
            const bleed = 0
            const left = -bleed
            const bottom = height
            const origin = root.connection === "straight" ? root.straightStub : root.slant
            const gap = root.gap
            const slant = root.slant
            const canvasRight = width + bleed

            ctx.reset()
            ctx.lineJoin = "round"
            ctx.lineCap = "round"
            ctx.lineWidth = root.thickness

            let leftTopX = origin
            let leftBottomX = origin
            let rightTopX = origin + gap
            let rightBottomX = origin + gap

            if (root.connection === "positive") {
                leftTopX = origin - gap
                leftBottomX = 0
                rightTopX = leftTopX + gap
                rightBottomX = leftBottomX + gap
            } else if (root.connection === "negative") {
                leftTopX = 0
                leftBottomX = origin - gap
                rightTopX = leftTopX + gap
                rightBottomX = leftBottomX + gap
            } else if (root.connection === "straight") {
                leftTopX = origin
                leftBottomX = origin
                rightTopX = origin + gap
                rightBottomX = rightTopX
            }

            // Draw left surface
            ctx.beginPath()
            ctx.moveTo(left, top)
            ctx.lineTo(leftTopX, top)
            ctx.lineTo(leftBottomX, bottom)
            ctx.lineTo(left, bottom)
            ctx.closePath()
            ctx.fillStyle = root.leftFrameFill
            ctx.globalAlpha = root.leftFrameFillOpacity
            ctx.fill()

            // Draw right surface
            ctx.beginPath()
            ctx.moveTo(rightTopX, top)
            ctx.lineTo(canvasRight, top)
            ctx.lineTo(canvasRight, bottom)
            ctx.lineTo(rightBottomX, bottom)
            ctx.closePath()
            ctx.fillStyle = root.rightFrameFill
            ctx.globalAlpha = root.rightFrameFillOpacity
            ctx.fill()

            // Draw left stroke
            ctx.beginPath()
            if (root.connection === "straight") {
                ctx.moveTo(left, top)
                ctx.lineTo(leftTopX, top)
                ctx.lineTo(leftBottomX, bottom)
                ctx.lineTo(left, bottom)
            } else if (root.connection === "negative") {
                ctx.moveTo(left, bottom)
                ctx.lineTo(leftBottomX, bottom)
                ctx.lineTo(leftTopX, top)
            } else {
                ctx.moveTo(left, top)
                ctx.lineTo(leftTopX, top)
                ctx.lineTo(leftBottomX, bottom)
            }
            ctx.strokeStyle = root.leftFrameStroke
            ctx.globalAlpha = root.leftFrameOpacity
            ctx.stroke()

            // Draw right stroke
            ctx.beginPath()
            if (root.connection === "straight") {
                ctx.moveTo(canvasRight, top)
                ctx.lineTo(rightTopX, top)
                ctx.lineTo(rightBottomX, bottom)
                ctx.lineTo(canvasRight, bottom)
            } else if (root.connection === "negative") {
                ctx.moveTo(rightBottomX, bottom)
                ctx.lineTo(rightTopX, top)
                ctx.lineTo(canvasRight, top)
            } else {
                ctx.moveTo(rightTopX, top)
                ctx.lineTo(rightBottomX, bottom)
                ctx.lineTo(canvasRight, bottom)
            }
            ctx.strokeStyle = root.rightFrameStroke
            ctx.globalAlpha = root.rightFrameOpacity
            ctx.stroke()
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onVisibleChanged: requestPaint()
        Component.onCompleted: requestPaint()
    }

    onConnectionChanged: separatorCanvas.requestPaint()
    onGapChanged: separatorCanvas.requestPaint()
    onSlantChanged: separatorCanvas.requestPaint()
    onLeftFrameFillChanged: separatorCanvas.requestPaint()
    onRightFrameFillChanged: separatorCanvas.requestPaint()
    onLeftFrameStrokeChanged: separatorCanvas.requestPaint()
    onRightFrameStrokeChanged: separatorCanvas.requestPaint()
    onLeftFrameFillOpacityChanged: separatorCanvas.requestPaint()
    onRightFrameFillOpacityChanged: separatorCanvas.requestPaint()
    onLeftFrameOpacityChanged: separatorCanvas.requestPaint()
    onRightFrameOpacityChanged: separatorCanvas.requestPaint()
}

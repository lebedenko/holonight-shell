import QtQuick
import Holonight.Core

Item {
    id: root

    property int frameInset: 1
    property real cornerRadius: 4
    property int leftTopOffset: 0
    property int leftBottomOffset: 0
    property int rightTopOffset: 0
    property int rightBottomOffset: 0
    property int leftCornerCut: 0
    property int rightCornerCut: 0
    property color frameFill: HoloniightPalette.surface
    property color frameStroke: HoloniightPalette.borderPassive
    property real strokeWidth: 2
    property real strokeOpacity: 0.1
    property real innerShadowOpacity: 0.34
    property real innerHighlightOpacity: 0.08
    property color innerGlowColor: HoloniightPalette.accentCyan
    property real innerGlowOpacity: 0.0
    property real topGradientOpacity: 0.035
    property real bottomGradientOpacity: 0.045

    Canvas {
        id: frameCanvas
        anchors.fill: parent
        antialiasing: true

        function drawFramePath(ctx) {
            const leftTop = root.frameInset + root.leftTopOffset
            const leftBottom = root.frameInset + root.leftBottomOffset
            const leftEdge = root.frameInset
            const top = root.frameInset
            const rightTop = width - root.frameInset - root.rightTopOffset
            const rightBottom = width - root.frameInset - root.rightBottomOffset
            const rightEdge = width - root.frameInset
            const bottom = height - root.frameInset
            const radius = root.cornerRadius

            ctx.beginPath()
            if (root.leftCornerCut > 0) ctx.moveTo(leftEdge + root.leftCornerCut, top)
            else ctx.moveTo(leftTop + radius, top)
            if (root.rightCornerCut > 0) {
                ctx.lineTo(rightEdge - root.rightCornerCut, top)
                ctx.lineTo(rightEdge, top + root.rightCornerCut)
                ctx.lineTo(rightEdge, bottom - root.rightCornerCut)
                ctx.lineTo(rightEdge - root.rightCornerCut, bottom)
            } else {
                ctx.lineTo(rightTop - radius, top)
                ctx.arcTo(rightTop, top, rightTop, top + radius, radius)
                ctx.lineTo(rightBottom, bottom - radius)
                ctx.arcTo(rightBottom, bottom, rightBottom - radius, bottom, radius)
            }
            if (root.leftCornerCut > 0) {
                ctx.lineTo(leftEdge + root.leftCornerCut, bottom)
                ctx.lineTo(leftEdge, bottom - root.leftCornerCut)
                ctx.lineTo(leftEdge, top + root.leftCornerCut)
            } else {
                ctx.lineTo(leftBottom + radius, bottom)
                ctx.arcTo(leftBottom, bottom, leftBottom, bottom - radius, radius)
                ctx.lineTo(leftTop, top + radius)
                ctx.arcTo(leftTop, top, leftTop + radius, top, radius)
            }
            ctx.closePath()
        }

        onPaint: {
            const ctx = getContext("2d")
            const top = root.frameInset
            const bottom = height - root.frameInset
            const innerShadowColor = Qt.darker(root.frameFill, 1.45)
            ctx.reset()
            ctx.lineJoin = "round"
            ctx.lineCap = "round"
            ctx.lineWidth = root.strokeWidth
            frameCanvas.drawFramePath(ctx)
            ctx.fillStyle = root.frameFill
            ctx.fill()

            ctx.save()
            frameCanvas.drawFramePath(ctx)
            ctx.clip()
            const fillGradient = ctx.createLinearGradient(0, top, 0, bottom)
            fillGradient.addColorStop(0.0, Qt.rgba(HoloniightPalette.textPrimary.r, HoloniightPalette.textPrimary.g,
                                                   HoloniightPalette.textPrimary.b, root.topGradientOpacity))
            fillGradient.addColorStop(0.5, Qt.rgba(root.frameFill.r, root.frameFill.g, root.frameFill.b, 0.0))
            fillGradient.addColorStop(1.0, Qt.rgba(innerShadowColor.r, innerShadowColor.g, innerShadowColor.b,
                                                   root.bottomGradientOpacity))
            ctx.fillStyle = fillGradient
            ctx.fillRect(0, top, width, bottom - top)
            ctx.restore()

            ctx.save()
            frameCanvas.drawFramePath(ctx)
            ctx.clip()
            ctx.translate(0, 1)
            frameCanvas.drawFramePath(ctx)
            ctx.strokeStyle = Qt.rgba(innerShadowColor.r, innerShadowColor.g, innerShadowColor.b,
                                      root.innerShadowOpacity)
            ctx.stroke()
            ctx.restore()

            ctx.save()
            frameCanvas.drawFramePath(ctx)
            ctx.clip()
            frameCanvas.drawFramePath(ctx)
            ctx.strokeStyle = Qt.rgba(HoloniightPalette.textPrimary.r, HoloniightPalette.textPrimary.g,
                                      HoloniightPalette.textPrimary.b, root.innerHighlightOpacity)
            ctx.stroke()
            ctx.restore()

            if (root.innerGlowOpacity > 0.0) {
                ctx.save()
                frameCanvas.drawFramePath(ctx)
                ctx.clip()
                ctx.lineWidth = 4
                frameCanvas.drawFramePath(ctx)
                ctx.strokeStyle = Qt.rgba(root.innerGlowColor.r, root.innerGlowColor.g, root.innerGlowColor.b,
                                          root.innerGlowOpacity)
                ctx.stroke()
                ctx.restore()
            }
            frameCanvas.drawFramePath(ctx)
            ctx.strokeStyle = root.frameStroke
            ctx.globalAlpha = root.strokeOpacity
            ctx.stroke()
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    Connections {
        target: root
        function onFrameInsetChanged() { frameCanvas.requestPaint() }
        function onCornerRadiusChanged() { frameCanvas.requestPaint() }
        function onLeftTopOffsetChanged() { frameCanvas.requestPaint() }
        function onLeftBottomOffsetChanged() { frameCanvas.requestPaint() }
        function onRightTopOffsetChanged() { frameCanvas.requestPaint() }
        function onRightBottomOffsetChanged() { frameCanvas.requestPaint() }
        function onLeftCornerCutChanged() { frameCanvas.requestPaint() }
        function onRightCornerCutChanged() { frameCanvas.requestPaint() }
        function onFrameFillChanged() { frameCanvas.requestPaint() }
        function onFrameStrokeChanged() { frameCanvas.requestPaint() }
        function onStrokeWidthChanged() { frameCanvas.requestPaint() }
        function onStrokeOpacityChanged() { frameCanvas.requestPaint() }
        function onInnerShadowOpacityChanged() { frameCanvas.requestPaint() }
        function onInnerHighlightOpacityChanged() { frameCanvas.requestPaint() }
        function onInnerGlowColorChanged() { frameCanvas.requestPaint() }
        function onInnerGlowOpacityChanged() { frameCanvas.requestPaint() }
        function onTopGradientOpacityChanged() { frameCanvas.requestPaint() }
        function onBottomGradientOpacityChanged() { frameCanvas.requestPaint() }
    }
}

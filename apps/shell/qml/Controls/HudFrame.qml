import QtQuick
import Holonight.Core
import Holonight.Controls

Item {
    id: root

    enum Variant {
        Popup,
        Section,
        Widget,
        Sidebar
    }

    property int variant: HudFrame.Section
    property int frameInset: root.variant === HudFrame.Popup ? 2 : 1
    property real cornerRadius: NaN
    property color frameFill: root.defaultFrameFill
    property color frameStroke: root.defaultFrameStroke
    property real strokeWidth: HoloniightPalette.borderWidth
    property real strokeOpacity: root.defaultStrokeOpacity
    property real innerShadowOpacity: root.variant === HudFrame.Section ? 0.34 : 0.42
    property real innerHighlightOpacity: root.variant === HudFrame.Section ? 0.08 : 0.12
    property color innerGlowColor: HoloniightPalette.accentCyan
    property real innerGlowOpacity: root.variant === HudFrame.Popup ? 0.04 : 0.0
    property real topGradientOpacity: root.defaultTopGradientOpacity
    property real bottomGradientOpacity: root.defaultBottomGradientOpacity

    readonly property int surfaceRole: {
        if (root.variant === HudFrame.Popup) return HnSurfaceRole.Popup
        if (root.variant === HudFrame.Widget) return HnSurfaceRole.Card
        if (root.variant === HudFrame.Sidebar) return HnSurfaceRole.Panel
        return HnSurfaceRole.Hud
    }
    readonly property real effectiveRadius: semanticFrame.effectiveRadius
    readonly property real effectiveChamfer: semanticFrame.effectiveChamfer
    readonly property int shapeKind: semanticFrame.shapeKind
    readonly property int chamferedCorners: semanticFrame.chamferedCorners
    readonly property string pathData: semanticFrame.pathData
    readonly property color defaultFrameFill: {
        if (root.variant === HudFrame.Popup || root.variant === HudFrame.Sidebar) {
            return HoloniightPalette.surfaceRaised
        }
        if (root.variant === HudFrame.Widget) return HoloniightPalette.surfaceElevated
        return HoloniightPalette.glassTint
    }
    readonly property color defaultFrameStroke: root.variant === HudFrame.Section
                                                  ? HoloniightPalette.borderActive
                                                  : HoloniightPalette.borderPassive
    readonly property color decorationHighlightColor: HoloniightPalette.textPrimary
    readonly property real defaultStrokeOpacity: {
        if (root.variant === HudFrame.Popup) return 0.92
        if (root.variant === HudFrame.Widget) return 0.24
        if (root.variant === HudFrame.Sidebar) return 0.34
        return 0.1
    }
    readonly property real defaultTopGradientOpacity: {
        if (root.variant === HudFrame.Popup) return 0.065
        if (root.variant === HudFrame.Widget || root.variant === HudFrame.Sidebar) return 0.05
        return 0.035
    }
    readonly property real defaultBottomGradientOpacity: {
        if (root.variant === HudFrame.Popup) return 0.075
        if (root.variant === HudFrame.Widget || root.variant === HudFrame.Sidebar) return 0.06
        return 0.045
    }

    // The installed module's generated qmltypes does not describe composite QML types; runtime import is covered by tests.
    // qmllint disable import unresolved-type
    HnSurfaceFrame {
        id: semanticFrame
        anchors.fill: parent
        anchors.margins: Math.max(0, root.frameInset)
        surfaceRole: root.surfaceRole
        cornerStyleOverride: HnCornerStyle.Inherit
        radiusOverride: root.cornerRadius
        fillColor: root.frameFill
        borderColor: Qt.rgba(root.frameStroke.r, root.frameStroke.g, root.frameStroke.b, root.strokeOpacity)
        borderWidth: root.strokeWidth
    }
    // qmllint enable import unresolved-type

    Canvas {
        id: decorationCanvas

        anchors.fill: parent
        visible: root.shapeKind === HnShapeKind.Rounded
        antialiasing: true

        function drawFramePath(ctx) {
            const left = root.frameInset
            const top = root.frameInset
            const right = width - root.frameInset
            const bottom = height - root.frameInset
            const radius = root.effectiveRadius

            ctx.beginPath()
            ctx.moveTo(left + radius, top)
            ctx.lineTo(right - radius, top)
            ctx.arcTo(right, top, right, top + radius, radius)
            ctx.lineTo(right, bottom - radius)
            ctx.arcTo(right, bottom, right - radius, bottom, radius)
            ctx.lineTo(left + radius, bottom)
            ctx.arcTo(left, bottom, left, bottom - radius, radius)
            ctx.lineTo(left, top + radius)
            ctx.arcTo(left, top, left + radius, top, radius)
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

            ctx.save()
            decorationCanvas.drawFramePath(ctx)
            ctx.clip()
            const fillGradient = ctx.createLinearGradient(0, top, 0, bottom)
            fillGradient.addColorStop(0.0, Qt.rgba(root.decorationHighlightColor.r,
                                                   root.decorationHighlightColor.g,
                                                   root.decorationHighlightColor.b,
                                                   root.topGradientOpacity))
            fillGradient.addColorStop(0.5, Qt.rgba(root.frameFill.r, root.frameFill.g, root.frameFill.b, 0.0))
            fillGradient.addColorStop(1.0, Qt.rgba(innerShadowColor.r, innerShadowColor.g, innerShadowColor.b,
                                                   root.bottomGradientOpacity))
            ctx.fillStyle = fillGradient
            ctx.fillRect(0, top, width, bottom - top)
            ctx.restore()

            ctx.save()
            decorationCanvas.drawFramePath(ctx)
            ctx.clip()
            ctx.translate(0, 1)
            decorationCanvas.drawFramePath(ctx)
            ctx.strokeStyle = Qt.rgba(innerShadowColor.r, innerShadowColor.g, innerShadowColor.b,
                                      root.innerShadowOpacity)
            ctx.stroke()
            ctx.restore()

            ctx.save()
            decorationCanvas.drawFramePath(ctx)
            ctx.clip()
            decorationCanvas.drawFramePath(ctx)
            ctx.strokeStyle = Qt.rgba(root.decorationHighlightColor.r, root.decorationHighlightColor.g,
                                      root.decorationHighlightColor.b, root.innerHighlightOpacity)
            ctx.stroke()
            ctx.restore()

            if (root.innerGlowOpacity > 0.0) {
                ctx.save()
                decorationCanvas.drawFramePath(ctx)
                ctx.clip()
                ctx.lineWidth = 4
                decorationCanvas.drawFramePath(ctx)
                ctx.strokeStyle = Qt.rgba(root.innerGlowColor.r, root.innerGlowColor.g, root.innerGlowColor.b,
                                          root.innerGlowOpacity)
                ctx.stroke()
                ctx.restore()
            }
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onVisibleChanged: requestPaint()
    }

    Connections {
        target: root
        function onEffectiveRadiusChanged() { decorationCanvas.requestPaint() }
        function onFrameInsetChanged() { decorationCanvas.requestPaint() }
        function onFrameFillChanged() { decorationCanvas.requestPaint() }
        function onDecorationHighlightColorChanged() { decorationCanvas.requestPaint() }
        function onStrokeWidthChanged() { decorationCanvas.requestPaint() }
        function onInnerShadowOpacityChanged() { decorationCanvas.requestPaint() }
        function onInnerHighlightOpacityChanged() { decorationCanvas.requestPaint() }
        function onInnerGlowColorChanged() { decorationCanvas.requestPaint() }
        function onInnerGlowOpacityChanged() { decorationCanvas.requestPaint() }
        function onTopGradientOpacityChanged() { decorationCanvas.requestPaint() }
        function onBottomGradientOpacityChanged() { decorationCanvas.requestPaint() }
    }
}

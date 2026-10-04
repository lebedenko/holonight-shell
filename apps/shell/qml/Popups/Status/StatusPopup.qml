import QtQuick
import QtQuick.Effects
import HolonightShell
import Holonight.Core
import Holonight.Controls

import "../../Utility" as Utility
import "PopupMetrics.js" as PopupMetrics

// Shared frame owns padding and publishes the loaded body's natural size to the surface host.
Item {
    id: root

    Utility.AppearanceReloadBridge {}

    // Injected from StatusPopupSurface::ensureSurface via setInitialProperties.
    required property string popupId

    readonly property var popupTitles: ({
        "audio": "Audio",
        "network": "Network",
        "battery": "Battery",
        "keyboard-layout": "Keyboard Layout"
    })
    readonly property string displayTitle: root.popupTitles[root.popupId] ?? root.popupId

    // Feature content is measured after loading; unknown IDs use the title fallback.
    readonly property var popupSources: ({
        "storage": "qrc:/HolonightShell/Popups/Storage/StoragePopupContent.qml",
        "weather": "qrc:/HolonightShell/Popups/Weather/WeatherPopupContent.qml",
        "audio": "qrc:/HolonightShell/Popups/Audio/AudioPopupContent.qml",
        "network": "qrc:/HolonightShell/Popups/Network/NetworkPopupContent.qml",
        "battery": "qrc:/HolonightShell/Popups/Battery/BatteryPopupContent.qml"
    })
    readonly property string contentSource: root.popupSources[root.popupId] ?? ""
    readonly property bool showTitle: root.contentSource.length === 0 || contentLoader.status === Loader.Error
    property bool geometryReady: false
    readonly property int contentPadding: PopupMetrics.contentInset
    readonly property Item loadedContent: contentLoader.status === Loader.Ready ? contentLoader.item as Item : null
    implicitWidth: (loadedContent ? loadedContent.implicitWidth : titleText.implicitWidth) + 2 * (glowPadding + contentPadding)
    implicitHeight: (loadedContent ? loadedContent.implicitHeight : titleText.implicitHeight) + panelTop + glowPadding + 2 * contentPadding

    readonly property int glowPadding: 24       // transparent room left/right/bottom for the glow
    readonly property int topPadding: 6         // transparent room above the notch tip
    readonly property int notchBandHeight: 9    // tip-to-panel-top distance
    readonly property int notchHalfWidth: 9
    readonly property color panelFillColor: HoloniightPalette.surfaceRaised
    readonly property color panelBorderColor: HoloniightPalette.borderPassive
    readonly property color panelAccentColor: HoloniightPalette.accentCyan
    readonly property real panelBorderWidth: HnMetrics.borderWidth

    // Panel body geometry within the (padded) surface.
    readonly property int panelLeft: root.glowPadding
    readonly property int panelTop: root.topPadding + root.notchBandHeight
    readonly property int panelRight: root.width - root.glowPadding
    readonly property int panelBottom: root.height - root.glowPadding
    readonly property real panelDrawableWidth: Math.max(0, root.panelRight - root.panelLeft - root.panelBorderWidth)
    readonly property real panelDrawableHeight: Math.max(0, root.panelBottom - root.panelTop - root.panelBorderWidth)
    readonly property var panelShapeProfile: HnAppearance.resolve(HnSurfaceRole.Popup, HnCornerStyle.Inherit,
                                                                  root.panelDrawableWidth,
                                                                  root.panelDrawableHeight,
                                                                  NaN, NaN, HnCornerMask.Inherit,
                                                                  HnAppearance.revision)
    readonly property real panelRadius: root.panelShapeProfile.radius
    readonly property real panelChamfer: root.panelShapeProfile.chamfer
    readonly property int panelChamferedCorners: root.panelShapeProfile.corners

    focus: true

    Keys.onEscapePressed: StatusPopupSurface.hide()

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: "red"
        border.width: 1
        visible: AppearanceService.debugOverlays
        z: 99999
    }

    // Entry animation: fade in while sliding up a few pixels.
    property real entryProgress: 0.0
    opacity: root.entryProgress
    transform: Translate { y: (1.0 - root.entryProgress) * 8 }
    NumberAnimation on entryProgress {
        from: 0.0
        to: 1.0
        duration: 250
        easing.type: Easing.OutCubic
        running: root.geometryReady
    }

    // Glow declared before the panel shape and labels so it renders behind them
    // (MultiEffect z-order rule from CLAUDE.md).
    //
    // HoloNight philosophy: geometry first, light second. The border carries the shape;
    // the glow is a soft ambient accent, not a neon emission. Keep shadowOpacity low and
    // the blur wide so it reads as energy around the panel rather than a bright halo.
    MultiEffect {
        source: panelShape
        anchors.fill: panelShape
        shadowEnabled: true
        shadowColor: HoloniightPalette.accentCyan
        shadowBlur: 0.5
        shadowOpacity: 0.12
        shadowScale: 1.01
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    // Whole popup outline (rounded panel + upward notch) drawn as one continuous path so the
    // notch is seamless with the border.
    Canvas {
        id: panelShape
        anchors.fill: parent

        function hasChamfer(corner) {
            return (root.panelChamferedCorners & corner) !== 0
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()

            const half = root.notchHalfWidth
            const rad = root.panelRadius
            const chamfer = root.panelChamfer
            const topLeft = panelShape.hasChamfer(HnCornerMask.TopLeft)
            const topRight = panelShape.hasChamfer(HnCornerMask.TopRight)
            const bottomRight = panelShape.hasChamfer(HnCornerMask.BottomRight)
            const bottomLeft = panelShape.hasChamfer(HnCornerMask.BottomLeft)
            const ptr = StatusPopupSurface.pointerX
            const frameInset = root.panelBorderWidth / 2
            const left = root.panelLeft + frameInset
            const right = root.panelRight - frameInset
            const top = root.panelTop + frameInset
            const bottom = root.panelBottom - frameInset
            const tipY = root.topPadding + frameInset
            const topLeftExtent = topLeft ? chamfer : rad
            const topRightExtent = topRight ? chamfer : rad

            ctx.beginPath()
            ctx.moveTo(left + topLeftExtent, top)
            ctx.lineTo(ptr - half, top)
            ctx.lineTo(ptr, tipY)
            ctx.lineTo(ptr + half, top)
            ctx.lineTo(right - topRightExtent, top)
            if (topRight) {
                ctx.lineTo(right, top + chamfer)
            } else {
                ctx.arcTo(right, top, right, top + rad, rad)
            }
            ctx.lineTo(right, bottom - (bottomRight ? chamfer : rad))
            if (bottomRight) {
                ctx.lineTo(right - chamfer, bottom)
            } else {
                ctx.arcTo(right, bottom, right - rad, bottom, rad)
            }
            ctx.lineTo(left + (bottomLeft ? chamfer : rad), bottom)
            if (bottomLeft) {
                ctx.lineTo(left, bottom - chamfer)
            } else {
                ctx.arcTo(left, bottom, left, bottom - rad, rad)
            }
            ctx.lineTo(left, top + topLeftExtent)
            if (topLeft) {
                ctx.lineTo(left + chamfer, top)
            } else {
                ctx.arcTo(left, top, left + rad, top, rad)
            }
            ctx.closePath()

            ctx.fillStyle = root.panelFillColor
            ctx.fill()
            ctx.lineWidth = root.panelBorderWidth
            ctx.strokeStyle = root.panelBorderColor
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(Math.max(left + topLeftExtent, ptr - half - 22), top)
            ctx.lineTo(ptr - half, top)
            ctx.lineTo(ptr, tipY)
            ctx.lineTo(ptr + half, top)
            ctx.lineTo(Math.min(right - topRightExtent, ptr + half + 22), top)
            ctx.lineWidth = root.panelBorderWidth * 1.2
            ctx.strokeStyle = Qt.rgba(root.panelAccentColor.r, root.panelAccentColor.g,
                                      root.panelAccentColor.b, 0.72)
            ctx.shadowBlur = 8
            ctx.shadowColor = root.panelAccentColor
            ctx.stroke()
        }

        Connections {
            target: StatusPopupSurface
            function onGeometryChanged() { panelShape.requestPaint() }
        }

        Connections {
            target: root
            function onPanelShapeProfileChanged() { panelShape.requestPaint() }
            function onPanelFillColorChanged() { panelShape.requestPaint() }
            function onPanelBorderColorChanged() { panelShape.requestPaint() }
            function onPanelAccentColorChanged() { panelShape.requestPaint() }
            function onPanelBorderWidthChanged() { panelShape.requestPaint() }
        }
    }

    HnLabel {
        id: titleText
        x: root.panelLeft + root.contentPadding
        y: root.panelTop + root.contentPadding
        width: Math.max(0, root.panelRight - root.panelLeft - 2 * root.contentPadding)
        visible: root.showTitle
        rawText: contentLoader.status === Loader.Error ? qsTr("Unable to load %1").arg(root.displayTitle) : root.displayTitle
        role: HnTypographyRole.Body
        color: HoloniightPalette.textPrimary
        font.weight: Font.Medium
        elide: Text.ElideRight
    }

    // Allocate the interior independently from the body's natural size.
    Loader {
        id: contentLoader
        objectName: "statusPopupContentLoader"
        x: root.panelLeft + root.contentPadding
        y: root.panelTop + root.contentPadding
        width: Math.max(0, root.panelRight - root.panelLeft - 2 * root.contentPadding)
        height: Math.max(0, root.panelBottom - y - root.contentPadding)
        active: root.contentSource.length > 0
        focus: active
        source: root.contentSource
    }
}

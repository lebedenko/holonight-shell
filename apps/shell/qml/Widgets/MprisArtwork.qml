pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import Holonight.Core

// Chamfered artwork display for the MPRIS desktop widget. Real and fallback artwork use the same
// crop and vector mask, so the image edge and visible frame stay aligned at fractional scales.
Item {
    id: root

    required property string artworkPath
    required property string desktopEntry

    implicitWidth: 256
    implicitHeight: 256

    property bool imageError: false
    readonly property bool placeholderVisible: root.artworkPath.length === 0 || root.imageError
    readonly property url fallbackSource: HnIconProvider.sourceUrl(
                                              "qrc:/HolonightShell/media/artwork-fallback.svg",
                                              Math.max(root.width, root.height),
                                              HoloniightPalette.background,
                                              HoloniightPalette.surface,
                                              HoloniightPalette.accentCyan,
                                              HoloniightPalette.accentBlue,
                                              HoloniightPalette.accentViolet,
                                              HoloniightPalette.revision)
    readonly property url effectiveSource: root.placeholderVisible
        ? root.fallbackSource
        : ("file://" + root.artworkPath)

    onArtworkPathChanged: root.imageError = false

    component ArtworkShape: Shape {
        id: shape

        property color fillColor: "transparent"
        property color strokeColor: "transparent"
        property real strokeWidth: 0

        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: shape.fillColor
            strokeColor: shape.strokeColor
            strokeWidth: shape.strokeWidth
            startX: shape.width * 0.078125
            startY: shape.height * 0.03125
            PathLine { x: shape.width * 0.8203125; y: shape.height * 0.03125 }
            PathLine { x: shape.width * 0.96875; y: shape.height * 0.1796875 }
            PathLine { x: shape.width * 0.96875; y: shape.height * 0.921875 }
            PathLine { x: shape.width * 0.921875; y: shape.height * 0.96875 }
            PathLine { x: shape.width * 0.1796875; y: shape.height * 0.96875 }
            PathLine { x: shape.width * 0.03125; y: shape.height * 0.8203125 }
            PathLine { x: shape.width * 0.03125; y: shape.height * 0.078125 }
            PathLine { x: shape.width * 0.078125; y: shape.height * 0.03125 }
        }
    }

    Image {
        id: effectiveImage
        objectName: "mprisArtworkContent"
        anchors.fill: parent
        visible: false
        source: root.effectiveSource
        sourceSize: Qt.size(Math.max(256, root.width), Math.max(256, root.height))
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        layer.enabled: true
        onStatusChanged: {
            if (!root.placeholderVisible && status === Image.Error)
                root.imageError = true;
        }
    }

    ArtworkShape {
        id: artworkMask
        objectName: "mprisArtworkMask"
        anchors.fill: parent
        visible: false
        fillColor: "white"
        layer.enabled: true
    }

    MultiEffect {
        id: artworkEffect
        objectName: "mprisArtworkEffect"
        anchors.fill: parent
        visible: effectiveImage.status === Image.Ready
        source: effectiveImage
        maskEnabled: true
        maskSource: artworkMask
    }

    ArtworkShape {
        objectName: "mprisArtworkFrame"
        anchors.fill: parent
        fillColor: "transparent"
        strokeColor: HoloniightPalette.accentCyan
        strokeWidth: 1
        opacity: 0.38
    }
}

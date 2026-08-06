import QtQuick
import QtQuick.Shapes
import Holonight.Core

// Renders one MPRIS transport control (previous/playPause/next) as a filled ShapePath glyph
// stroked in HoloniightPalette colors (REQ-F-024). The only interactive element in the MPRIS
// pill (REQ-F-021); reacts to left-click only (REQ-F-022).
Item {
    id: root

    required property string glyph  // "previous" | "playPause" | "next"
    property bool playing: false
    property bool buttonEnabled: true

    signal clicked()

    // Plain Item, instantiated directly inside a Row (not a Layout) by MprisWidget.qml — Row
    // positions children by actual width/height, which a bare Item does not auto-bind from
    // implicitWidth/implicitHeight the way Text/Image/Control do, so these must be explicit.
    width: 24
    height: 24
    readonly property color glyphColor: root.buttonEnabled
        ? HoloniightPalette.textPrimary
        : HoloniightPalette.textDisabled
    readonly property int glyphSize: 12

    Shape {
        id: previousShape
        anchors.centerIn: parent
        width: root.glyphSize
        height: root.glyphSize
        visible: root.glyph === "previous"
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.glyphColor
            strokeWidth: 0
            startX: previousShape.width
            startY: 0
            PathLine { x: previousShape.width * 0.3; y: previousShape.height / 2 }
            PathLine { x: previousShape.width; y: previousShape.height }
            PathLine { x: previousShape.width; y: 0 }
        }
        ShapePath {
            fillColor: root.glyphColor
            strokeWidth: 0
            startX: previousShape.width * 0.25
            startY: 0
            PathLine { x: 0; y: 0 }
            PathLine { x: 0; y: previousShape.height }
            PathLine { x: previousShape.width * 0.25; y: previousShape.height }
        }
    }

    Shape {
        id: nextShape
        anchors.centerIn: parent
        width: root.glyphSize
        height: root.glyphSize
        visible: root.glyph === "next"
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.glyphColor
            strokeWidth: 0
            startX: 0
            startY: 0
            PathLine { x: nextShape.width * 0.7; y: nextShape.height / 2 }
            PathLine { x: 0; y: nextShape.height }
            PathLine { x: 0; y: 0 }
        }
        ShapePath {
            fillColor: root.glyphColor
            strokeWidth: 0
            startX: nextShape.width * 0.75
            startY: 0
            PathLine { x: nextShape.width; y: 0 }
            PathLine { x: nextShape.width; y: nextShape.height }
            PathLine { x: nextShape.width * 0.75; y: nextShape.height }
        }
    }

    Shape {
        id: playShape
        anchors.centerIn: parent
        width: root.glyphSize
        height: root.glyphSize
        visible: root.glyph === "playPause" && !root.playing
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.glyphColor
            strokeWidth: 0
            startX: 0
            startY: 0
            PathLine { x: playShape.width; y: playShape.height / 2 }
            PathLine { x: 0; y: playShape.height }
            PathLine { x: 0; y: 0 }
        }
    }

    Shape {
        id: pauseShape
        anchors.centerIn: parent
        width: root.glyphSize
        height: root.glyphSize
        visible: root.glyph === "playPause" && root.playing
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.glyphColor
            strokeWidth: 0
            startX: 0
            startY: 0
            PathLine { x: pauseShape.width * 0.35; y: 0 }
            PathLine { x: pauseShape.width * 0.35; y: pauseShape.height }
            PathLine { x: 0; y: pauseShape.height }
        }
        ShapePath {
            fillColor: root.glyphColor
            strokeWidth: 0
            startX: pauseShape.width * 0.65
            startY: 0
            PathLine { x: pauseShape.width; y: 0 }
            PathLine { x: pauseShape.width; y: pauseShape.height }
            PathLine { x: pauseShape.width * 0.65; y: pauseShape.height }
        }
    }

    TapHandler {
        gesturePolicy: TapHandler.ReleaseWithinBounds
        acceptedButtons: Qt.LeftButton
        enabled: root.buttonEnabled
        onTapped: root.clicked()
    }
}

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import Holonight.Core

Item {
    id: root
    required property bool pointRight   // chevron direction
    required property bool urgent       // true if an urgent workspace lies beyond this edge (either direction)
    property bool canActivate: true
    readonly property bool active: root.canActivate || root.urgent

    signal activated()

    width: 16
    height: 32
    enabled: root.active
    readonly property color resolvedStrokeColor: _style.strokeColor
    readonly property real resolvedStrokeWidth: _style.strokeWidth

    property real urgentPulseOpacity: 0.45
    SequentialAnimation on urgentPulseOpacity {
        running: root.active && root.urgent
        loops: Animation.Infinite
        NumberAnimation { to: 0.95; duration: 760; easing.type: Easing.InOutSine }
        NumberAnimation { to: 0.40; duration: 760; easing.type: Easing.InOutSine }
    }

    MultiEffect {
        id: glowEffect
        objectName: "glowEffect"
        source: chevron
        anchors.fill: chevron
        visible: root.active && root.urgent
        shadowEnabled: true
        shadowColor: HoloniightPalette.accentViolet
        shadowBlur: 0.35
        shadowOpacity: root.urgentPulseOpacity
        shadowScale: 1.1
    }

    Shape {
        id: chevron
        objectName: "chevronShape"
        anchors.centerIn: parent
        width: 14
        height: 18
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: "transparent"
            strokeColor: _style.strokeColor
            strokeWidth: _style.strokeWidth
            capStyle: ShapePath.FlatCap
            joinStyle: ShapePath.MiterJoin
            startX: root.pointRight ? 4 : 10
            startY: 3

            PathLine { x: root.pointRight ? 10 : 4; y: chevron.height / 2 }
            PathLine { x: root.pointRight ? 4 : 10; y: chevron.height - 3 }
        }
    }

    MouseArea {
        objectName: "pointerArea"
        anchors.fill: parent
        enabled: root.active
        onClicked: root.activated()
    }

    QtObject {
        id: _style

        readonly property color strokeColor: {
            if (root.urgent) return HoloniightPalette.accentViolet
            if (root.active) return HoloniightPalette.textPrimary
            return HoloniightPalette.textDisabled
        }
        readonly property real strokeWidth: 1.35
    }
}

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import HolonightShell
import Holonight.Core

Item {
    id: root
    required property string workspaceId
    required property string wsName
    required property bool active
    required property bool urgent
    property bool occupied: false
    required property string barMonitorName
    property var monitorNames: []

    width: 32
    height: 32

    readonly property bool activeOnCurrentMonitor: root.active && root.monitorNames.indexOf(root.barMonitorName) !== -1
    readonly property bool activeOnAnotherMonitor: root.active && !root.activeOnCurrentMonitor

    readonly property color resolvedFillColor: _style.fill
    readonly property color resolvedBorderColor: _style.borderColor
    readonly property color resolvedIconColor: _style.iconColor
    readonly property real resolvedBorderWidth: _style.borderWidth

    property real urgentPulseOpacity: 0.5
    SequentialAnimation on urgentPulseOpacity {
        running: root.urgent
        loops: Animation.Infinite
        NumberAnimation { to: 0.95; duration: 760; easing.type: Easing.InOutSine }
        NumberAnimation { to: 0.40; duration: 760; easing.type: Easing.InOutSine }
    }

    MultiEffect {
        objectName: "specialGlow"
        source: pill
        anchors.fill: pill
        visible: _style.glowing
        autoPaddingEnabled: true
        shadowEnabled: true
        shadowColor: _style.glowColor
        shadowBlur: _style.glowBlur
        shadowOpacity: root.urgent ? root.urgentPulseOpacity : _style.glowOpacity
        shadowScale: root.urgent ? 1.07 : 1.04
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
    }

    Shape {
        id: pill
        objectName: "specialDot"
        anchors.centerIn: parent
        width: parent.width
        height: parent.height
        opacity: _style.fillOpacity
        preferredRendererType: Shape.CurveRenderer

        readonly property int cut: 6

        ShapePath {
            objectName: "specialPillPath"
            fillColor: _style.fill
            strokeColor: _style.borderColor
            strokeWidth: _style.borderWidth
            startX: pill.cut
            startY: 0

            PathLine { x: pill.width - pill.cut; y: 0 }
            PathLine { x: pill.width; y: pill.cut }
            PathLine { x: pill.width; y: pill.height - pill.cut }
            PathLine { x: pill.width - pill.cut; y: pill.height }
            PathLine { x: pill.cut; y: pill.height }
            PathLine { x: 0; y: pill.height - pill.cut }
            PathLine { x: 0; y: pill.cut }
            PathLine { x: pill.cut; y: 0 }
        }

        Behavior on opacity {
            NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
        }
    }

    // qmllint disable import unresolved-type
    HnIcon {
        objectName: "specialIcon"
        anchors.centerIn: pill
        source: "qrc:/HolonightShell/bar-icons/special-ws.svg"
        size: 16
        normalColor: _style.iconColor
        iconState: HnIcon.Normal
    }
    // qmllint enable import unresolved-type

    Rectangle {
        objectName: "otherMonitorBadge"
        anchors.horizontalCenter: pill.horizontalCenter
        anchors.bottom: pill.bottom
        anchors.bottomMargin: 4
        width: 12
        height: 2
        radius: 1
        visible: false
        color: HoloniightPalette.accentBlue
    }

    MouseArea {
        id: pointer
        objectName: "pointerArea"
        anchors.fill: parent
        hoverEnabled: true
        onClicked: CompositorService.activateWorkspace(root.workspaceId)
    }

    BarTooltipArea {
        barMonitorName: root.barMonitorName
        title: root.wsName
        description: root.urgent ? "Needs attention."
            : (root.activeOnCurrentMonitor ? "Currently visible on this monitor."
            : (root.activeOnAnotherMonitor ? "Currently visible on another monitor." : "Hidden special workspace."))
        iconName: "workspace"
    }

    QtObject {
        id: _style

        readonly property bool hovering: pointer.containsMouse
        readonly property bool pressing: pointer.pressed
        readonly property bool occupied: root.occupied && !root.active && !root.urgent
        readonly property bool glowing: root.activeOnCurrentMonitor || root.urgent || hovering

        readonly property color fill: {
            if (root.activeOnCurrentMonitor) return HoloniightPalette.workspaceActive
            if (root.activeOnAnotherMonitor) return HoloniightPalette.workspaceOccupied
            if (root.urgent) return HoloniightPalette.workspaceOccupied
            if (occupied) return HoloniightPalette.workspaceOccupied
            return HoloniightPalette.surface
        }
        readonly property real fillOpacity: {
            const base = root.activeOnCurrentMonitor ? 1.0 : (root.activeOnAnotherMonitor || root.urgent || occupied ? 0.72 : 0.42)
            if (pressing) return base * 0.76
            if (hovering) return Math.min(1.0, base + 0.1)
            return base
        }
        readonly property color borderColor: {
            if (root.activeOnCurrentMonitor) return HoloniightPalette.accentCyan
            if (root.activeOnAnotherMonitor) return HoloniightPalette.accentBlue
            if (root.urgent) return HoloniightPalette.accentViolet
            if (occupied) return HoloniightPalette.borderPassive
            return HoloniightPalette.borderPassive
        }
        readonly property real borderWidth: (root.activeOnCurrentMonitor || root.activeOnAnotherMonitor || root.urgent) ? 1.8 : 1.0
        readonly property color iconColor: {
            if (root.activeOnCurrentMonitor) return HoloniightPalette.accentCyan
            if (root.activeOnAnotherMonitor) return HoloniightPalette.accentBlue
            if (root.urgent) return HoloniightPalette.accentViolet
            if (occupied) return HoloniightPalette.textSecondary
            return HoloniightPalette.textMuted
        }
        readonly property color glowColor: root.urgent ? HoloniightPalette.accentViolet : HoloniightPalette.accentCyan
        readonly property real glowOpacity: hovering ? 1.0 : (root.activeOnCurrentMonitor ? 0.75 : 0)
        readonly property real glowBlur: root.activeOnCurrentMonitor ? 0.3 : 0.18
    }
}

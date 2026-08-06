import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import HolonightShell
import Holonight.Core

Item {
    id: root

    required property int wsId
    required property int wsState
    required property string barMonitorName
    readonly property string label: wsId > 0 ? String(wsId) : ""
    readonly property bool active: wsId > 0
    property bool glowAllowed: true

    width: 32
    height: 32
    enabled: active
    opacity: active ? 1 : 0

    property real glowOpacity: _style.targetGlowOpacity
    property real urgentPulseOpacity: 0.45

    // Exposed for test inspection (see HnIcon.resolvedColor for the same pattern).
    readonly property color resolvedFillColor: _style.fill
    readonly property color resolvedBorderColor: _style.borderColor
    readonly property color resolvedTextColor: _style.textColor
    readonly property real resolvedBorderWidth: _style.borderWidth

    Behavior on glowOpacity {
        NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
    }

    SequentialAnimation on urgentPulseOpacity {
        running: root.active && _style.urgent && !pointer.containsMouse
        loops: Animation.Infinite
        NumberAnimation { to: 0.92; duration: 820; easing.type: Easing.InOutSine }
        NumberAnimation { to: 0.42; duration: 920; easing.type: Easing.InOutSine }
    }

    MultiEffect {
        source: pill
        anchors.fill: pill
        visible: root.glowAllowed && root.glowOpacity > 0.01
        autoPaddingEnabled: true
        shadowEnabled: true
        shadowColor: _style.glowColor
        shadowBlur: _style.glowBlur
        shadowOpacity: _style.urgent && !pointer.containsMouse ? root.urgentPulseOpacity : root.glowOpacity
        shadowScale: _style.glowScale
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
    }

    Shape {
        id: pill
        anchors.centerIn: parent
        width: parent.width
        height: parent.height
        opacity: _style.fillOpacity
        preferredRendererType: Shape.CurveRenderer

        readonly property int cut: 6

        ShapePath {
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

    Text {
        anchors.centerIn: pill
        text: root.label
        color: _style.textColor
        font.weight: _style.fontWeight
    }

    MouseArea {
        id: pointer
        objectName: "pointerArea"
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        enabled: root.active
        onClicked: WorkspaceModel.activateWorkspace(root.wsId)
    }

    BarTooltipArea {
        barMonitorName: root.barMonitorName
        enabled: root.active
        title: "Workspace " + root.label
        description: {
            if (_style.focusedActive) return "Focused workspace on this monitor."
            if (_style.focusedInactive) return "Focused workspace on another monitor."
            if (_style.urgent) return "Workspace needs attention."
            if (_style.occupied) return "Workspace has windows."
            return "Empty workspace."
        }
        iconName: "workspace"
    }

    QtObject {
        id: _style

        readonly property bool focusedActive: root.wsState === WorkspaceModel.FocusedActiveMonitor
                                           || root.wsState === WorkspaceModel.Active
        readonly property bool focusedInactive: root.wsState === WorkspaceModel.FocusedInactiveMonitor
        readonly property bool focused: focusedActive || focusedInactive
        readonly property bool urgent: root.wsState === WorkspaceModel.Urgent
        readonly property bool occupied: root.wsState === WorkspaceModel.Occupied
        readonly property bool glowing: focusedActive || urgent
        readonly property bool hovering: pointer.containsMouse
        readonly property bool pressing: pointer.pressed

        readonly property color activeFill: Qt.tint(
            HoloniightPalette.surface,
            Qt.rgba(HoloniightPalette.accentBlue.r, HoloniightPalette.accentBlue.g,
                    HoloniightPalette.accentBlue.b, 0.05))
        readonly property color occupiedFill: Qt.tint(
            HoloniightPalette.workspaceOccupied,
            Qt.rgba(HoloniightPalette.accentBlue.r, HoloniightPalette.accentBlue.g,
                    HoloniightPalette.accentBlue.b, 0.03))
        readonly property color emptyFill: Qt.tint(
            HoloniightPalette.surface,
            Qt.rgba(HoloniightPalette.borderPassive.r, HoloniightPalette.borderPassive.g,
                    HoloniightPalette.borderPassive.b, 0.08))

        readonly property color fill: {
            if (focusedActive)   return activeFill
            if (focusedInactive) return occupiedFill
            if (urgent)          return occupiedFill
            if (occupied)        return occupiedFill
            return emptyFill
        }
        readonly property real fillOpacity: {
            const base = focusedActive ? 1.0 : (occupied || urgent || focusedInactive ? 0.76 : 0.5)
            if (pressing) return base * 0.76
            if (hovering) return Math.min(1.0, base + 0.1)
            return base
        }
        readonly property color borderColor: {
            if (focusedActive)   return HoloniightPalette.accentCyan
            if (focusedInactive) return HoloniightPalette.borderPassive   // irrelevant — width is 0 below
            if (urgent)          return HoloniightPalette.accentViolet
            if (occupied)        return HoloniightPalette.borderPassive
            return HoloniightPalette.borderPassive
        }
        readonly property real borderWidth: {
            if (focusedActive) return 1.0
            if (focusedInactive) return 0
            if (urgent) return 1.4
            return 1.0
        }
        readonly property color textColor: {
            if (focusedActive)   return HoloniightPalette.textPrimary
            if (focusedInactive) return HoloniightPalette.accentCyan
            if (urgent)          return HoloniightPalette.accentViolet
            if (occupied)        return HoloniightPalette.textSecondary
            return HoloniightPalette.textMuted
        }
        readonly property int fontWeight: focusedActive ? Font.DemiBold : Font.Normal
        readonly property color glowColor: hovering
            ? Qt.tint(borderColor, Qt.rgba(HoloniightPalette.accentCyan.r, HoloniightPalette.accentCyan.g,
                                           HoloniightPalette.accentCyan.b, 0.13))
            : borderColor
        readonly property real targetGlowOpacity: {
            if (hovering) return 1.0
            if (focusedActive) return 0.48
            if (glowing) return 0.68
            return 0
        }
        readonly property real glowBlur: focusedActive ? 0.48 : 0.15
        readonly property real glowScale: urgent ? 1.07 : (focusedActive ? 1.09 : 1.02)
    }
}

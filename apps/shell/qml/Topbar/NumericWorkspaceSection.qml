pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell
import Holonight.Controls
import "../Controls"

BarSection {
    id: root
    required property string barMonitorName
    readonly property int slantCut: 12
    readonly property int contentLeftMargin: 16 + root.slantCut
    readonly property int contentRightMargin: 16 + root.slantCut
    readonly property int inheritedSectionPadding: 8
    readonly property int activeWorkspaceId: CompositorService.revision >= 0
        ? CompositorService.activeNumericWorkspaceForOutput(root.barMonitorName) : 0
    property int manualPanOffset: 0
    onActiveWorkspaceIdChanged: root.manualPanOffset = 0
    readonly property int targetWindowStart: Math.max(
        1, root.activeWorkspaceId - Math.floor((CompositorService.workspaceDisplayCount - 1) / 2))
    readonly property int windowStart: Math.max(1, root.targetWindowStart + root.manualPanOffset)
    readonly property int windowEndExclusive: root.windowStart + CompositorService.workspaceDisplayCount
    readonly property bool rightUrgentBeyond: CompositorService.revision >= 0
        ? CompositorService.hasUrgentNumericWorkspaceAtOrBeyond(root.windowEndExclusive) : false
    readonly property bool rightOccupiedBeyond: CompositorService.revision >= 0
        ? CompositorService.hasNavigableNumericWorkspaceAtOrBeyond(root.windowEndExclusive) : false
    readonly property bool leftUrgentBefore: CompositorService.revision >= 0
        ? CompositorService.hasUrgentNumericWorkspaceBefore(root.windowStart) : false
    readonly property var specialWorkspaces: CompositorService.revision >= 0
        ? CompositorService.specialWorkspaces() : []

    visible: CompositorService.connected && CompositorService.canListWorkspaces
    implicitWidth: visible ? root.contentLeftMargin + pillRow.implicitWidth + root.contentRightMargin : 0

    BarFrame {
        anchors { fill: parent; leftMargin: -root.inheritedSectionPadding; rightMargin: -root.inheritedSectionPadding }
        leftTopOffset: root.slantCut
        rightBottomOffset: root.slantCut
    }

    Row {
        id: pillRow
        anchors {
            left: parent.left
            leftMargin: root.contentLeftMargin - root.inheritedSectionPadding
            right: parent.right
            rightMargin: root.contentRightMargin - root.inheritedSectionPadding
            verticalCenter: parent.verticalCenter
        }
        spacing: 8
        move: Transition { NumberAnimation { properties: "x"; duration: 150; easing.type: Easing.OutCubic } }

        Repeater {
            objectName: "specialDotsRepeater"
            model: root.specialWorkspaces
            delegate: SpecialWorkspaceDot {
                required property var modelData
                y: Math.round((pillRow.height - height) / 2)
                workspaceId: modelData.id
                wsName: modelData.name
                active: modelData.active
                urgent: modelData.urgent
                occupied: modelData.occupied
                monitorNames: modelData.monitorNames
                barMonitorName: root.barMonitorName
            }
        }

        Loader {
            objectName: "separatorLoader"
            anchors.verticalCenter: parent.verticalCenter
            active: root.specialWorkspaces.length > 0
            sourceComponent: HnSeparator {
                orientation: Qt.Vertical
                height: 48
                fadeMode: HnSeparator.FadeBoth
            }
        }

        WorkspaceEdgeArrow {
            objectName: "leftArrow"
            anchors.verticalCenter: parent.verticalCenter
            pointRight: false
            urgent: root.leftUrgentBefore
            canActivate: root.windowStart > 1
            onActivated: {
                if (root.leftUrgentBefore)
                    CompositorService.activateWorkspace(String(CompositorService.lastUrgentNumericWorkspaceBefore(root.windowStart)))
                else
                    root.manualPanOffset -= 1
            }
        }

        WorkspacePillStrip {
            objectName: "pillStrip"
            anchors.verticalCenter: parent.verticalCenter
            barMonitorName: root.barMonitorName
            windowStart: root.windowStart
        }

        WorkspaceEdgeArrow {
            objectName: "rightArrow"
            anchors.verticalCenter: parent.verticalCenter
            pointRight: true
            urgent: root.rightUrgentBeyond
            canActivate: root.rightOccupiedBeyond || root.rightUrgentBeyond
            onActivated: {
                if (root.rightUrgentBeyond)
                    CompositorService.activateWorkspace(String(CompositorService.firstUrgentNumericWorkspaceAtOrBeyond(root.windowEndExclusive)))
                else
                    root.manualPanOffset += 1
            }
        }
    }
}

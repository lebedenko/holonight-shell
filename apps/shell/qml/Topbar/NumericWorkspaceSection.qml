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
    readonly property int activeWorkspaceId: WorkspacePresentation.revision >= 0
        ? WorkspacePresentation.activeNumericWorkspaceForOutput(root.barMonitorName) : 0
    property int manualPanOffset: 0
    onActiveWorkspaceIdChanged: root.manualPanOffset = 0
    readonly property int windowStart: WorkspacePresentation.revision >= 0
        ? WorkspacePresentation.viewportStart(root.barMonitorName, root.manualPanOffset) : 1
    readonly property int windowEndExclusive: root.windowStart + WorkspacePresentation.workspaceDisplayCount
    readonly property bool rightUrgentBeyond: WorkspacePresentation.revision >= 0
        ? WorkspacePresentation.hasUrgentNumericWorkspaceAtOrBeyond(root.windowEndExclusive) : false
    readonly property bool rightOccupiedBeyond: WorkspacePresentation.revision >= 0
        ? WorkspacePresentation.hasNavigableNumericWorkspaceAtOrBeyond(root.windowEndExclusive) : false
    readonly property bool leftUrgentBefore: WorkspacePresentation.revision >= 0
        ? WorkspacePresentation.hasUrgentNumericWorkspaceBefore(root.windowStart) : false
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

        WorkspaceEdgeArrow {
            objectName: "leftArrow"
            anchors.verticalCenter: parent.verticalCenter
            pointRight: false
            urgent: root.leftUrgentBefore
            canActivate: root.windowStart > 1
            onActivated: {
                if (root.leftUrgentBefore)
                    WorkspacePresentation.activateNumberedSlot(WorkspacePresentation.lastUrgentNumericWorkspaceBefore(root.windowStart))
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
                    WorkspacePresentation.activateNumberedSlot(WorkspacePresentation.firstUrgentNumericWorkspaceAtOrBeyond(root.windowEndExclusive))
                else
                    root.manualPanOffset += 1
            }
        }
    }
}

pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell
import Holonight.Core
import Holonight.Controls

import "../Controls"

BarSection {
    id: root

    required property string barMonitorName
    readonly property int slantCut: 12
    readonly property int contentLeftMargin: 16 + root.slantCut
    readonly property int contentRightMargin: 16 + root.slantCut
    readonly property int inheritedSectionPadding: 8

    readonly property int activeWorkspaceId: WorkspaceModel.revision >= 0
        ? WorkspaceModel.activeWorkspaceForMonitor(root.barMonitorName)
        : 0
    property int manualPanOffset: 0
    onActiveWorkspaceIdChanged: root.manualPanOffset = 0

    readonly property int targetWindowStart: Math.max(
        1, root.activeWorkspaceId - Math.floor((WorkspaceModel.displayCount - 1) / 2))
    readonly property int windowStart: Math.max(1, root.targetWindowStart + root.manualPanOffset)
    readonly property int windowEndExclusive: root.windowStart + WorkspaceModel.displayCount

    readonly property bool rightUrgentBeyond: WorkspaceModel.revision >= 0
        ? WorkspaceModel.hasUrgentBeyond(root.windowEndExclusive) : false
    readonly property bool rightOccupiedBeyond: WorkspaceModel.revision >= 0
        ? WorkspaceModel.hasVisibleOrNavigableBeyond(root.windowEndExclusive) : false
    readonly property bool leftUrgentBefore: WorkspaceModel.revision >= 0
        ? WorkspaceModel.hasUrgentBefore(root.windowStart) : false
    readonly property var specialWorkspaces: WorkspaceModel.revision >= 0
        ? WorkspaceModel.specialWorkspaceList() : []

    implicitWidth: root.contentLeftMargin + pillRow.implicitWidth + root.contentRightMargin

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
            visible: active
            active: root.specialWorkspaces.length > 0
            sourceComponent: separatorComponent
        }

        WorkspaceEdgeArrow {
            objectName: "leftArrow"
            anchors.verticalCenter: parent.verticalCenter
            pointRight: false
            urgent: root.leftUrgentBefore
            canActivate: root.windowStart > 1
            onActivated: {
                if (root.leftUrgentBefore) {
                    WorkspaceModel.activateWorkspace(WorkspaceModel.lastUrgentIdBefore(root.windowStart))
                } else {
                    root.manualPanOffset -= 1
                }
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
                if (root.rightUrgentBeyond) {
                    WorkspaceModel.activateWorkspace(WorkspaceModel.firstUrgentIdBeyond(root.windowEndExclusive))
                } else {
                    root.manualPanOffset += 1
                }
            }
        }
    }

    Component {
        id: separatorComponent
        HnSeparator {
            objectName: "workspaceSpecialSeparator"
            orientation: Qt.Vertical
            height: 48
            fadeMode: HnSeparator.FadeBoth
            opacity: 0
            Component.onCompleted: opacity = 1
            Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        }
    }
}

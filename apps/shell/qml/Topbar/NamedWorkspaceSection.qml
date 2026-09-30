pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell

import "../Controls"

BarSection {
    id: root

    required property string barMonitorName
    readonly property int slantCut: 12
    readonly property int contentLeftMargin: 16 + root.slantCut
    readonly property int contentRightMargin: 16 + root.slantCut
    readonly property int inheritedSectionPadding: 8
    readonly property int firstRow: WorkspacePresentation.revision >= 0 ? WorkspacePresentation.firstVisibleWorkspaceRow() : 0

    visible: CompositorService.connected && CompositorService.canListWorkspaces
    implicitWidth: visible ? root.contentLeftMargin + pills.implicitWidth + root.contentRightMargin : 0

    BarFrame {
        anchors { fill: parent; leftMargin: -root.inheritedSectionPadding; rightMargin: -root.inheritedSectionPadding }
        leftTopOffset: root.slantCut
        rightBottomOffset: root.slantCut
    }

    Row {
        id: pills
        anchors.centerIn: parent
        spacing: 16

        Repeater {
            model: CompositorService.workspaces
            delegate: Loader {
                id: workspaceLoader
                required property int index
                required property string workspaceId
                required property string displayName
                required property bool canActivate
                required property string visualState
                readonly property bool inWindow: index >= root.firstRow
                                                     && index < root.firstRow + WorkspacePresentation.workspaceDisplayCount
                active: inWindow
                visible: inWindow
                sourceComponent: WorkspacePill {
                    workspaceId: workspaceLoader.workspaceId
                    numericSlot: null
                    label: workspaceLoader.displayName
                    canActivate: workspaceLoader.canActivate
                    onActivated: CompositorService.activateWorkspace(workspaceLoader.workspaceId)
                    visualState: workspaceLoader.visualState
                    barMonitorName: root.barMonitorName
                }
            }
        }
    }
}

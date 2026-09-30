pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: root
    required property string barMonitorName
    required property var contributionModel
    implicitWidth: dots.count > 0 ? row.implicitWidth : 0
    implicitHeight: dots.count > 0 ? row.implicitHeight : 0
    visible: dots.count > 0
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 8
        Repeater {
            id: dots
            model: root.contributionModel.specialWorkspaces
            delegate: SpecialWorkspaceDot {
                objectName: "specialWorkspaceDot"
                required property var modelData
                workspaceId: modelData.id
                wsName: modelData.name
                active: modelData.active
                urgent: modelData.urgent
                occupied: modelData.occupied
                monitorNames: modelData.monitorNames
                barMonitorName: root.barMonitorName
                onActivated: root.contributionModel.activateSpecialWorkspace(workspaceId)
            }
        }
    }
}

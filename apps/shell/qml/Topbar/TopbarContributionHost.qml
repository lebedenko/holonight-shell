import QtQuick
import HolonightShell

Loader {
    id: root
    required property string barMonitorName
    active: String(IntegrationLoader.componentUrl).length > 0
    Component.onCompleted: {
        if (active) setSource(IntegrationLoader.componentUrl, {
            "barMonitorName": Qt.binding(function() { return root.barMonitorName }),
            "contributionModel": IntegrationLoader.contributionModel
        })
    }
}

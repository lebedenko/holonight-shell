import QtQuick
import HolonightShell

Loader {
    id: root
    required property string barMonitorName
    property url componentUrl: IntegrationLoader.componentUrl
    property var contributionModel: IntegrationLoader.contributionModel
    readonly property Item contributionItem: root.status === Loader.Ready ? root.item as Item : null
    readonly property bool hasContent: root.contributionItem !== null
        && root.contributionItem.visible && root.contributionItem.implicitWidth > 0
    property bool completed: false
    active: String(root.componentUrl).length > 0
    width: root.hasContent ? root.contributionItem.implicitWidth : 0
    height: root.hasContent ? root.contributionItem.implicitHeight : 0

    function loadContribution(): void {
        if (root.completed) root.setSource(root.componentUrl, {
            "barMonitorName": Qt.binding(function() { return root.barMonitorName }),
            "contributionModel": Qt.binding(function() { return root.contributionModel })
        })
    }
    onComponentUrlChanged: root.loadContribution()
    Component.onCompleted: {
        root.completed = true
        root.loadContribution()
    }
}

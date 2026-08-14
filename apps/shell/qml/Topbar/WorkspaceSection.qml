pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell

Loader {
    id: root
    required property string barMonitorName
    readonly property bool numericMode: CompositorService.canCreateNumericWorkspaces
    sourceComponent: root.numericMode ? numericSection : namedSection

    Component {
        id: numericSection
        NumericWorkspaceSection { barMonitorName: root.barMonitorName }
    }
    Component {
        id: namedSection
        NamedWorkspaceSection { barMonitorName: root.barMonitorName }
    }
}

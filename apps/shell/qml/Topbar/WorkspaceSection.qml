pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell

Loader {
    id: root
    required property string barMonitorName
    readonly property bool numericMode: WorkspacePresentation.useNumericWorkspacePresentation
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

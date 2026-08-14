import QtQuick
import QtTest
import HolonightShell

TestCase {
    name: "WorkspaceSectionQmlTests"

    Component {
        id: sectionComponent
        WorkspaceSection { barMonitorName: "DP-1" }
    }

    function test_hyprland_uses_numeric_workspace_presentation() {
        const section = createTemporaryObject(sectionComponent, this)
        verify(section !== null)
        verify(CompositorService.connected)
        compare(section.numericMode, true)
        verify(section.item !== null)
        compare(section.item.activeWorkspaceId, 1)
        compare(section.item.windowStart, 1)
    }
}

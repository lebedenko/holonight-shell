import QtQuick
import QtTest
import HolonightShell

TestCase {
    name: "WorkspaceSectionQmlTests"

    Component {
        id: sectionComponent
        WorkspaceSection { barMonitorName: "DP-1" }
    }

    function test_section_uses_connected_neutral_model() {
        const section = createTemporaryObject(sectionComponent, this)
        verify(section !== null)
        verify(CompositorService.connected)
        compare(section.firstRow, 0)
    }
}

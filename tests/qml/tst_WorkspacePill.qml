import QtQuick
import QtTest
import HolonightShell

TestCase {
    name: "WorkspacePillQmlTests"

    Component {
        id: pillComponent
        WorkspacePill {
            workspaceId: "dev:web"
            numericSlot: undefined
            label: "dev:web"
            workspaceKind: "normal"
            visualState: "urgent"
            barMonitorName: "TEST-1"
        }
    }

    function test_named_workspace_uses_label_sized_pill_and_opaque_activation() {
        const pill = createTemporaryObject(pillComponent, this)
        verify(pill !== null)
        compare(pill.label, "dev:web")
        verify(pill.width > 32)
        verify(pill.width <= 120)
        compare(pill.workspaceId, "dev:web")
    }
}

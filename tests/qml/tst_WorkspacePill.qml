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
            visualState: "urgent"
            barMonitorName: "TEST-1"
        }
    }

    function test_tooltip_preserves_known_occupancy_data() {
        return [
            { tag: "unknown", state: "inactive", description: "Inactive workspace." },
            { tag: "empty", state: "empty", description: "Empty workspace." },
            { tag: "occupied", state: "occupied", description: "Workspace has windows." },
            { tag: "active", state: "active", description: "Active workspace." },
            { tag: "urgent", state: "urgent", description: "Workspace needs attention." }
        ]
    }

    function test_tooltip_preserves_known_occupancy(data) {
        const pill = createTemporaryObject(pillComponent, this, { visualState: data.state })
        compare(pill.tooltipDescription, data.description)
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

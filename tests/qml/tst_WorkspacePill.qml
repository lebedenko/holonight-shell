import QtQuick
import QtTest
import HolonightShell
import Holonight.Core

// T-020: WorkspacePill urgent color unification (REQ-F-007) and FocusedInactiveMonitor
// restyle (REQ-F-008, REQ-NF-003).

TestCase {
    id: root

    name: "WorkspacePillQmlTests"

    Component {
        id: pillComponent
        WorkspacePill {
            wsId: 3
            barMonitorName: "TEST-1"
        }
    }

    function tinted(base, tint, amount) {
        return Qt.tint(base, Qt.rgba(tint.r, tint.g, tint.b, amount))
    }

    // ── REQ-F-007: urgent state uses accentViolet, never borderUrgent/error ─

    function test_urgent_state_uses_accent_violet_for_border_and_text() {
        const pill = createTemporaryObject(pillComponent, null, { wsState: WorkspaceModel.Urgent })
        verify(pill)
        compare(pill.resolvedBorderColor, HoloniightPalette.accentViolet)
        compare(pill.resolvedTextColor, HoloniightPalette.accentViolet)
    }

    function test_focused_active_monitor_uses_thin_cyan_border() {
        const pill = createTemporaryObject(pillComponent, null, { wsState: WorkspaceModel.FocusedActiveMonitor })
        verify(pill)
        compare(pill.resolvedBorderWidth, 1.0)
        compare(pill.resolvedBorderColor, HoloniightPalette.accentCyan)
    }

    function test_focused_active_monitor_uses_blue_tinted_fill() {
        const active = createTemporaryObject(pillComponent, null, { wsId: 3, wsState: WorkspaceModel.FocusedActiveMonitor })
        const occupied = createTemporaryObject(pillComponent, null, { wsId: 4, wsState: WorkspaceModel.Occupied })
        const empty = createTemporaryObject(pillComponent, null, { wsId: 5, wsState: WorkspaceModel.Empty })
        verify(active)
        verify(occupied)
        verify(empty)
        compare(active.resolvedFillColor, tinted(HoloniightPalette.surface, HoloniightPalette.accentBlue, 0.05))
        verify(active.resolvedFillColor !== occupied.resolvedFillColor)
        verify(active.resolvedFillColor !== empty.resolvedFillColor)
    }

    function test_occupied_inactive_fill_remains_subtly_brighter_than_empty() {
        const occupied = createTemporaryObject(pillComponent, null, { wsId: 4, wsState: WorkspaceModel.Occupied })
        const empty = createTemporaryObject(pillComponent, null, { wsId: 5, wsState: WorkspaceModel.Empty })
        verify(occupied)
        verify(empty)
        compare(occupied.resolvedFillColor, tinted(HoloniightPalette.workspaceOccupied, HoloniightPalette.accentBlue, 0.03))
        compare(empty.resolvedFillColor, tinted(HoloniightPalette.surface, HoloniightPalette.borderPassive, 0.08))
        verify(occupied.resolvedFillColor !== empty.resolvedFillColor)
        verify(occupied.resolvedFillColor.b > empty.resolvedFillColor.b)
    }

    // ── REQ-F-008: FocusedInactiveMonitor — no border, Occupied-matching fill, accent number ──

    function test_focused_inactive_monitor_has_zero_border_width() {
        const pill = createTemporaryObject(pillComponent, null, { wsState: WorkspaceModel.FocusedInactiveMonitor })
        verify(pill)
        compare(pill.resolvedBorderWidth, 0)
    }

    function test_focused_inactive_monitor_fill_matches_occupied() {
        const occupied = createTemporaryObject(pillComponent, null, { wsId: 4, wsState: WorkspaceModel.Occupied })
        const inactive = createTemporaryObject(pillComponent, null, { wsId: 5, wsState: WorkspaceModel.FocusedInactiveMonitor })
        verify(occupied)
        verify(inactive)
        compare(inactive.resolvedFillColor, occupied.resolvedFillColor)
    }

    function test_focused_inactive_monitor_uses_accent_number_without_dot() {
        const pill = createTemporaryObject(pillComponent, null, { wsState: WorkspaceModel.FocusedInactiveMonitor })
        verify(pill)
        compare(pill.resolvedTextColor, HoloniightPalette.accentCyan)
        verify(!findChild(pill, "inactiveMonitorDot"))
    }

    function test_focused_active_monitor_does_not_create_inactive_monitor_dot() {
        const pill = createTemporaryObject(pillComponent, null, { wsState: WorkspaceModel.FocusedActiveMonitor })
        verify(pill)
        verify(!findChild(pill, "inactiveMonitorDot"))
    }

    function test_clicking_an_empty_pill_requests_its_workspace_id() {
        const pill = createTemporaryObject(pillComponent, null, { wsId: 4, wsState: WorkspaceModel.Empty })
        verify(pill)
        const area = findChild(pill, "pointerArea")
        verify(area)

        let activatedId = 0
        function onActivate(id) { activatedId = id }
        WorkspaceModel.activateWorkspaceRequested.connect(onActivate)
        area.clicked(null)
        WorkspaceModel.activateWorkspaceRequested.disconnect(onActivate)
        compare(activatedId, 4)
    }
}

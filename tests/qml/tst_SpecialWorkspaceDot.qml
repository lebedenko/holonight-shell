import QtQuick
import QtTest
import HolonightShell
import Holonight.Core

// T-022: SpecialWorkspaceDot radius/color/glow per state and click dispatch
// (REQ-F-005, REQ-F-006, REQ-NF-002).

TestCase {
    id: root

    name: "SpecialWorkspaceDotQmlTests"

    Component {
        id: dotComponent
        SpecialWorkspaceDot {
            wsName: "special:scratch"
            barMonitorName: "TEST-1"
        }
    }

    function test_empty_state_uses_dim_outline_and_icon_no_glow() {
        const dot = createTemporaryObject(dotComponent, null, { active: false, urgent: false, occupied: false })
        verify(dot)
        const path = findChild(dot, "specialPillPath")
        const icon = findChild(dot, "specialIcon")
        const glow = findChild(dot, "specialGlow")
        verify(path)
        verify(icon)
        verify(glow)
        compare(path.strokeColor, HoloniightPalette.borderPassive)
        compare(icon.resolvedColor, HoloniightPalette.textMuted)
        compare(glow.visible, false)
    }

    function test_occupied_state_uses_medium_outline_and_secondary_icon() {
        const dot = createTemporaryObject(dotComponent, null, { active: false, urgent: false, occupied: true })
        verify(dot)
        const path = findChild(dot, "specialPillPath")
        const icon = findChild(dot, "specialIcon")
        verify(path)
        verify(icon)
        compare(path.strokeColor, HoloniightPalette.borderPassive)
        compare(icon.resolvedColor, HoloniightPalette.textSecondary)
    }

    function test_active_on_current_monitor_uses_cyan_glow_and_bright_icon() {
        const dot = createTemporaryObject(dotComponent, null, {
            active: true,
            urgent: false,
            monitorNames: ["TEST-1"]
        })
        verify(dot)
        const path = findChild(dot, "specialPillPath")
        const icon = findChild(dot, "specialIcon")
        const glow = findChild(dot, "specialGlow")
        verify(path)
        verify(icon)
        verify(glow)
        compare(path.strokeColor, HoloniightPalette.accentCyan)
        compare(icon.resolvedColor, HoloniightPalette.accentCyan)
        compare(glow.visible, true)
    }

    function test_active_on_another_monitor_uses_blue_outline() {
        const dot = createTemporaryObject(dotComponent, null, {
            active: true,
            urgent: false,
            monitorNames: ["OTHER-1"]
        })
        verify(dot)
        const badge = findChild(dot, "otherMonitorBadge")
        const icon = findChild(dot, "specialIcon")
        const path = findChild(dot, "specialPillPath")
        verify(badge)
        verify(icon)
        verify(path)
        compare(badge.visible, false)
        compare(path.strokeColor, HoloniightPalette.accentBlue)
        compare(icon.resolvedColor, HoloniightPalette.accentBlue)
    }

    function test_urgent_state_uses_accent_violet_with_glow() {
        const dot = createTemporaryObject(dotComponent, null, { active: false, urgent: true })
        verify(dot)
        const path = findChild(dot, "specialPillPath")
        const icon = findChild(dot, "specialIcon")
        const glow = findChild(dot, "specialGlow")
        verify(path)
        verify(icon)
        verify(glow)
        compare(path.strokeColor, HoloniightPalette.accentViolet)
        compare(icon.resolvedColor, HoloniightPalette.accentViolet)
        compare(glow.shadowColor, HoloniightPalette.accentViolet)
        compare(glow.visible, true)
    }

    function test_urgent_pulse_animates_between_at_least_two_intensity_levels() {
        const dot = createTemporaryObject(dotComponent, null, { active: false, urgent: true })
        verify(dot)
        const initial = dot.urgentPulseOpacity
        wait(200)
        verify(dot.urgentPulseOpacity !== initial)
    }

    function test_clicking_invokes_activate_special_workspace_with_name() {
        const dot = createTemporaryObject(dotComponent, null, { wsName: "special:notes", active: true, urgent: false })
        verify(dot)
        const area = findChild(dot, "pointerArea")
        verify(area)
        let activatedName = ""
        function onActivate(name) { activatedName = name }
        WorkspaceModel.activateSpecialWorkspaceRequested.connect(onActivate)
        area.clicked(null)
        WorkspaceModel.activateSpecialWorkspaceRequested.disconnect(onActivate)
        compare(activatedName, "special:notes")
    }
}

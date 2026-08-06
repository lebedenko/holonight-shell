import QtQuick
import QtTest
import HolonightShell
import Holonight.Core

// T-021: WorkspaceEdgeArrow color/glow state and click dispatch (REQ-F-004, REQ-NF-002).

TestCase {
    id: root

    name: "WorkspaceEdgeArrowQmlTests"

    Component {
        id: arrowComponent
        WorkspaceEdgeArrow {
            pointRight: true
            urgent: false
        }
    }

    function test_non_urgent_active_uses_neutral_chevron_stroke_and_no_glow() {
        const arrow = createTemporaryObject(arrowComponent, null, { urgent: false })
        verify(arrow)
        const chevron = findChild(arrow, "chevronShape")
        const glow = findChild(arrow, "glowEffect")
        const area = findChild(arrow, "pointerArea")
        verify(chevron)
        verify(glow)
        verify(area)
        compare(arrow.enabled, true)
        compare(arrow.resolvedStrokeColor, HoloniightPalette.textPrimary)
        compare(arrow.resolvedStrokeWidth, 1.35)
        compare(glow.visible, false)
        compare(area.hoverEnabled, false)
    }

    function test_urgent_uses_accent_violet_chevron_stroke_and_glow() {
        const arrow = createTemporaryObject(arrowComponent, null, { urgent: true })
        verify(arrow)
        const chevron = findChild(arrow, "chevronShape")
        const glow = findChild(arrow, "glowEffect")
        verify(chevron)
        verify(glow)
        compare(arrow.enabled, true)
        compare(arrow.resolvedStrokeColor, HoloniightPalette.accentViolet)
        compare(arrow.resolvedStrokeWidth, 1.35)
        compare(glow.visible, true)
        compare(glow.shadowColor, HoloniightPalette.accentViolet)
    }

    function test_disabled_state_keeps_slot_and_uses_disabled_chevron_stroke() {
        const arrow = createTemporaryObject(arrowComponent, null, { canActivate: false })
        verify(arrow)
        const chevron = findChild(arrow, "chevronShape")
        const area = findChild(arrow, "pointerArea")
        verify(chevron)
        verify(area)
        compare(arrow.visible, true)
        compare(arrow.enabled, false)
        compare(arrow.opacity, 1)
        compare(arrow.resolvedStrokeColor, HoloniightPalette.textDisabled)
        compare(area.enabled, false)
    }

    function test_urgent_overrides_can_activate_for_visual_and_click_state() {
        const arrow = createTemporaryObject(arrowComponent, null, { canActivate: false, urgent: true })
        verify(arrow)
        const chevron = findChild(arrow, "chevronShape")
        const glow = findChild(arrow, "glowEffect")
        const area = findChild(arrow, "pointerArea")
        verify(chevron)
        verify(glow)
        verify(area)
        compare(arrow.enabled, true)
        compare(arrow.resolvedStrokeColor, HoloniightPalette.accentViolet)
        compare(glow.visible, true)
        compare(area.enabled, true)
    }

    function test_urgent_pulse_animates_between_at_least_two_intensity_levels() {
        const arrow = createTemporaryObject(arrowComponent, null, { urgent: true })
        verify(arrow)
        // Pulse cycles between 0.40 and 0.95 continuously — wait long enough to observe movement
        // away from the initial value.
        const initial = arrow.urgentPulseOpacity
        wait(200)
        verify(arrow.urgentPulseOpacity !== initial)
    }

    function test_activated_signal_fires_on_click() {
        const arrow = createTemporaryObject(arrowComponent, null, {})
        verify(arrow)
        const area = findChild(arrow, "pointerArea")
        verify(area)
        let fired = false
        function onActivate() { fired = true }
        arrow.activated.connect(onActivate)
        area.clicked(null)
        arrow.activated.disconnect(onActivate)
        verify(fired)
    }
}

import QtQuick
import QtTest
import HolonightShell

// T-019: WorkspacePillStrip sliding-viewport behavior (REQ-F-002, REQ-F-003, REQ-NF-002).

TestCase {
    id: root

    name: "WorkspacePillStripQmlTests"

    Component {
        id: stripComponent
        WorkspacePillStrip {
            barMonitorName: "TEST-1"
            windowStart: 1
        }
    }

    function init() {
        WorkspaceModelTestSeed.seedRows([{ id: 1, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] }])
        WorkspaceModelTestSeed.seedSpecials([])
    }

    // ── REQ-F-002: constant footprint regardless of window position ─────────

    function test_implicit_width_matches_display_count_footprint() {
        const strip = createTemporaryObject(stripComponent, null, { windowStart: 1 })
        verify(strip)
        const expected = WorkspaceModel.displayCount * (32 + 16) - 16 + 10 * 2
        compare(strip.implicitWidth, expected)
        compare(strip.implicitHeight, 32 + 10 * 2)
        compare(strip.clip, true)
    }

    function test_implicit_width_unchanged_when_window_start_shifts() {
        const strip = createTemporaryObject(stripComponent, null, { windowStart: 1 })
        verify(strip)
        const widthAt1 = strip.implicitWidth
        strip.windowStart = 4
        compare(strip.implicitWidth, widthAt1)
    }

    // ── REQ-F-003: every pill in the visible window is instantiated and carries the right absolute id ──

    function test_strip_contains_a_pill_for_every_absolute_id_in_the_visible_window() {
        WorkspaceModelTestSeed.seedRows([{ id: 6, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] }])
        const strip = createTemporaryObject(stripComponent, null, { windowStart: 4 })
        verify(strip)

        const inner = findChild(strip, "stripInner")
        verify(inner)
        // Visible window is [4, 8] for displayCount=5; every id in that range must have a
        // real WorkspacePill delegate present in the strip (not merely opacity: 0).
        for (let wsId = 4; wsId <= 8; wsId++) {
            let found = false
            for (const child of inner.children) {
                if (child.wsId === wsId) {
                    found = true
                    break
                }
            }
            verify(found, "expected a pill for wsId=" + wsId)
        }
    }

    function test_focused_inactive_pill_does_not_create_separate_indicator_dot() {
        WorkspaceModelTestSeed.seedRows([
            { id: 1, state: WorkspaceModel.FocusedInactiveMonitor, monitorNames: ["OTHER-1"] }
        ])
        const strip = createTemporaryObject(stripComponent, null, { windowStart: 1 })
        verify(strip)

        const inner = findChild(strip, "stripInner")
        verify(inner)

        let pill = null
        for (const child of inner.children) {
            if (child.wsId === 1) {
                pill = child
                break
            }
        }
        verify(pill)
        verify(!findChild(pill, "inactiveMonitorDot"))
    }

    function test_right_pad_pill_suppresses_glow_until_inside_viewport() {
        WorkspaceModelTestSeed.seedRows([{ id: 6, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] }])
        const strip = createTemporaryObject(stripComponent, null, { windowStart: 1 })
        verify(strip)

        const inner = findChild(strip, "stripInner")
        verify(inner)

        function findPill(wsId) {
            for (const child of inner.children) {
                if (child.wsId === wsId) {
                    return child
                }
            }
            return null
        }

        let padPill = findPill(6)
        verify(padPill)
        compare(padPill.glowAllowed, false)

        strip.windowStart = 2
        tryCompare(inner, "x", 10 - (2 - 1) * (32 + 16), 1000)

        padPill = findPill(6)
        verify(padPill)
        compare(padPill.glowAllowed, true)
    }

    function test_left_pad_pill_suppresses_glow_until_inside_viewport() {
        WorkspaceModelTestSeed.seedRows([{ id: 1, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] }])
        const strip = createTemporaryObject(stripComponent, null, { windowStart: 2 })
        verify(strip)

        const inner = findChild(strip, "stripInner")
        verify(inner)

        let padPill = null
        for (const child of inner.children) {
            if (child.wsId === 1) {
                padPill = child
                break
            }
        }
        verify(padPill)
        compare(padPill.glowAllowed, false)
    }

    function test_high_sparse_workspace_id_uses_a_fixed_local_delegate_range() {
        WorkspaceModelTestSeed.seedRows([
            { id: 1000, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] }
        ])
        const strip = createTemporaryObject(stripComponent, null, { windowStart: 998 })
        verify(strip)

        const repeater = findChild(strip, "pillRepeater")
        verify(repeater)
        const expectedCount = WorkspaceModel.displayCount + 2 * strip.stripPad
        compare(strip.stripCount, expectedCount)
        compare(repeater.count, expectedCount)

        const inner = findChild(strip, "stripInner")
        verify(inner)
        for (let wsId = 998; wsId < 998 + WorkspaceModel.displayCount; wsId++) {
            let found = false
            for (const child of inner.children) {
                if (child.wsId === wsId) {
                    found = true
                    break
                }
            }
            verify(found, "expected a visible pill for wsId=" + wsId)
        }
    }

    function test_high_window_keeps_real_pills_on_both_sides() {
        WorkspaceModelTestSeed.seedRows([{ id: 1000, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] }])
        const strip = createTemporaryObject(stripComponent, null, { windowStart: 998 })
        verify(strip)

        const inner = findChild(strip, "stripInner")
        verify(inner)
        const ids = inner.children.filter(child => child.wsId !== undefined)
                .map(child => child.wsId).sort((left, right) => left - right)
        compare(ids[0], 997)
        compare(ids[ids.length - 1], 1003)
    }

    // ── REQ-F-003 AC4 / REQ-NF-002: strip.x animates toward -(windowStart-1)*pillStep ──

    function test_strip_x_animates_a_one_step_window_shift_from_the_left_boundary() {
        const strip = createTemporaryObject(stripComponent, null, { windowStart: 1 })
        verify(strip)
        const inner = findChild(strip, "stripInner")
        verify(inner)
        const pillStep = 32 + 16

        strip.windowStart = 2
        tryCompare(inner, "x", 10 - pillStep, 1000)
    }
}

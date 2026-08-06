import QtQuick
import QtTest
import HolonightShell

// T-018: WorkspaceSection window-centering, manual pan, edge-arrow visibility, and
// special-workspace separator/dot lifecycle (REQ-F-001..004, REQ-F-006).

TestCase {
    id: root

    name: "WorkspaceSectionQmlTests"

    Component {
        id: sectionComponent
        WorkspaceSection {
            barMonitorName: "TEST-1"
        }
    }

    function init() {
        // Every test starts from a known, single-workspace state so tests don't leak into
        // each other via the shared WorkspaceModel singleton.
        WorkspaceModelTestSeed.seedRows([{ id: 1, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] }])
        WorkspaceModelTestSeed.seedSpecials([])
        WorkspaceModelTestSeed.setFocused(0)
    }

    // ── REQ-F-003: window-centering worked examples ─────────────────────────

    function test_window_centered_on_active_id_1_left_clamped() {
        WorkspaceModelTestSeed.seedRows([{ id: 1, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] }])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)
        compare(section.windowStart, 1)
    }

    function test_window_centered_on_active_id_3() {
        WorkspaceModelTestSeed.seedRows([
            { id: 1, state: WorkspaceModel.Occupied },
            { id: 2, state: WorkspaceModel.Occupied },
            { id: 3, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] },
            { id: 4, state: WorkspaceModel.Occupied },
            { id: 5, state: WorkspaceModel.Occupied }
        ])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)
        compare(section.windowStart, 1)
    }

    function test_window_slides_when_active_moves_to_id_4() {
        WorkspaceModelTestSeed.seedRows([
            { id: 1, state: WorkspaceModel.Occupied },
            { id: 4, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] },
            { id: 6, state: WorkspaceModel.Occupied }
        ])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)
        compare(section.windowStart, 2)
    }

    function test_window_slides_when_active_moves_to_id_6() {
        WorkspaceModelTestSeed.seedRows([
            { id: 4, state: WorkspaceModel.Occupied },
            { id: 6, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] },
            { id: 8, state: WorkspaceModel.Occupied }
        ])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)
        compare(section.windowStart, 4)
    }

    // ── REQ-F-004: manual pan survives incidental revision bumps, resets on real focus change ──

    function test_manual_pan_survives_incidental_revision_bump_and_resets_on_focus_change() {
        WorkspaceModelTestSeed.seedRows([
            { id: 4, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] },
            { id: 9, state: WorkspaceModel.Occupied }
        ])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)
        compare(section.windowStart, 2)

        section.manualPanOffset = 1
        compare(section.windowStart, 3)

        // Incidental revision bump unrelated to this monitor's active workspace — pan survives.
        WorkspaceModelTestSeed.seedSpecials([{ name: "special:scratch", active: true, urgent: false }])
        compare(section.manualPanOffset, 1)
        compare(section.windowStart, 3)

        // Real active-workspace change on this monitor — pan resets.
        WorkspaceModelTestSeed.seedRows([
            { id: 7, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] },
            { id: 9, state: WorkspaceModel.Occupied }
        ])
        compare(section.manualPanOffset, 0)
    }

    // ── REQ-F-001/004: edge-arrow visibility ─────────────────────────────────

    function test_left_arrow_disabled_when_window_start_is_1() {
        WorkspaceModelTestSeed.seedRows([{ id: 1, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] }])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)
        const arrow = findChild(section, "leftArrow")
        verify(arrow)
        compare(arrow.visible, true)
        compare(arrow.canActivate, false)
    }

    function test_left_arrow_enabled_when_window_start_beyond_1() {
        WorkspaceModelTestSeed.seedRows([
            { id: 4, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] },
            { id: 9, state: WorkspaceModel.Occupied }
        ])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)
        const arrow = findChild(section, "leftArrow")
        verify(arrow)
        compare(arrow.canActivate, true)
    }

    function test_right_arrow_disabled_when_nothing_occupied_beyond_window() {
        WorkspaceModelTestSeed.seedRows([{ id: 1, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] }])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)
        const arrow = findChild(section, "rightArrow")
        verify(arrow)
        compare(arrow.visible, true)
        compare(arrow.canActivate, false)
    }

    function test_right_arrow_enabled_when_occupied_beyond_window() {
        WorkspaceModelTestSeed.seedRows([
            { id: 1, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] },
            { id: 9, state: WorkspaceModel.Occupied }
        ])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)
        const arrow = findChild(section, "rightArrow")
        verify(arrow)
        compare(arrow.canActivate, true)
    }

    function test_right_arrow_enabled_when_urgent_beyond_window() {
        WorkspaceModelTestSeed.seedRows([
            { id: 1, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] },
            { id: 9, state: WorkspaceModel.Urgent }
        ])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)
        const arrow = findChild(section, "rightArrow")
        verify(arrow)
        compare(arrow.canActivate, true)
        compare(arrow.enabled, true)
    }

    function test_right_arrow_visible_when_active_workspace_is_panned_beyond_window() {
        WorkspaceModelTestSeed.seedRows([
            { id: 7, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] }
        ])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)
        compare(section.windowStart, 5)

        section.manualPanOffset = -3
        compare(section.windowStart, 2)

        const arrow = findChild(section, "rightArrow")
        verify(arrow)
        compare(arrow.canActivate, true)
    }

    // ── REQ-F-006: separator/dots — not in DOM when zero specials ───────────

    function test_separator_and_dots_absent_when_no_specials() {
        WorkspaceModelTestSeed.seedRows([{ id: 1, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] }])
        WorkspaceModelTestSeed.seedSpecials([])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)
        const separatorLoader = findChild(section, "separatorLoader")
        verify(separatorLoader)
        compare(separatorLoader.active, false)
        compare(separatorLoader.item, null)

        const dotsRepeater = findChild(section, "specialDotsRepeater")
        verify(dotsRepeater)
        compare(dotsRepeater.count, 0)
    }

    function test_separator_and_dots_present_when_specials_exist() {
        WorkspaceModelTestSeed.seedRows([{ id: 1, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] }])
        WorkspaceModelTestSeed.seedSpecials([
            { name: "special:scratch", active: true, urgent: false },
            { name: "special:notes", active: false, urgent: false }
        ])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)
        const separatorLoader = findChild(section, "separatorLoader")
        verify(separatorLoader)
        compare(separatorLoader.active, true)
        verify(separatorLoader.item !== null)

        const dotsRepeater = findChild(section, "specialDotsRepeater")
        verify(dotsRepeater)
        compare(dotsRepeater.count, 2)
    }

    // ── REQ-F-004 AC6/AC7: edge-arrow urgent-jump vs. pan dispatch, both sides ──

    function test_right_arrow_urgent_jump_activates_nearest_urgent_beyond_edge() {
        WorkspaceModelTestSeed.seedRows([
            { id: 1, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] },
            { id: 8, state: WorkspaceModel.Urgent },
            { id: 9, state: WorkspaceModel.Urgent }
        ])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)

        let activatedId = -1
        function onActivate(id) { activatedId = id }
        WorkspaceModel.activateWorkspaceRequested.connect(onActivate)
        const arrow = findChild(section, "rightArrow")
        verify(arrow)
        arrow.activated()
        WorkspaceModel.activateWorkspaceRequested.disconnect(onActivate)

        compare(activatedId, 8)
        compare(section.manualPanOffset, 0)
    }

    function test_right_arrow_pan_when_only_occupied_beyond_edge() {
        WorkspaceModelTestSeed.seedRows([
            { id: 1, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] },
            { id: 9, state: WorkspaceModel.Occupied }
        ])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)

        let activatedId = -1
        function onActivate(id) { activatedId = id }
        WorkspaceModel.activateWorkspaceRequested.connect(onActivate)
        const arrow = findChild(section, "rightArrow")
        verify(arrow)
        arrow.activated()
        WorkspaceModel.activateWorkspaceRequested.disconnect(onActivate)

        compare(activatedId, -1)
        compare(section.manualPanOffset, 1)
    }

    function test_left_arrow_urgent_jump_activates_nearest_urgent_before_edge() {
        WorkspaceModelTestSeed.seedRows([
            { id: 1, state: WorkspaceModel.Urgent },
            { id: 2, state: WorkspaceModel.Urgent },
            { id: 7, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] }
        ])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)
        compare(section.windowStart, 5)

        let activatedId = -1
        function onActivate(id) { activatedId = id }
        WorkspaceModel.activateWorkspaceRequested.connect(onActivate)
        const arrow = findChild(section, "leftArrow")
        verify(arrow)
        arrow.activated()
        WorkspaceModel.activateWorkspaceRequested.disconnect(onActivate)

        compare(activatedId, 2)
        compare(section.manualPanOffset, 0)
    }

    function test_left_arrow_pan_when_only_occupied_before_edge() {
        WorkspaceModelTestSeed.seedRows([
            { id: 1, state: WorkspaceModel.Occupied },
            { id: 7, state: WorkspaceModel.Active, monitorNames: ["TEST-1"] }
        ])
        const section = createTemporaryObject(sectionComponent, null)
        verify(section)
        compare(section.windowStart, 5)

        let activatedId = -1
        function onActivate(id) { activatedId = id }
        WorkspaceModel.activateWorkspaceRequested.connect(onActivate)
        const arrow = findChild(section, "leftArrow")
        verify(arrow)
        arrow.activated()
        WorkspaceModel.activateWorkspaceRequested.disconnect(onActivate)

        compare(activatedId, -1)
        compare(section.manualPanOffset, -1)
    }
}

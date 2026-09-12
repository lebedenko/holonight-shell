import QtQuick
import QtTest
import Holonight.Core
import HolonightShell

TestCase {
    id: test
    name: "LauncherInput"
    when: windowShown
    visible: true
    width: 1200
    height: 800

    Component {
        id: launcherComponent
        Launcher { width: 1200; height: 800 }
    }

    function init() {
        LauncherService.setQuery("")
        LauncherService.seedResults([
            {name: "Alpha", desktopFile: "alpha.desktop"},
            {name: "Beta", desktopFile: "beta.desktop"},
            {name: "Gamma", desktopFile: "gamma.desktop"}
        ])
        LauncherService.resetLaunchRecord()
    }

    function test_stationary_open_preserves_initial_enter_target() {
        let launcher = createTemporaryObject(launcherComponent, test)
        verify(launcher)
        waitForRendering(launcher)
        // Known row geometry is discovered through the real result delegate tree.
        tryVerify(() => findRows(launcher).length >= 2)
        const rows = findRows(launcher)
        const row = rows[1]
        mouseMove(row, row.width / 2, row.height / 2)
        launcher.destroy()
        wait(0)
        LauncherService.setSelectedIndex(0)
        launcher = createTemporaryObject(launcherComponent, test)
        verify(launcher)
        wait(200) // Wait through the production 150 ms opening animation.
        compare(LauncherService.selectedIndex, 0)
        keyClick(Qt.Key_Return)
        compare(LauncherService.lastLaunchedIndex, 0)
    }

    function createLauncher() {
        const launcher = createTemporaryObject(launcherComponent, test)
        verify(launcher)
        const list = findChild(launcher, "launcherBrowseList")
        verify(list)
        tryCompare(list, "count", 3)
        tryVerify(() => list.itemAtIndex(1) !== null)
        wait(200)
        return launcher
    }

    function test_movement_within_hovered_row_restores_pointer_after_keyboard() {
        const launcher = createLauncher()
        const row = findChild(launcher, "launcherBrowseList").itemAtIndex(1)
        mouseMove(row, row.width / 2, row.height / 2)
        compare(LauncherService.selectedIndex, 1)
        keyClick(Qt.Key_Up)
        compare(LauncherService.selectedIndex, 0)
        compare(launcher.HnInputInteraction.hoverAllowed, false)
        mouseMove(row, row.width / 2 + 2, row.height / 2)
        compare(LauncherService.selectedIndex, 1)
        keyClick(Qt.Key_Return)
        compare(LauncherService.lastLaunchedIndex, 1)
    }

    function test_click_is_immediate_while_keyboard_has_authority() {
        const launcher = createLauncher()
        const row = findChild(launcher, "launcherBrowseList").itemAtIndex(1)
        mouseMove(row, row.width / 2, row.height / 2)
        keyClick(Qt.Key_Up)
        compare(LauncherService.selectedIndex, 0)
        mouseClick(row, row.width / 2, row.height / 2)
        compare(LauncherService.lastLaunchedIndex, 1)
    }

    function test_filtering_and_search_keep_keyboard_target_until_movement() {
        const launcher = createLauncher()
        const browseRow = findChild(launcher, "launcherBrowseList").itemAtIndex(1)
        mouseMove(browseRow, browseRow.width / 2, browseRow.height / 2)
        LauncherService.setQuery("a")
        const list = findChild(launcher, "launcherSearchList")
        tryVerify(() => list.itemAtIndex(1) !== null)
        waitForRendering(launcher)
        compare(LauncherService.selectedIndex, 0)
        const row = list.itemAtIndex(1)
        mouseMove(row, row.width / 2, row.height / 2)
        compare(LauncherService.selectedIndex, 1)
        keyClick(Qt.Key_Up)
        compare(LauncherService.selectedIndex, 0)
        findChild(launcher, "launcherSearchPanel").activeFilter = "apps"
        waitForRendering(launcher)
        compare(LauncherService.selectedIndex, 0)
        compare(launcher.HnInputInteraction.hoverAllowed, false)
        keyClick(Qt.Key_Return)
        compare(LauncherService.lastLaunchedIndex, 0)
        mouseMove(row, row.width / 2 + 2, row.height / 2)
        compare(LauncherService.selectedIndex, 1)
    }

    function test_headers_empty_space_and_disabled_rows_do_not_select() {
        const launcher = createLauncher()
        LauncherService.setQuery("a")
        const list = findChild(launcher, "launcherSearchList")
        tryVerify(() => list.itemAtIndex(1) !== null)
        waitForRendering(launcher)
        LauncherService.setSelectedIndex(1)
        mouseMove(list, list.width / 2, 2)
        compare(LauncherService.selectedIndex, 1)
        mouseMove(list, list.width / 2, list.height - 2)
        compare(LauncherService.selectedIndex, 1)
        const row = list.itemAtIndex(0)
        row.enabled = false
        mouseMove(row, row.width / 2, row.height / 2)
        compare(LauncherService.selectedIndex, 1)
    }

    function test_reentry_and_reopen_require_movement_for_selection() {
        const launcher = createLauncher()
        const row = findChild(launcher, "launcherBrowseList").itemAtIndex(1)
        mouseMove(row, row.width / 2, row.height / 2)
        launcher.resetAndFocus()
        launcher.forceReopen()
        wait(200)
        compare(LauncherService.selectedIndex, 0)
        mouseMove(test, 1, 1)
        compare(LauncherService.selectedIndex, 0)
        mouseMove(row, row.width / 2, row.height / 2)
        compare(LauncherService.selectedIndex, 1)
    }

    function test_action_rows_and_filtered_layout_use_the_current_hit_target() {
        const launcher = createLauncher()
        LauncherService.seedResults([
            {name: "Alpha", desktopFile: "alpha.desktop"},
            {name: "New window", isAction: true, actionParent: "Alpha", actionExec: "test-only", actionIndex: 7},
            {name: "Beta", desktopFile: "beta.desktop"}
        ])
        LauncherService.setQuery("a")
        const list = findChild(launcher, "launcherSearchList")
        const panel = findChild(launcher, "launcherSearchPanel")
        panel.activeFilter = "actions"
        tryVerify(() => list.itemAtIndex(1) !== null && list.itemAtIndex(1).height > 0)
        waitForRendering(launcher)
        const row = list.itemAtIndex(1)
        mouseMove(test, 1, 1)
        mouseMove(row, row.width / 2, row.height / 2)
        compare(LauncherService.selectedIndex, 1)
        mouseClick(row, row.width / 2, row.height / 2)
        compare(LauncherService.lastLaunchedIndex, 1)
        LauncherService.setSelectedIndex(0)
        panel.activeFilter = "apps"
        waitForRendering(launcher)
        compare(LauncherService.selectedIndex, 0)
        compare(launcher.HnInputInteraction.hoverAllowed, false)
        keyClick(Qt.Key_Return)
        compare(LauncherService.lastLaunchedIndex, 0)
    }

    function cleanupTestCase() {
        LauncherService.setQuery("")
        LauncherService.seedResults([{name: "App 1"}, {name: "App 2"}])
        LauncherService.resetLaunchRecord()
    }

    function findRows(item) {
        let rows = []
        if (item.appName !== undefined && item.appName.length > 0 && item.visible)
            rows.push(item)
        for (const child of item.children)
            rows = rows.concat(findRows(child))
        return rows
    }
}

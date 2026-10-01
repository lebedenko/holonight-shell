import QtQuick
import QtTest
import QtQuick.Layouts
import HolonightShell

TestCase {
    name: "WindowManagement"
    when: windowShown
    visible: true
    width: 900
    height: 700
    property var overlay: null
    Component { id: overlayComponent; WindowOverlay { width: 900; height: 700 } }
    Component { id: taskbarComponent; WindowTaskbar { width: 320; height: 64; barMonitorName: "TEST-1" } }
    function init() {
        WindowSurface.hide()
        CompositorTestSeed.setToplevels(true, true, true)
        WindowPresentation.grouped = true
        WindowSurface.toggle("TEST-1")
    }
    function cleanup() {
        if (overlay) { overlay.destroy(); overlay = null }
        WindowSurface.hide()
        WindowPresentation.grouped = true
        CompositorTestSeed.reset()
    }
    function makeOverlay() {
        overlay = createTemporaryObject(overlayComponent, this)
        verify(overlay !== null)
        tryCompare(overlay, "selectedId", "second")
        return findChild(overlay, "windowSearch")
    }
    function test_keyboardSelectionAndActivationDismissal() {
        const search = makeOverlay()
        verify(search !== null)
        search.forceActiveFocus(); keyClick(Qt.Key_Down)
        compare(overlay.selectedId, "first")
        overlay.forceActiveFocus(); keyClick(Qt.Key_K)
        compare(overlay.selectedId, "second")
        overlay.forceActiveFocus(); keyClick(Qt.Key_L)
        compare(overlay.selectedId, "first")
        overlay.forceActiveFocus(); keyClick(Qt.Key_Return)
        compare(WindowSurface.visible, false)
        // Requests cannot invent compositor state, including when no backend dispatches them.
        compare(WindowPresentation.window("first").activated, false)
    }
    function test_searchEmptyResultsAndSelectionClosure() {
        const search = makeOverlay()
        search.text = "FIRST"
        tryCompare(overlay, "selectedId", "first")
        compare(overlay.cards.length, 1)
        CompositorTestSeed.setToplevels(false, true, true)
        compare(overlay.cards.length, 0)
        compare(overlay.selectedId, "")
        search.forceActiveFocus(); keyClick(Qt.Key_Return)
        compare(WindowSurface.visible, true)
        search.text = ""
        tryCompare(overlay, "selectedId", "second")
        overlay.forceActiveFocus(); keyClick(Qt.Key_Escape)
        compare(WindowSurface.visible, false)
    }
    function test_staleMenuDismissal() {
        WindowSurface.menu("first", "TEST-1")
        overlay = createTemporaryObject(overlayComponent, this)
        verify(overlay !== null)
        compare(WindowSurface.visible, true)
        CompositorTestSeed.setToplevels(false, true, true)
        tryCompare(WindowSurface, "visible", false)
        compare(WindowPresentation.window("first").windowId, undefined)
        compare(WindowPresentation.window("second").windowId, "second")
    }
    Component { id: barComponent; TopBar { width: 1920; height: 64; barMonitorName: "TEST-1" } }
    function test_sectionOrderAndWidthOwnership() {
        const bar = createTemporaryObject(barComponent, this)
        const taskbar = findChild(bar, "taskbarSection")
        const active = findChild(bar, "activeWindowSection")
        const spacer = findChild(bar, "activeWindowSpacer")
        verify(taskbar.visible)
        verify(taskbar.Layout.fillWidth)
        verify(!active.Layout.fillWidth)
        compare(active.Layout.maximumWidth, 320)
        active.localTitle = "A very long window title ".repeat(30)
        compare(active.implicitWidth, 320)
        verify(!spacer.visible)
        verify(taskbar.parent.children.indexOf(taskbar) < taskbar.parent.children.indexOf(active))
        CompositorTestSeed.reset()
        tryVerify(function() { return !taskbar.visible && active.Layout.fillWidth })
        CompositorTestSeed.setWindow(false, "", "", "")
        tryVerify(function() { return spacer.visible })
    }
    function test_iconSizesEmptyInventoryAndPinnedButton() {
        const taskbar = createTemporaryObject(taskbarComponent, this)
        const list = findChild(taskbar, "windowTaskList")
        const leading = findChild(taskbar, "windowListButton")
        compare(taskbar.iconSize, 48)
        taskbar.height = 48
        compare(taskbar.iconSize, 32)
        taskbar.height = 40
        compare(taskbar.iconSize, 24)
        WindowPresentation.grouped = false
        taskbar.width = taskbar.minimumContentWidth
        tryCompare(list, "count", 2)
        verify(list.contentWidth > list.width)
        const pinnedX = leading.x
        list.contentX = 20
        compare(leading.x, pinnedX)
        CompositorTestSeed.setToplevels(false, false)
        tryCompare(list, "count", 0)
        verify(leading.visible)
    }
    function test_groupChooserAndRightClickMenu() {
        WindowSurface.hide()
        const taskbar = createTemporaryObject(taskbarComponent, this)
        const list = findChild(taskbar, "windowTaskList")
        tryCompare(list, "count", 1)
        tryVerify(function() { return list.itemAtIndex(0) !== null })
        mouseClick(list.itemAtIndex(0), 20, 20, Qt.RightButton)
        compare(WindowSurface.mode, 2)
        compare(WindowSurface.choices.length, 2)
        WindowSurface.hide()
        WindowPresentation.grouped = false
        tryCompare(list, "count", 2)
        mouseClick(list.itemAtIndex(0), 20, 20, Qt.RightButton)
        compare(WindowSurface.mode, 1)
        compare(WindowSurface.target, "first")
    }
    function test_badgesMixedMinimizedActiveAndFeedback() {
        WindowSurface.hide()
        const taskbar = createTemporaryObject(taskbarComponent, this)
        const list = findChild(taskbar, "windowTaskList")
        tryVerify(function() { return list.itemAtIndex(0) !== null })
        const task = list.itemAtIndex(0)
        task.windows = [{windowId: "first", minimized: true}, {windowId: "second", minimized: false}]
        compare(task.minimizedCount, 1)
        verify(findChild(task, "taskbarMinimizedDot").visible)
        verify(findChild(task, "taskbarCountBadge").visible)
        verify(findChild(task, "taskbarActiveUnderline").visible)
        const icon = findChild(task, "taskbarAppIcon")
        compare(icon.width, 48)
        compare(icon.iconName, "")
        verify(icon.children[1].visible)
        task.windows = [{windowId: "first", minimized: false}]
        verify(!findChild(task, "taskbarMinimizedDot").visible)
        verify(!findChild(task, "taskbarCountBadge").visible)
        task.windows = [{windowId: "first", minimized: true}, {windowId: "second", minimized: false}]
        mouseMove(taskbar, 300, 60)
        waitForRendering(taskbar)
        mouseMove(task, 20, 20)
        tryCompare(task, "hovered", true)
        const restY = icon.y
        mousePress(task, 20, 20)
        tryCompare(task, "down", true)
        compare(icon.y, restY + 1)
        mouseRelease(task, 20, 20)
        compare(task.down, false)
        verify(WindowSurface.visible)
        compare(TooltipSurface.tooltipVisible, false)
        wait(550)
        compare(TooltipSurface.tooltipVisible, false)
        WindowSurface.hide()
    }
    function test_groupingAndBoundedTaskbar() {
        const taskbar = createTemporaryObject(taskbarComponent, this)
        verify(taskbar !== null)
        const list = findChild(taskbar, "windowTaskList")
        tryCompare(list, "count", 1)
        WindowPresentation.grouped = false
        tryCompare(list, "count", 2)
        compare(taskbar.width, 320)
        verify(list.width < taskbar.width)
    }
}

import QtQuick
import QtTest
import HolonightShell

TestCase {
    name: "ActiveWindow"
    when: windowShown
    visible: true
    width: 1920
    height: 200
    ActiveWindowSection {
        id: section
        width: 600
        barMonitorName: "DP-1"
    }
    TopBar {
        id: bar
        y: 100
        width: 1920
        height: 64
        barMonitorName: "DP-1"
    }
    AppWindowIcon { id: icon }
    function test_fallback_resets_on_app_change() {
        icon.appId = "first"
        icon.loadFailed = true
        icon.appId = "second"
        compare(icon.loadFailed, false)
        icon.loadFailed = true
        icon.iconName = ""
        icon.appId = "third"
        compare(icon.loadFailed, false)
    }
    function test_hidden_section_preserves_groups() {
        CompositorTestSeed.setWindow(false, "", "", "")
        const spacer = findChild(bar, "activeWindowSpacer")
        verify(spacer !== null)
        tryVerify(function() { return spacer.visible && spacer.width > 0 })
        const row = spacer.parent
        const first = row.children[0]
        const last = row.children[row.children.length - 1]
        compare(first.x, 0)
        fuzzyCompare(last.x + last.width, row.width, 0.1)
        CompositorTestSeed.setWindow(true, "", "", "")
        verify(!spacer.visible)
    }
    function cleanup() {
        CompositorTestSeed.reset()
        LauncherService.setAppIcon("example", "")
        LauncherService.setAppIcon("other", "")
    }
    function test_title_and_app_fallback() {
        CompositorTestSeed.setWindow(true, "DP-1", "Title", "example")
        compare(section.displayTitle, "Title")
        compare(section.localAppClass, "example")
        CompositorTestSeed.setWindow(true, "DP-1", "", "example")
        compare(section.displayTitle, "example")
        CompositorTestSeed.setWindow(true, "DP-2", "Other monitor", "example")
        compare(section.displayTitle, "Desktop")
    }
    function test_icon_keeps_original_colors_and_broken_icon_falls_back() {
        CompositorTestSeed.setWindow(true, "DP-1", "Title", "example")
        LauncherService.setAppIcon("example", controlsEvidence.coloredIconPath())
        const appIcon = findChild(section, "activeWindowIcon")
        tryVerify(function() { return appIcon.children[0].status === Image.Ready })
        compare(appIcon.children[0].visible, true)
        compare(appIcon.children[1].visible, false)
        waitForRendering(section)
        const image = controlsEvidence.captureItem(appIcon)
        compare(image.pixel(Math.floor(image.width / 2), Math.floor(image.height / 2)), "#c14287")
        LauncherService.setAppIcon("example", "/holonight-missing-test-icon.png")
        tryCompare(appIcon, "loadFailed", true)
        verify(appIcon.children[1].visible)
        LauncherService.setAppIcon("other", "/holonight-missing-test-icon.png")
        CompositorTestSeed.setWindow(true, "DP-1", "Next", "other")
        compare(appIcon.loadFailed, false)
        verify(appIcon.children[1].visible)
        LauncherService.setAppIcon("other", "")
    }
    function test_inventory_refresh_and_capability() {
        CompositorTestSeed.setWindow(true, "DP-1", "Title", "example")
        LauncherService.setAppIcon("example", "example-icon")
        compare(section.localIcon, "example-icon")
        verify(section.visible)
        CompositorTestSeed.setWindow(false, "", "", "")
        verify(!section.visible)
        CompositorTestSeed.setWindow(true, "", "", "")
        verify(section.visible)
        compare(section.displayTitle, "Desktop")
    }
}

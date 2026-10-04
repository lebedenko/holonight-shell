import QtQuick
import QtTest
import HolonightShell

TestCase {
    name: "DesktopMenu"
    when: windowShown
    visible: true
    width: 900
    height: 700
    property bool visibleDuringDispatch: false
    Connections {
        target: SessionService
        function onDispatched(action) { visibleDuringDispatch = WindowSurface.visible }
    }
    Connections {
        target: LauncherSurface
        function onShown(monitor) { visibleDuringDispatch = WindowSurface.visible }
    }
    Connections {
        target: SettingsNavigationService
        function onLastOpenedPageChanged() { visibleDuringDispatch = WindowSurface.visible }
    }
    property var overlay
    Component { id: overlayComponent; WindowOverlay { width: 900; height: 700 } }
    Component { id: backgroundComponent; Background { width: 900; height: 700; imagePath: ""; monitorName: "TEST-2"; desktopMenuEnabled: true } }
    SignalSpy { id: sessionSpy; target: SessionService; signalName: "dispatched" }
    SignalSpy { id: launcherSpy; target: LauncherSurface; signalName: "shown" }
    SignalSpy { id: settingsSpy; target: SettingsNavigationService; signalName: "lastOpenedPageChanged" }
    function init() {
        WindowSurface.hide()
        visibleDuringDispatch = false
        mouseMove(this, 850, 650)
        SessionService.lockerAvailable = true
        SessionService.logoutSupported = true
        sessionSpy.clear(); launcherSpy.clear(); settingsSpy.clear()
    }
    function cleanup() { WindowSurface.hide() }
    function makeMenu(x, y) {
        WindowSurface.desktopMenu("TEST-2", x, y)
        overlay = createTemporaryObject(overlayComponent, this)
        verify(overlay !== null)
        const menu = findChild(overlay, "desktopMenu")
        menu.forceActiveFocus()
        return menu
    }
    function test_clickPropagationAndOptIn() {
        const background = createTemporaryObject(backgroundComponent, this)
        mouseClick(background, 137, 243, Qt.RightButton)
        compare(WindowSurface.screenName, "TEST-2")
        compare(WindowSurface.menuPosition.x, 137)
        compare(WindowSurface.menuPosition.y, 243)
        WindowSurface.hide()
        background.desktopMenuEnabled = false
        mouseClick(background, 137, 243, Qt.RightButton)
        verify(!WindowSurface.visible)
    }
    function test_placement_data() {
        return [{tag: "topLeft", x: 0, y: 0}, {tag: "topRight", x: 899, y: 0},
                {tag: "bottomLeft", x: 0, y: 699}, {tag: "bottomRight", x: 899, y: 699},
                {tag: "insideMarginThreshold", x: 671, y: 577},
                {tag: "pastMarginThreshold", x: 673, y: 579},
                {tag: "interior", x: 200, y: 200}]
    }
    function test_placement(data) {
        const menu = makeMenu(data.x, data.y)
        compare(overlay.color.a, 0)
        const panel = findChild(menu, "desktopMenuPanel")
        compare(panel.x, Math.min(data.x, 672))
        compare(panel.y, Math.min(data.y, 578))
        menu.openPower()
        const power = findChild(menu, "desktopPowerPanel")
        verify(power.x >= 0 && power.x + power.width <= 892)
        verify(power.y >= 0 && power.y + power.height <= 692)
        compare(power.x, panel.x + panel.width + power.width > 892 ? panel.x - power.width : panel.x + panel.width)
    }
    function test_centerFallback() {
        WindowSurface.desktopMenu("TEST-1")
        overlay = createTemporaryObject(overlayComponent, this)
        const panel = findChild(overlay, "desktopMenuPanel")
        compare(panel.x, 340); compare(panel.y, 293)
    }
    function test_navigationCapabilitiesAndEscape() {
        const menu = makeMenu(100, 100)
        keyClick(Qt.Key_Up); compare(menu.selectedIndex, 2)
        keyClick(Qt.Key_Right); verify(menu.submenuOpen)
        SessionService.lockerAvailable = false
        SessionService.logoutSupported = false
        verify(!findChild(menu, "desktopPowerRow0").enabled)
        verify(!findChild(menu, "desktopPowerRow2").enabled)
        keyClick(Qt.Key_Down); compare(menu.powerIndex, 1)
        keyClick(Qt.Key_Down); compare(menu.powerIndex, 3)
        keyClick(Qt.Key_Left); verify(!menu.submenuOpen)
        keyClick(Qt.Key_Return); verify(menu.submenuOpen)
        keyClick(Qt.Key_Escape); verify(!menu.submenuOpen); verify(WindowSurface.visible)
        keyClick(Qt.Key_Escape); verify(!WindowSurface.visible)
    }
    function test_outsideAndInsideDismissal() {
        const menu = makeMenu(100, 100)
        mouseClick(overlay, 101, 101); verify(WindowSurface.visible)
        menu.openPower()
        mouseClick(overlay, 850, 650, Qt.RightButton); verify(!WindowSurface.visible)
    }
    function test_launcherAndSettingsDispatch() {
        let menu = makeMenu(100, 100)
        keyClick(Qt.Key_Return)
        compare(launcherSpy.count, 1)
        verify(!visibleDuringDispatch)
        compare(launcherSpy.signalArguments[0][0], "TEST-2")
        verify(!WindowSurface.visible)
        menu.activate(); compare(launcherSpy.count, 1)
        overlay.destroy()
        menu = makeMenu(100, 100)
        keyClick(Qt.Key_Down); keyClick(Qt.Key_Enter)
        compare(SettingsNavigationService.lastOpenedPage, "bar")
        compare(settingsSpy.count, 1)
        verify(!visibleDuringDispatch)
        verify(!WindowSurface.visible)
    }
    function test_disabledAndPointerActivation() {
        const menu = makeMenu(100, 100)
        SessionService.lockerAvailable = false
        menu.openPower()
        menu.powerIndex = 0
        menu.activate()
        compare(sessionSpy.count, 0)
        verify(WindowSurface.visible)
        const disabled = findChild(menu, "desktopPowerRow0")
        verify(!disabled.enabled)
        waitForRendering(menu)
        mouseClick(disabled, 50, 16)
        compare(sessionSpy.count, 0)
        const suspend = findChild(menu, "desktopPowerRow1")
        mouseClick(suspend, 50, 16)
        compare(sessionSpy.count, 1)
        compare(sessionSpy.signalArguments[0][0], "sleep")
        verify(!WindowSurface.visible)
    }
    function test_powerDispatch_data() {
        return [{tag: "lock", index: 0, action: "lock"}, {tag: "suspend", index: 1, action: "sleep"},
                {tag: "logout", index: 2, action: "logout"}, {tag: "restart", index: 3, action: "reboot"},
                {tag: "shutdown", index: 4, action: "shutdown"}]
    }
    function test_powerDispatch(data) {
        const menu = makeMenu(100, 100)
        menu.openPower(); menu.powerIndex = data.index
        keyClick(Qt.Key_Return)
        compare(sessionSpy.count, 1)
        verify(!visibleDuringDispatch)
        compare(sessionSpy.signalArguments[0][0], data.action)
        verify(!WindowSurface.visible)
        menu.activate(); compare(sessionSpy.count, 1)
    }
}

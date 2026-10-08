pragma ComponentBehavior: Bound
import QtQuick
import Holonight.Core
import HolonightShell

Item {
    id: root
    objectName: "desktopMenu"
    Accessible.role: Accessible.PopupMenu
    readonly property int edgeMargin: 8
    property int selectedIndex: 0
    property int powerIndex: 0
    property bool submenuOpen: false
    property bool dispatched: false
    readonly property var powerActions: [
        { label: "Lock screen", icon: "system-lock-screen-symbolic", action: "lock", available: SessionService.lockerAvailable },
        { label: "Suspend", icon: "system-suspend-symbolic", action: "sleep", available: true },
        { label: "Log out", icon: "system-log-out-symbolic", action: "logout", available: SessionService.logoutSupported },
        { label: "Restart", icon: "system-reboot-symbolic", action: "reboot", available: true },
        { label: "Shut down", icon: "system-shutdown-symbolic", action: "shutdown", available: true }
    ]
    function clamp(value, maximum) { return Math.max(0, Math.min(value, maximum)) }
    function openPower() {
        selectedIndex = 3
        submenuOpen = true
        powerIndex = powerActions.findIndex(action => action.available)
    }
    function dispatch(action) {
        if (dispatched || !WindowSurface.visible) return
        const screen = WindowSurface.screenName
        dispatched = true
        WindowSurface.hide()
        switch (action) {
        case "applications": LauncherSurface.show(screen); break
        case "wallpaper": SettingsNavigationService.openWallpaper(screen); break
        case "settings": SettingsNavigationService.openPage("bar"); break
        case "lock": SessionService.lockScreen(); break
        case "sleep": SessionService.sleep(); break
        case "logout": SessionService.logout(); break
        case "reboot": SessionService.reboot(); break
        case "shutdown": SessionService.shutdown(); break
        }
    }
    function activate() {
        if (submenuOpen) {
            if (powerActions[powerIndex].available) dispatch(powerActions[powerIndex].action)
        } else if (selectedIndex === 3) openPower()
        else dispatch(["applications", "settings", "wallpaper"][selectedIndex])
    }
    function move(delta) {
        if (!submenuOpen) selectedIndex = (selectedIndex + delta + 4) % 4
        else {
            do { powerIndex = (powerIndex + delta + powerActions.length) % powerActions.length }
            while (!powerActions[powerIndex].available)
        }
    }
    Keys.onPressed: event => {
        switch (event.key) {
        case Qt.Key_Escape:
            if (submenuOpen) submenuOpen = false
            else WindowSurface.hide()
            break
        case Qt.Key_Left: submenuOpen = false; break
        case Qt.Key_Right: if (selectedIndex === 3 && !submenuOpen) openPower(); break
        case Qt.Key_Down: move(1); break
        case Qt.Key_Up: move(-1); break
        case Qt.Key_Return:
        case Qt.Key_Enter: activate(); break
        default: return
        }
        event.accepted = true
    }
    Rectangle {
        id: panel
        objectName: "desktopMenuPanel"
        width: Math.min(220, Math.max(0, root.width - root.edgeMargin))
        height: 146
        x: root.clamp(WindowSurface.menuPosition.x < 0 ? (root.width - width) / 2 : WindowSurface.menuPosition.x, root.width - width - root.edgeMargin)
        y: root.clamp(WindowSurface.menuPosition.y < 0 ? (root.height - height) / 2 : WindowSurface.menuPosition.y, root.height - height - root.edgeMargin)
        radius: 8
        color: HoloniightPalette.surface
        border.color: HoloniightPalette.borderSubtle
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }
        Column {
            anchors.fill: parent
            anchors.margins: 4
            DesktopMenuRow {
                label: "Applications"; iconName: "applications"; highlighted: root.selectedIndex === 0
                onHovered: { root.selectedIndex = 0; root.submenuOpen = false }
                onTriggered: root.dispatch("applications")
            }
            DesktopMenuRow {
                label: "HoloNight settings"; iconName: "settings"; highlighted: root.selectedIndex === 1
                onHovered: { root.selectedIndex = 1; root.submenuOpen = false }
                onTriggered: root.dispatch("settings")
            }
            DesktopMenuRow {
                label: qsTr("Change Wallpaper…"); iconName: "settings"; highlighted: root.selectedIndex === 2
                onHovered: { root.selectedIndex = 2; root.submenuOpen = false }
                onTriggered: root.dispatch("wallpaper")
            }
            Item {
                width: parent.width; height: 10
                Rectangle { anchors.centerIn: parent; width: parent.width - 12; height: 1; color: HoloniightPalette.borderSubtle }
            }
            DesktopMenuRow {
                label: "Power"; iconName: "system-shutdown-symbolic"; submenu: true; highlighted: root.selectedIndex === 3
                onHovered: root.openPower()
                onTriggered: root.openPower()
            }
        }
    }
    Rectangle {
        id: powerPanel
        objectName: "desktopPowerPanel"
        visible: root.submenuOpen
        width: Math.min(220, Math.max(0, root.width - root.edgeMargin))
        height: 168
        x: root.clamp(panel.x + panel.width + width <= root.width - root.edgeMargin ? panel.x + panel.width : panel.x - width, root.width - width - root.edgeMargin)
        y: root.clamp(panel.y + 110, root.height - height - root.edgeMargin)
        radius: 8
        color: HoloniightPalette.surface
        border.color: HoloniightPalette.borderSubtle
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }
        Column {
            anchors.fill: parent
            anchors.margins: 4
            Repeater {
                model: root.powerActions
                DesktopMenuRow {
                    required property var modelData
                    required property int index
                    objectName: "desktopPowerRow" + index
                    label: modelData.label
                    iconName: modelData.icon
                    enabled: modelData.available
                    highlighted: root.powerIndex === index
                    onHovered: root.powerIndex = index
                    onTriggered: root.dispatch(modelData.action)
                }
            }
        }
    }
}

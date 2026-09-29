import QtQuick
import QtTest
import HolonightShell
import Holonight.Core

TestCase {
    id: root
    name: "LogoSection"
    when: windowShown
    visible: true
    width: 480
    height: 120

    readonly property string bundledLogo: "qrc:/HolonightShell/linux-logo/archlinux.svg"

    Component {
        id: logoSectionComponent

        LogoSection {
            x: 24
            y: 24
            barMonitorName: "TEST-1"
        }
    }

    SignalSpy {
        id: toggleSpy
        target: LauncherSurface
        signalName: "toggled"
    }

    function init() {
        toggleSpy.clear()
    }

    function cleanup() {
        TooltipSurface.hide()
        SystemInfoService.setLogo(bundledLogo, true)
    }

    function test_icon_and_label_toggle_on_their_monitor_data() {
        return [
            { tag: "icon on first monitor", objectName: "logoIcon", monitor: "TEST-1" },
            { tag: "label on second monitor", objectName: "logoLabel", monitor: "TEST-2" }
        ]
    }

    function test_icon_and_label_toggle_on_their_monitor(data) {
        const section = createTemporaryObject(logoSectionComponent, root, { barMonitorName: data.monitor })
        verify(section)
        const target = findChild(section, data.objectName)
        verify(target)
        mouseClick(target, target.width / 2, target.height / 2)
        compare(toggleSpy.count, 1)
        compare(toggleSpy.signalArguments[0][0], data.monitor)
    }

    function test_inset_rectangle_boundaries_data() {
        // Default frame: 1 px inset, 2 px stroke, 12 px cuts, 4 px right rounding.
        return [
            { tag: "left edge", x: 14, y: 32, toggles: true },
            { tag: "top edge", x: 60, y: 2, toggles: true },
            { tag: "right interior", x: -19, y: 32, toggles: true },
            { tag: "bottom interior", x: 60, y: 61, toggles: true },
            { tag: "top left interior", x: 14, y: 2, toggles: true },
            { tag: "bottom right interior", x: -19, y: 61, toggles: true },
            { tag: "left end", x: 13, y: 32, toggles: false },
            { tag: "left chamfer", x: 8, y: 8, toggles: false },
            { tag: "right end", x: -17, y: 32, toggles: false },
            { tag: "right rounding", x: -8, y: 8, toggles: false },
            { tag: "top border", x: 60, y: 1, toggles: false },
            { tag: "bottom border", x: 60, y: 63, toggles: false },
            { tag: "outside left", x: -2, y: 32, outsideLeft: true, toggles: false },
            { tag: "outside right", x: 2, y: 32, outsideRight: true, toggles: false },
            { tag: "outside top", x: 60, y: -2, toggles: false },
            { tag: "outside bottom", x: 60, y: 66, toggles: false }
        ]
    }

    function test_inset_rectangle_boundaries(data) {
        const section = createTemporaryObject(logoSectionComponent, root)
        verify(section)
        const frame = findChild(section, "logoFrame")
        verify(frame)
        const x = data.outsideRight ? frame.width + data.x
                                   : data.x < 0 && !data.outsideLeft ? frame.width + data.x : data.x
        const point = frame.mapToItem(root, x, data.y)
        mouseClick(root, point.x, point.y)
        compare(toggleSpy.count, data.toggles ? 1 : 0)
        if (data.toggles)
            compare(toggleSpy.signalArguments[0][0], "TEST-1")
    }

    function test_other_mouse_buttons_do_not_toggle_data() {
        return [
            { tag: "right button", button: Qt.RightButton },
            { tag: "middle button", button: Qt.MiddleButton }
        ]
    }

    function test_other_mouse_buttons_do_not_toggle(data) {
        const section = createTemporaryObject(logoSectionComponent, root)
        verify(section)
        mouseClick(section, section.width / 2, section.height / 2, data.button)
        compare(toggleSpy.count, 0)
    }

    function test_click_dismisses_tooltip_until_pointer_leaves() {
        const section = createTemporaryObject(logoSectionComponent, root)
        verify(section)
        mouseMove(root, 0, 0)
        mouseMove(section, section.width / 2, section.height / 2)
        tryCompare(TooltipSurface, "tooltipVisible", true)
        verify(TooltipSurface.description.includes("Click to toggle launcher"))
        mouseClick(section, section.width / 2, section.height / 2)
        compare(toggleSpy.count, 1)
        compare(TooltipSurface.tooltipVisible, false)
        mouseMove(section, section.width / 2 + 1, section.height / 2)
        // Cover the tooltip's 450 ms hover delay before leaving and re-entering.
        wait(500)
        compare(TooltipSurface.tooltipVisible, false)
        mouseMove(root, 0, 0)
        mouseMove(section, section.width / 2, section.height / 2)
        tryCompare(TooltipSurface, "tooltipVisible", true)
    }

    function test_hnicon_reflects_system_info_service_logo_properties_data() {
        return [
            {
                tag: "bundled logo",
                source: bundledLogo,
                tinted: true
            },
            {
                tag: "file override",
                source: "file:///tmp/custom-logo.png",
                tinted: false
            },
            {
                tag: "pixmaps fallback",
                source: "file:///usr/share/pixmaps/linux.png",
                tinted: false
            }
        ]
    }

    function test_hnicon_reflects_system_info_service_logo_properties(data) {
        SystemInfoService.setLogo(data.source, data.tinted)
        const section = createTemporaryObject(logoSectionComponent, null)
        verify(section)

        const icon = findChild(section, "logoIcon")
        verify(icon)
        compare(icon.source, data.source)
        compare(icon.rendering, data.tinted ? HnIcon.Semantic : HnIcon.Original)
    }
}

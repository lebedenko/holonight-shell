import QtQuick
import QtTest
import HolonightShell

TestCase {
    name: "WorkspaceSectionQmlTests"

    visible: true
    width: 600
    height: 80
    when: windowShown

    SignalSpy {
        id: activationSpy
        target: NumberedTestProvider
        signalName: "slotActivated"
    }

    function cleanup() {
        WorkspacePresentation.workspaceDisplayCount = 5
        activationSpy.clear()
    }

    Component {
        id: sectionComponent
        WorkspaceSection { barMonitorName: "DP-1"; width: 500; height: 64 }
    }

    function test_injected_provider_uses_numeric_workspace_presentation() {
        const section = createTemporaryObject(sectionComponent, this)
        verify(section !== null)
        verify(CompositorService.connected)
        compare(section.numericMode, true)
        verify(section.item !== null)
        compare(section.item.activeWorkspaceId, 1)
        compare(section.item.windowStart, 1)
        const strip = findChild(section, "pillStrip")
        const repeater = findChild(strip, "pillRepeater")
        verify(repeater !== null)
        let visiblePills = 0
        for (let i = 0; i < repeater.count; ++i) {
            if (repeater.itemAt(i).glowAllowed)
                ++visiblePills
        }
        compare(visiblePills, 5)
        compare(repeater.itemAt(0).visualState, "focused-active")
        const emptyPill = repeater.itemAt(4)
        compare(emptyPill.visualState, "empty")
        mouseClick(emptyPill, emptyPill.width / 2, emptyPill.height / 2)
        compare(activationSpy.count, 1)
        compare(activationSpy.signalArguments[0][0], 5)
        WorkspacePresentation.workspaceDisplayCount = 3
        tryCompare(strip, "lastStripId", 4)
        WorkspacePresentation.workspaceDisplayCount = 7
        tryCompare(strip, "lastStripId", 8)
    }
}

import QtQuick
import QtTest
import HolonightShell
import Holonight.Controls

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

    QtObject {
        id: contributionModel
        property int contentWidth: 0
        property bool shown: true
    }

    Component {
        id: numericSectionComponent
        NumericWorkspaceSection { barMonitorName: "DP-1"; height: 64 }
    }

    function test_contribution_shares_frame_and_collapses() {
        contributionModel.contentWidth = 0
        contributionModel.shown = true
        const section = createTemporaryObject(numericSectionComponent, this)
        verify(section !== null)
        const host = findChild(section, "contributionHost")
        const separator = findChild(section, "separatorLoader")
        const left = findChild(section, "leftArrow")
        const strip = findChild(section, "pillStrip")
        const right = findChild(section, "rightArrow")
        compare(host.width, 0)
        compare(separator.active, false)
        const emptyWidth = section.implicitWidth
        const emptyLeft = left.x
        host.contributionModel = contributionModel
        host.componentUrl = Qt.resolvedUrl("fixtures/WorkspaceContribution.qml")
        tryCompare(host, "status", Loader.Ready)
        compare(host.width, 0)
        compare(section.implicitWidth, emptyWidth)

        contributionModel.contentWidth = 72
        tryCompare(host, "width", 72)
        tryVerify(() => separator.item !== null)
        compare(separator.item.orientation, Qt.Vertical)
        compare(separator.item.fadeMode, HnSeparator.FadeBoth)
        compare(separator.height, 48)
        tryCompare(section, "implicitWidth", emptyWidth + 72 + separator.width + 16)
        tryVerify(() => host.x + host.width <= separator.x)
        tryVerify(() => separator.x + separator.width <= left.x)
        verify(left.x + left.width <= strip.x)
        verify(strip.x + strip.width <= right.x)
        compare(host.mapToItem(section, 0, host.height / 2).y, section.height / 2)
        compare(separator.mapToItem(section, 0, separator.height / 2).y, section.height / 2)
        compare(host.item.barMonitorName, "DP-1")
        section.barMonitorName = "DP-2"
        compare(host.item.barMonitorName, "DP-2")
        section.barMonitorName = "DP-1"

        contributionModel.shown = false
        tryCompare(host, "width", 0)
        tryCompare(section, "implicitWidth", emptyWidth)
        contributionModel.shown = true
        tryCompare(host, "width", 72)
        contributionModel.contentWidth = 0
        tryCompare(host, "width", 0)
        tryCompare(separator, "active", false)
        tryCompare(section, "implicitWidth", emptyWidth)
        tryCompare(left, "x", emptyLeft)
        host.componentUrl = ""
        tryCompare(host, "active", false)
        compare(section.implicitWidth, emptyWidth)
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

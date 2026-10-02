import QtQuick
import QtTest
import QtQuick.Layouts
import HolonightShell
import Holonight.Controls
import Holonight.Core

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

    function init() {
        NumberedTestProvider.eligible = true
        CompositorTestSeed.reset()
    }

    function cleanup() {
        NumberedTestProvider.eligible = true
        CompositorTestSeed.reset()
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

    Component {
        id: layoutComponent
        RowLayout {
            spacing: 0
            Item { implicitWidth: 20; implicitHeight: 64 }
            WorkspaceSection {
                objectName: "workspaceSection"
                barMonitorName: "DP-1"
                Layout.leftMargin: -12
            }
            Item { objectName: "followingItem"; implicitWidth: 20; implicitHeight: 64 }
        }
    }

    function test_named_visibility_transitions_leave_no_gap() {
        NumberedTestProvider.eligible = false
        CompositorTestSeed.setNamedWorkspaces(0)
        const layout = createTemporaryObject(layoutComponent, this)
        const section = findChild(layout, "workspaceSection")
        const following = findChild(layout, "followingItem")
        for (const count of [0, 1, 3, 1, 2, 0]) {
            CompositorTestSeed.setNamedWorkspaces(count)
            tryCompare(section, "visible", count >= 2)
            compare(section.active, count >= 2)
            if (count < 2) {
                compare(section.item, null)
                compare(section.implicitWidth, 0)
                tryCompare(following, "x", 20)
                tryCompare(layout, "implicitWidth", 40)
            } else {
                verify(section.item !== null)
                verify(section.implicitWidth > 0)
            }
        }
        for (const flags of [[false, true], [true, false]]) {
            CompositorTestSeed.setNamedWorkspaces(3, 0, -1, flags[0], flags[1])
            tryCompare(section, "active", false)
            compare(section.item, null)
            tryCompare(layout, "implicitWidth", 40)
        }
    }

    function test_named_labels_identity_states_and_active_anchor() {
        NumberedTestProvider.eligible = false
        CompositorTestSeed.setNamedWorkspaces(7, 5)
        WorkspacePresentation.workspaceDisplayCount = 3
        const section = createTemporaryObject(sectionComponent, this)
        compare(section.numericMode, false)
        compare(section.item.firstRow, 4)
        CompositorTestSeed.setNamedWorkspaces(3, 0)
        const repeater = findChild(section, "namedRepeater")
        const active = repeater.itemAt(0).item
        const urgent = repeater.itemAt(1).item
        const inactive = repeater.itemAt(2).item
        compare(active.label, "Desktop 1")
        compare(active.workspaceId, "opaque-0")
        compare(active.visualState, "active")
        compare(active.resolvedBorderColor, HoloniightPalette.accentCyan)
        compare(active.resolvedTextColor, HoloniightPalette.textPrimary)
        tryVerify(() => active.glowOpacity > 0)
        compare(active.tooltipDescription, "Active workspace.")
        compare(urgent.visualState, "urgent")
        compare(urgent.resolvedBorderWidth, 1.4)
        compare(inactive.tooltipDescription, "Inactive workspace.")
        const spy = Qt.createQmlObject('import QtTest; SignalSpy {}', this)
        spy.target = CompositorService
        spy.signalName = "workspaceActivationRequested"
        mouseClick(inactive, inactive.width / 2, inactive.height / 2)
        compare(spy.count, 1)
        compare(spy.signalArguments[0][0], "opaque-2")
        spy.destroy()
        CompositorTestSeed.setNamedWorkspaces(3, 2)
        compare(repeater.itemAt(0).item.resolvedBorderColor, HoloniightPalette.borderPassive)
        compare(repeater.itemAt(2).item.resolvedBorderColor, HoloniightPalette.accentCyan)
        compare(repeater.itemAt(2).item.tooltipDescription, "Active workspace.")
        CompositorTestSeed.setNamedWorkspaces(7, 5, 1)
        compare(section.item.firstRow, 0)
        CompositorTestSeed.setNamedWorkspaces(7, -1)
        compare(section.item.firstRow, 0)
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

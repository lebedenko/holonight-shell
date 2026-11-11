import QtQuick
import QtTest
import HolonightShell
TestCase {
    id: testCase
    name: "PackageUpdatesWidget"
    when: windowShown
    visible: true
    width: 200
    height: 100
    QtObject {
        id: fakeProvider
        property bool available: false
        property bool loading: false
        property bool checking: false
        property int count: 0
        property int ignoredCount: 2
        property real downloadSizeBytes: 1048576
        property real dataAsOf: 0
        property real lastCheckTime: 0
        property real snapshotFetchedAt: 0
        property bool checkMetadataAvailable: false
        property bool lastCheckSucceeded: false
        property string lastCheckError: ""
        property string errorMessage: ""
        property string launchError: ""
        property int launches: 0
        function openPackages() { launches++ }
    }
    Component {
        id: widgetComponent
        PackageUpdatesWidget { barMonitorName: "test"; service: fakeProvider }
    }
    function test_states() {
        const widget = createTemporaryObject(widgetComponent, testCase)
        verify(widget !== null)
        compare(widget.visible, false)
        compare(widget.implicitWidth, 0)
        fakeProvider.available = true
        compare(widget.visible, false)
        fakeProvider.checking = true
        compare(widget.visible, true)
        fakeProvider.checking = false
        fakeProvider.checkMetadataAvailable = true
        fakeProvider.lastCheckTime = 1234
        fakeProvider.lastCheckError = "network-unavailable"
        verify(widget.problem)
        verify(widget.tooltipText.includes("network-unavailable"))
        fakeProvider.lastCheckSucceeded = true
        compare(widget.visible, false)
        fakeProvider.loading = true
        compare(widget.visible, true)
        fakeProvider.loading = false
        fakeProvider.count = 123
        compare(widget.badgeText, "99+")
        verify(widget.tooltipText.includes("123 pending updates"))
        verify(widget.tooltipText.includes("2 ignored"))
        verify(widget.tooltipText.includes("unknown"))
        fakeProvider.errorMessage = "broken"
        verify(widget.problem)
        verify(widget.tooltipText.includes("broken"))
        fakeProvider.count = 0
        compare(widget.visible, true)
        fakeProvider.errorMessage = ""
        compare(widget.visible, false)
        fakeProvider.launchError = "launch failed"
        compare(widget.visible, true)
        TooltipSurface.show("test", 0, 44, "Packages", widget.tooltipText, "", 100, false, 100)
        verify(TooltipSurface.tooltipVisible)
        mouseClick(widget, 20, 10, Qt.LeftButton)
        compare(TooltipSurface.tooltipVisible, false)
        compare(fakeProvider.launches, 1)
        fakeProvider.launchError = ""
        compare(widget.implicitWidth, 0)
    }
}

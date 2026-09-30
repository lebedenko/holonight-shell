pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell

Item {
    id: root

    required property string barMonitorName
    required property int windowStart        // first visible absolute workspace id (>= 1)

    readonly property int pillSize: 32
    readonly property int pillSpacing: 16
    readonly property int pillStep: root.pillSize + root.pillSpacing
    readonly property int glowMargin: 10
    readonly property int stripPad: 1         // keeps a real pill on each valid side of the viewport
    property int renderedWindowStart: root.windowStart
    property bool rebasingRange: false

    implicitWidth: WorkspacePresentation.workspaceDisplayCount * root.pillStep - root.pillSpacing + root.glowMargin * 2
    implicitHeight: root.pillSize + root.glowMargin * 2
    clip: true

    readonly property int firstStripId: Math.max(1, root.renderedWindowStart - root.stripPad)
    readonly property int lastStripId: root.renderedWindowStart + WorkspacePresentation.workspaceDisplayCount - 1 + root.stripPad
    readonly property int stripCount: Math.max(0, root.lastStripId - root.firstStripId + 1)

    onWindowStartChanged: {
        const windowEnd = root.windowStart + WorkspacePresentation.workspaceDisplayCount - 1
        if (root.windowStart < root.firstStripId || windowEnd > root.lastStripId) {
            root.rebasingRange = true
            root.renderedWindowStart = root.windowStart
            rangeReenableTimer.restart()
            return
        }
        rangeRebaseTimer.restart()
    }

    Item {
        id: strip
        objectName: "stripInner"
        width: root.stripCount * root.pillStep
        height: root.implicitHeight
        x: root.glowMargin + (root.firstStripId - root.windowStart) * root.pillStep

        Behavior on x {
            enabled: !root.rebasingRange
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        Repeater {
            id: pillRepeater
            objectName: "pillRepeater"
            model: WorkspacePresentation.revision >= 0
                ? WorkspacePresentation.numberedSlots(root.firstStripId, root.stripCount) : []
            delegate: WorkspacePill {
                required property int index
                required property var modelData
                readonly property int absoluteId: modelData.slot
                readonly property real viewportLeft: -strip.x
                readonly property real viewportRight: viewportLeft + root.width
                x: index * root.pillStep
                y: root.glowMargin
                workspaceId: modelData.workspaceId
                onActivated: WorkspacePresentation.activateNumberedSlot(absoluteId)
                numericSlot: absoluteId
                label: String(absoluteId)
                barMonitorName: root.barMonitorName
                visualState: modelData.visualState
                glowAllowed: x >= viewportLeft && x + width <= viewportRight
            }
        }
    }

    Timer {
        id: rangeRebaseTimer
        interval: 200
        onTriggered: {
            root.rebasingRange = true
            root.renderedWindowStart = root.windowStart
            rangeReenableTimer.restart()
        }
    }

    Timer {
        id: rangeReenableTimer
        interval: 0
        onTriggered: root.rebasingRange = false
    }
}

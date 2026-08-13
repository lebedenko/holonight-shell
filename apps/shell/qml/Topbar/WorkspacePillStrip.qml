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

    implicitWidth: WorkspaceModel.displayCount * root.pillStep - root.pillSpacing + root.glowMargin * 2
    implicitHeight: root.pillSize + root.glowMargin * 2
    clip: true

    readonly property int firstStripId: Math.max(1, root.renderedWindowStart - root.stripPad)
    readonly property int lastStripId: root.renderedWindowStart + WorkspaceModel.displayCount - 1 + root.stripPad
    readonly property int stripCount: Math.max(0, root.lastStripId - root.firstStripId + 1)

    onWindowStartChanged: {
        const windowEnd = root.windowStart + WorkspaceModel.displayCount - 1
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
            model: root.stripCount
            delegate: WorkspacePill {
                required property int index
                readonly property int absoluteId: root.firstStripId + index
                readonly property real viewportLeft: -strip.x
                readonly property real viewportRight: viewportLeft + root.width
                x: index * root.pillStep
                y: root.glowMargin
                workspaceId: String(absoluteId)
                numericSlot: absoluteId
                label: String(absoluteId)
                workspaceKind: "normal"
                barMonitorName: root.barMonitorName
                visualState: {
                    const state = WorkspaceModel.stateForId(absoluteId)
                    if (state === WorkspaceModel.FocusedActiveMonitor || state === WorkspaceModel.Active)
                        return "focused"
                    if (state === WorkspaceModel.Urgent) return "urgent"
                    if (state === WorkspaceModel.Occupied) return "occupied"
                    return "empty"
                }
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

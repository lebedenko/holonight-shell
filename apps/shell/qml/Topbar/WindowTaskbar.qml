import QtQuick
import QtQuick.Controls as Controls
import Holonight.Core
import HolonightShell
import "../Controls"

BarSection {
    id: root
    required property string barMonitorName
    visible: WindowPresentation.taskbarEnabled
    readonly property int iconSize: height - 16 >= 48 ? 48 : height - 16 >= 32 ? 32 : 24
    readonly property int slotSize: iconSize + 8
    readonly property int minimumContentWidth: 48 + slotSize + 4 + 40
    implicitWidth: minimumContentWidth

    function globalAnchor(item) {
        const point = item.mapToGlobal(0, 0)
        return Qt.rect(point.x, point.y, item.width, item.height)
    }

    BarFrame {
        anchors.fill: parent
        anchors.leftMargin: -8
        anchors.rightMargin: -8
        leftTopOffset: 12
        rightBottomOffset: 12
    }

    Controls.Button {
        id: overview
        hoverEnabled: true
        objectName: "windowListButton"
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        width: 48
        height: root.height - 8
        background: Rectangle {
            radius: 6
            color: overview.down ? Qt.rgba(HoloniightPalette.accentBlue.r, HoloniightPalette.accentBlue.g, HoloniightPalette.accentBlue.b, 0.24)
                : overview.hovered ? Qt.rgba(HoloniightPalette.accentBlue.r, HoloniightPalette.accentBlue.g, HoloniightPalette.accentBlue.b, 0.12) : "transparent"
            Behavior on color { ColorAnimation { duration: 90 } }
        }
        contentItem: Item {
            Column {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: overview.down ? 1 : 0
                spacing: 4
                Repeater {
                    model: 3
                    Rectangle { width: 4; height: 4; radius: 2; color: HoloniightPalette.textPrimary }
                }
            }
        }
        BarTooltipArea { id: overviewTooltip; barMonitorName: root.barMonitorName; title: "All windows" }
        onClicked: {
            overviewTooltip.dismissForClick()
            if (WindowPresentation.overviewAccess) WindowSurface.toggle(root.barMonitorName, root.globalAnchor(overview))
            else WindowSurface.chooser(WindowPresentation.windowIds, root.barMonitorName, root.globalAnchor(overview))
        }
    }

    ListView {
        id: list
        objectName: "windowTaskList"
        anchors.left: overview.right
        anchors.leftMargin: 4
        anchors.right: parent.right
        anchors.rightMargin: 12
        height: root.height
        orientation: ListView.Horizontal
        clip: true
        spacing: 4
        model: WindowPresentation.applications
        boundsBehavior: Flickable.StopAtBounds
        onWidthChanged: contentX = Math.max(0, Math.min(contentX, contentWidth - width))
        Controls.ScrollBar.horizontal: Controls.ScrollBar {}
        delegate: Controls.Button {
            id: task
            hoverEnabled: true
            required property string appId
            required property string title
            required property var windows
            required property bool active
            readonly property int minimizedCount: windows.filter(w => w.minimized).length
            property string resolvedIcon: LauncherService.iconForAppId(appId)
            width: root.slotSize
            height: root.height
            padding: 0
            background: Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: root.height - 8
                radius: 6
                color: task.down ? Qt.rgba(HoloniightPalette.accentBlue.r, HoloniightPalette.accentBlue.g, HoloniightPalette.accentBlue.b, 0.24)
                    : task.hovered ? Qt.rgba(HoloniightPalette.accentBlue.r, HoloniightPalette.accentBlue.g, HoloniightPalette.accentBlue.b, 0.12) : "transparent"
                Behavior on color { ColorAnimation { duration: 90 } }
            }
            contentItem: Item {
                AppWindowIcon {
                    objectName: "taskbarAppIcon"
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: task.down ? 1 : 0
                    width: root.iconSize
                    height: root.iconSize
                    appId: task.appId
                    iconName: task.resolvedIcon
                }
                Rectangle {
                    objectName: "taskbarCountBadge"
                    visible: task.windows.length > 1
                    anchors.top: parent.top
                    anchors.topMargin: 4
                    anchors.right: parent.right
                    anchors.rightMargin: -4
                    width: Math.max(16, countLabel.implicitWidth + 6)
                    height: 16
                    radius: 8
                    color: HoloniightPalette.accentBlue
                    Controls.Label { id: countLabel; anchors.centerIn: parent; text: task.windows.length; font.pointSize: 7.5; color: HoloniightPalette.textPrimary }
                }
                Rectangle {
                    objectName: "taskbarMinimizedDot"
                    visible: task.minimizedCount > 0
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height - 7
                    width: 3; height: 3; radius: 1.5
                    color: HoloniightPalette.textSecondary
                }
                Rectangle {
                    objectName: "taskbarActiveUnderline"
                    visible: task.active
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 1
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: root.iconSize; height: 2
                    color: HoloniightPalette.accentBlue
                }
            }
            Connections {
                target: LauncherService
                function onEntriesUpdated() { task.resolvedIcon = LauncherService.iconForAppId(task.appId) }
            }
            BarTooltipArea {
                id: tooltip
                barMonitorName: root.barMonitorName
                title: task.windows.length === 1 ? task.title || task.appId || "Window" : task.appId || "Windows"
                description: task.windows.length === 1 ? task.appId + (task.minimizedCount ? " · Minimized" : "")
                    : task.windows.length + " windows · " + task.minimizedCount + " minimized"
            }
            onClicked: {
                tooltip.dismissForClick()
                if (windows.length === 1) WindowPresentation.click(windows[0].windowId)
                else WindowSurface.chooser(windows.map(w => w.windowId), root.barMonitorName, root.globalAnchor(task))
            }
            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: {
                    tooltip.dismissForClick()
                    if (task.windows.length === 1) WindowSurface.menu(task.windows[0].windowId, root.barMonitorName, root.globalAnchor(task))
                    else WindowSurface.chooser(task.windows.map(w => w.windowId), root.barMonitorName, root.globalAnchor(task))
                }
            }
        }
    }
}

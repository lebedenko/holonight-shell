pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Components
import Holonight.Controls

import HolonightShell

ColumnLayout {
    id: root

    property var groupedNotifs: []
    readonly property int totalNotificationCount: {
        var total = 0
        for (var index = 0; index < root.groupedNotifs.length; index++) {
            total += root.groupedNotifs[index].totalCount || 0
        }
        return total
    }
    readonly property int notificationOverflowCount: {
        var total = 0
        for (var index = 3; index < root.groupedNotifs.length; index++) {
            total += root.groupedNotifs[index].totalCount || 0
        }
        return total
    }
    property int timeTickCounter: 0

    signal switchTab(int index)

    Layout.fillWidth: true
    spacing: 0

    Component.onCompleted: {
        root.groupedNotifs = NotificationService.recentHistoryGrouped(100)
    }

    Connections {
        target: NotificationService
        function onUnreadCountChanged() {
            root.groupedNotifs = NotificationService.recentHistoryGrouped(100)
        }
        function onNotificationClosed() {
            root.groupedNotifs = NotificationService.recentHistoryGrouped(100)
        }
    }

    Timer {
        interval: 60000
        repeat: true
        running: true
        onTriggered: root.timeTickCounter++
    }

    function formatRelativeTime(timestampMs) {
        var diffMs = Date.now() - timestampMs
        var diffSec = Math.floor(diffMs / 1000)
        if (diffSec < 60) return "just now"
        var diffMin = Math.floor(diffSec / 60)
        if (diffMin < 60) return diffMin + "m ago"
        var diffHr = Math.floor(diffMin / 60)
        if (diffHr < 24) return diffHr + "h ago"
        var diffDay = Math.floor(diffHr / 24)
        return diffDay + "d ago"
    }

    Text {
        text: "// NOTIFICATIONS"
        color: HoloniightPalette.borderActive
        font.family: AppearanceService.titleFont
        font.pointSize: AppearanceService.titleFontSize * 0.75
        Layout.bottomMargin: 6
    }

    Text {
        visible: root.groupedNotifs.length === 0
        text: "No new notifications"
        font.pointSize: 9
        color: HoloniightPalette.textMuted
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 8
        Layout.bottomMargin: 8
    }

    Repeater {
        model: root.groupedNotifs.slice(0, 3)

        delegate: HnListDelegate {
            id: notifRow

            objectName: "overviewNotificationDelegate"
            required property var modelData
            required property int index
            readonly property var notif: modelData

            Layout.fillWidth: true
            title: notifRow.notif.appName
            subtitle: notifRow.notif.latestSummary
            subtitlePresentation: HnListDelegate.SingleLine
            metadata: {
                root.timeTickCounter
                return root.formatRelativeTime(notifRow.notif.latestTimestampMs)
            }
            dividerVisible: index < Math.min(root.groupedNotifs.length, 3) - 1
            leadingContent: Component {
                ExternalIcon {
                    iconName: notifRow.notif.appIcon
                    iconSize: 32
                    tintColor: HoloniightPalette.textSecondary
                    fallbackIconName: "application-x-executable"
                    width: 32
                    height: 32
                }
            }
            trailingContent: Component {
                Item {
                    implicitWidth: unreadBadge.visible ? unreadBadge.width : 0
                    implicitHeight: unreadBadge.visible ? unreadBadge.height : 0
                    Rectangle {
                        id: unreadBadge
                        visible: notifRow.notif.unreadCount > 0
                        width: Math.max(16, unreadText.implicitWidth + 6)
                        height: 16
                        radius: 8
                        color: HoloniightPalette.accentCyan
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            id: unreadText

                            anchors.centerIn: parent
                            text: notifRow.notif.unreadCount
                            font.pointSize: 6.75
                            font.bold: true
                            color: HoloniightPalette.surface
                        }
                    }
                }
            }
        }
    }

    Item {
        visible: root.notificationOverflowCount > 0
        Layout.fillWidth: true
        implicitHeight: 32
        Layout.topMargin: 4

        Text {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: "+" + root.notificationOverflowCount + " notifications"
            font.pointSize: 8.25
            color: HoloniightPalette.textSecondary
        }

        Text {
            id: viewAllLabel

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: "View all"
            font.pointSize: 8.25
            color: viewAllMouseArea.containsMouse
                   ? HoloniightPalette.accentCyan
                   : HoloniightPalette.textMuted

            MouseArea {
                id: viewAllMouseArea

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.switchTab(2)
            }
        }
    }
}

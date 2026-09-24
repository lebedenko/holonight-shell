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

    HnLabel {
        rawText: qsTr("// NOTIFICATIONS")
        role: HnTypographyRole.MicroHeader
        color: HoloniightPalette.borderActive
        Layout.bottomMargin: 6
    }

    HnLabel {
        visible: root.groupedNotifs.length === 0
        rawText: qsTr("No new notifications")
        role: HnTypographyRole.Caption
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
                        height: Math.max(16, unreadText.implicitHeight + 4)
                        radius: height / 2
                        color: HoloniightPalette.accentCyan
                        anchors.verticalCenter: parent.verticalCenter

                        HnLabel {
                            id: unreadText

                            anchors.centerIn: parent
                            rawText: String(notifRow.notif.unreadCount)
                            role: HnTypographyRole.Caption
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
        implicitHeight: Math.max(32, Math.max(overflowLabel.implicitHeight, viewAllLabel.implicitHeight) + 8)
        Layout.topMargin: 4

        HnLabel {
            id: overflowLabel
            anchors.left: parent.left
            anchors.right: viewAllLabel.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            rawText: qsTr("+%1 notifications").arg(root.notificationOverflowCount)
            role: HnTypographyRole.Caption
            color: HoloniightPalette.textSecondary
            elide: Text.ElideRight
        }

        HnLabel {
            id: viewAllLabel

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            rawText: qsTr("View all")
            role: HnTypographyRole.Caption
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

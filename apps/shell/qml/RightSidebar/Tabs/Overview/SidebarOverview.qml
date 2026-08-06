import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import Holonight.Core

import HolonightShell

Item {
    id: root

    property int preferredWidth: 380
    property int preferredHeight: contentColumn.implicitHeight + 24
    readonly property int totalNotificationCount: notificationsSection.totalNotificationCount
    readonly property int notificationOverflowCount: notificationsSection.notificationOverflowCount

    signal switchTab(int index)

    function buildDayModel(year, month, weekStart) {
        return calendarSection.buildDayModel(year, month, weekStart)
    }

    Controls.ScrollView {
        id: contentScroll

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        contentWidth: availableWidth
        contentHeight: contentColumn.implicitHeight

        ColumnLayout {
            id: contentColumn

            width: contentScroll.availableWidth
            height: implicitHeight
            spacing: 0

            SidebarOverviewCalendar {
                id: calendarSection

                Layout.fillWidth: true
                Layout.topMargin: 12
                Layout.leftMargin: 12
                Layout.rightMargin: 12
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: HoloniightPalette.surface
                opacity: 0.4
            }

            SidebarOverviewUpcoming {
                id: upcomingSection

                Layout.fillWidth: true
                Layout.topMargin: 12
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.bottomMargin: 4
                onSwitchTab: function(index) {
                    root.switchTab(index)
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: HoloniightPalette.surface
                opacity: 0.4
            }

            SidebarOverviewNotifications {
                id: notificationsSection

                Layout.fillWidth: true
                Layout.topMargin: 12
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.bottomMargin: 12
                onSwitchTab: function(index) {
                    root.switchTab(index)
                }
            }
        }
    }
}

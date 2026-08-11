pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import QtQuick.Effects
import Holonight.Core
import Holonight.Controls

import HolonightShell

ColumnLayout {
    id: root

    signal switchTab(int index)

    Layout.fillWidth: true
    spacing: 0

    property int tickCounter: 0

    // Only ticks while the sidebar is visible — avoids updating off-screen.
    Timer {
        interval: 60000
        repeat: true
        running: root.visible
        onTriggered: root.tickCounter++
    }

    Component.onCompleted: {
        CalendarService.notifySidebarOpened()
    }

    function formatDuration(startTime, endTime) {
        var start = new Date(startTime)
        var end = new Date(endTime)
        if (isNaN(start.getTime()) || isNaN(end.getTime())) return ""
        var diffMs = end - start
        if (diffMs <= 0) return ""
        var totalMin = Math.round(diffMs / 60000)
        if (totalMin <= 0) return ""
        var hrs = Math.floor(totalMin / 60)
        var mins = totalMin % 60
        if (hrs === 0) return totalMin + " min"
        if (mins === 0) return hrs + "h"
        return hrs + "h " + mins + "m"
    }

    function formatAllDaySecondary(startTime, endTime) {
        var dummy = root.tickCounter
        var now = new Date()
        var start = new Date(startTime)
        var end = new Date(endTime)
        if (!isNaN(end.getTime()) && now >= start && now < end) {
            var tillDays = Math.ceil((end - now) / 86400000)
            if (tillDays <= 1) return "Till tomorrow"
            return "Till " + Qt.formatDate(end, "d MMM")
        }
        var diffDays = Math.ceil((start - now) / 86400000)
        if (diffDays <= 0) return "Today"
        return diffDays + "d left"
    }

    function getLocationIcon(location) {
        if (!location) return "";
        var loc = location.toLowerCase();
        if (loc.includes("meeting") || loc.includes("teams") || loc.includes("zoom") || loc.includes("meet") || loc.startsWith("http") || loc.includes("online")) {
            return "camera-video-symbolic";
        }
        return "mark-location-symbolic";
    }

    function getEventState(startTime, endTime, isAllDay, index) {
        var dummy = root.tickCounter // force re-evaluation on timer tick
        var now = new Date()
        var start = new Date(startTime)
        var end = new Date(endTime)
        
        if (isNaN(start.getTime()) || isNaN(end.getTime())) {
            return {
                name: "upcoming",
                color: HoloniightPalette.borderPassive,
                text: "",
                progress: 0,
                isHighlighted: false
            }
        }
        
        if (isAllDay) {
            var isCurrentlyActive = now >= start && now < end
            var text = root.formatAllDaySecondary(startTime, endTime)
            var isHighlighted = isCurrentlyActive || (index === 0)
            var col = isCurrentlyActive ? HoloniightPalette.success 
                        : (isHighlighted ? HoloniightPalette.accentBlue 
                                         : Qt.rgba(HoloniightPalette.accentBlue.r, HoloniightPalette.accentBlue.g, HoloniightPalette.accentBlue.b, 0.4))
            return {
                name: isCurrentlyActive ? "active" : "upcoming",
                color: col,
                text: text,
                progress: isCurrentlyActive ? 1.0 : 0,
                isHighlighted: isHighlighted
            }
        }
        
        var isActive = now >= start && now < end
        if (isActive) {
            var diffMs = end - now
            var totalMin = Math.round(diffMs / 60000)
            var text = ""
            if (totalMin <= 0) {
                text = "Ending"
            } else if (totalMin < 60) {
                text = "Ends in " + totalMin + " min"
            } else {
                var hrs = Math.floor(totalMin / 60)
                var mins = totalMin % 60
                text = "Ends in " + hrs + "h" + (mins > 0 ? " " + mins + "m" : "")
            }
            
            var progress = 0.0
            var durationMs = end - start
            if (durationMs > 0) {
                progress = (now - start) / durationMs
            }
            
            return {
                name: "active",
                color: HoloniightPalette.success,
                text: text,
                progress: Math.min(1.0, Math.max(0.0, progress)),
                isHighlighted: true
            }
        }
        
        var diffMs = start - now
        var diffMin = Math.round(diffMs / 60000)
        
        var isHighlighted = (index === 0)
        
        if (diffMin > 0 && diffMin <= 15) {
            return {
                name: "soon",
                color: isHighlighted ? HoloniightPalette.warning : Qt.rgba(HoloniightPalette.warning.r, HoloniightPalette.warning.g, HoloniightPalette.warning.b, 0.4),
                text: "In " + diffMin + " min",
                progress: 0,
                isHighlighted: isHighlighted
            }
        }
        
        // General upcoming
        var text = ""
        var todayDate = now.getDate()
        var tomorrow = new Date()
        tomorrow.setDate(now.getDate() + 1)
        var tomorrowDate = tomorrow.getDate()
        
        var startDay = start.getDate()
        
        if (start.getFullYear() === now.getFullYear() && start.getMonth() === now.getMonth()) {
            if (startDay === todayDate) {
                if (diffMin >= 60) {
                    var hrs = Math.floor(diffMin / 60)
                    text = "In " + hrs + " h"
                } else {
                    text = "In " + diffMin + " min"
                }
            } else if (startDay === tomorrowDate) {
                text = "Tomorrow"
            } else {
                text = Qt.formatDate(start, "d MMM")
            }
        } else {
            text = Qt.formatDate(start, "d MMM")
        }
        
        return {
            name: "upcoming",
            color: isHighlighted ? HoloniightPalette.accentBlue : Qt.rgba(HoloniightPalette.accentBlue.r, HoloniightPalette.accentBlue.g, HoloniightPalette.accentBlue.b, 0.4),
            text: text,
            progress: 0,
            isHighlighted: isHighlighted
        }
    }

    Rectangle {
        id: cardContainer
        Layout.fillWidth: true
        Layout.preferredHeight: containerLayout.implicitHeight + 24 // vertical padding
        color: Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g, HoloniightPalette.surface.b, 0.25)
        border.color: Qt.rgba(HoloniightPalette.borderPassive.r, HoloniightPalette.borderPassive.g, HoloniightPalette.borderPassive.b, 0.15)
        border.width: 1
        radius: 12

        ColumnLayout {
            id: containerLayout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.topMargin: 12
            spacing: 12

            // Header: Icon + Title + View All
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Item {
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16

                    Image {
                        id: headerCalIcon
                        anchors.fill: parent
                        source: "image://icon/x-office-calendar-symbolic"
                        fillMode: Image.PreserveAspectFit
                        visible: false
                    }

                    MultiEffect {
                        anchors.fill: parent
                        source: headerCalIcon
                        colorization: 1.0
                        colorizationColor: HoloniightPalette.accentBlue
                    }
                }

                Text {
                    text: "UPCOMING"
                    color: HoloniightPalette.accentBlue
                    font.family: AppearanceService.titleFont
                    font.pointSize: AppearanceService.titleFontSize * 0.75
                    font.bold: true
                    font.letterSpacing: 0.8
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    text: "View all ›"
                    font.pointSize: 8.25
                    color: viewAllArea.containsMouse ? HoloniightPalette.accentCyan : HoloniightPalette.accentBlue

                    MouseArea {
                        id: viewAllArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.switchTab(2)
                    }
                }
            }

            // Loading state
            HnLoadingState {
                objectName: "upcomingLoadingState"
                Layout.fillWidth: true
                visible: CalendarService.upcomingState === CalendarService.Loading &&
                         (CalendarService.upcomingEvents === null || CalendarService.upcomingEvents.rowCount() === 0)
                running: visible
                titleText: qsTr("Loading upcoming events")
            }

            // Connection Error state
            Item {
                Layout.fillWidth: true
                implicitHeight: 44
                visible: CalendarService.upcomingState === CalendarService.ConnectError

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: "Calendar connection error"
                        font.pointSize: 8.25
                        color: HoloniightPalette.warning
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: CalendarService.lastError
                        font.pointSize: 6.75
                        color: HoloniightPalette.textSecondary
                        elide: Text.ElideRight
                        visible: CalendarService.lastError !== ""
                    }
                }
            }

            // Offline state
            Item {
                Layout.fillWidth: true
                implicitHeight: 16
                visible: CalendarService.upcomingState === CalendarService.Offline

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "No connection — showing cached events"
                    font.pointSize: 6.75
                    color: HoloniightPalette.textSecondary
                }
            }

            // Empty state
            Item {
                Layout.fillWidth: true
                implicitHeight: 32
                visible: {
                    var mdl = CalendarService.upcomingEvents
                    var isEmpty = (mdl === null || mdl.rowCount() === 0)
                    return isEmpty &&
                           CalendarService.upcomingState !== CalendarService.Loading &&
                           CalendarService.upcomingState !== CalendarService.ConnectError
                }

                Text {
                    anchors.centerIn: parent
                    text: "No upcoming events"
                    font.pointSize: 8.25
                    color: HoloniightPalette.textSecondary
                }
            }

            // Event list
            ColumnLayout {
                id: listContainer
                Layout.fillWidth: true
                spacing: 8
                visible: CalendarService.upcomingEvents !== null && CalendarService.upcomingEvents.rowCount() > 0

                Repeater {
                    model: CalendarService.upcomingEvents

                    delegate: Item {
                        id: eventRow

                        required property date startTime
                        required property date endTime
                        required property bool isAllDay
                        required property string title
                        required property string location
                        required property int index

                        Layout.fillWidth: true
                        visible: index < 3
                        implicitHeight: visible ? cardColumn.implicitHeight + 24 : 0

                        property var eventState: root.getEventState(startTime, endTime, isAllDay, index)
                        property string _duration: root.formatDuration(startTime, endTime)
                        property string _locationIcon: root.getLocationIcon(location)

                        Rectangle {
                            id: cardRect
                            anchors.fill: parent
                            radius: 8
                            color: Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g, HoloniightPalette.surface.b, 0.45)
                            border.color: Qt.rgba(HoloniightPalette.borderPassive.r, HoloniightPalette.borderPassive.g, HoloniightPalette.borderPassive.b, 0.05)
                            border.width: 1
                            clip: true

                            Rectangle {
                                id: accentBar
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 4
                                color: eventRow.eventState.color
                            }

                            ColumnLayout {
                                id: cardColumn
                                anchors.left: parent.left
                                anchors.leftMargin: 16
                                anchors.right: parent.right
                                anchors.rightMargin: 16
                                anchors.top: parent.top
                                anchors.topMargin: 12
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    ColumnLayout {
                                        Layout.alignment: Qt.AlignTop
                                        Layout.preferredWidth: 75
                                        spacing: 2

                                        Text {
                                            text: eventRow.isAllDay ? "All day" : Qt.formatTime(eventRow.startTime, "hh:mm")
                                            font.pointSize: 13.5
                                            font.bold: true
                                            font.family: AppearanceService.uiFont
                                            color: HoloniightPalette.textPrimary
                                            Layout.fillWidth: true
                                        }

                                        Text {
                                            text: eventRow.eventState.text
                                            font.pointSize: 8.25
                                            font.bold: eventRow.eventState.isHighlighted
                                            font.family: AppearanceService.uiFont
                                            color: eventRow.eventState.color
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignTop
                                        spacing: 6

                                        Text {
                                            text: eventRow.title
                                            font.pointSize: 10.5
                                            font.bold: true
                                            font.family: AppearanceService.uiFont
                                            color: HoloniightPalette.textPrimary
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 12

                                            RowLayout {
                                                spacing: 4
                                                visible: eventRow._duration !== ""

                                                Item {
                                                    Layout.preferredWidth: 14
                                                    Layout.preferredHeight: 14

                                                    Image {
                                                        id: durIcon
                                                        anchors.fill: parent
                                                        source: "image://icon/preferences-system-time-symbolic"
                                                        fillMode: Image.PreserveAspectFit
                                                        visible: false
                                                    }

                                                    MultiEffect {
                                                        anchors.fill: parent
                                                        source: durIcon
                                                        colorization: 1.0
                                                        colorizationColor: HoloniightPalette.textSecondary
                                                    }
                                                }

                                                Text {
                                                    text: eventRow._duration
                                                    font.pointSize: 8.25
                                                    font.family: AppearanceService.uiFont
                                                    color: HoloniightPalette.textMuted
                                                }
                                            }

                                            RowLayout {
                                                spacing: 4
                                                visible: eventRow.location !== ""
                                                Layout.fillWidth: true

                                                Item {
                                                    Layout.preferredWidth: 14
                                                    Layout.preferredHeight: 14

                                                    Image {
                                                        id: locIcon
                                                        anchors.fill: parent
                                                        source: "image://icon/" + eventRow._locationIcon
                                                        fillMode: Image.PreserveAspectFit
                                                        visible: false
                                                    }

                                                    MultiEffect {
                                                        anchors.fill: parent
                                                        source: locIcon
                                                        colorization: 1.0
                                                        colorizationColor: HoloniightPalette.textSecondary
                                                    }
                                                }

                                                Text {
                                                    text: eventRow.location
                                                    font.pointSize: 8.25
                                                    font.family: AppearanceService.uiFont
                                                    color: HoloniightPalette.textMuted
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                id: progressTrack
                                visible: eventRow.eventState.name === "active" && !eventRow.isAllDay
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 4
                                color: Qt.rgba(HoloniightPalette.success.r, HoloniightPalette.success.g, HoloniightPalette.success.b, 0.20)

                                Rectangle {
                                    id: progressBar
                                    width: parent.width * eventRow.eventState.progress
                                    height: parent.height
                                    color: HoloniightPalette.success
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

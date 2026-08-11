pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Holonight.Core

import HolonightShell

ColumnLayout {
    id: root

    property int viewYear: (new Date()).getFullYear()
    property int viewMonth: (new Date()).getMonth()

    readonly property bool viewIsCurrentMonth: {
        var now = new Date()
        return root.viewYear === now.getFullYear() && root.viewMonth === now.getMonth()
    }
    readonly property var monthNames: [
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    ]
    readonly property var dayHeadersMon: ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
    readonly property var dayHeadersSun: ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"]
    property var dayHeaders: CalendarService.weekStartDay === "Sun" ? root.dayHeadersSun : root.dayHeadersMon
    property var dayModel: root.buildDayModel(root.viewYear, root.viewMonth, CalendarService.weekStartDay)

    Layout.fillWidth: true
    spacing: 4

    function monthLabel(year, month) {
        return root.monthNames[month] + " " + year
    }

    function buildDayModel(year, month, weekStart) {
        var startDayOffset = (weekStart === "Sun") ? 0 : 1
        var firstDate = new Date(year, month, 1)
        var firstDayOfWeek = firstDate.getDay()
        var leadingDays = (firstDayOfWeek - startDayOffset + 7) % 7
        var today = new Date()
        var todayYear = today.getFullYear()
        var todayMonth = today.getMonth()
        var todayDay = today.getDate()
        var result = []

        for (var i = 0; i < 35; i++) {
            var dayDate = new Date(year, month, 1 - leadingDays + i)
            var dayYear = dayDate.getFullYear()
            var dayMonth = dayDate.getMonth()
            var dayOfMonth = dayDate.getDate()
            var dayOfWeek = dayDate.getDay()
            result.push({
                day: dayOfMonth,
                month: dayMonth,
                year: dayYear,
                isCurrentMonth: dayYear === year && dayMonth === month,
                isToday: dayYear === todayYear && dayMonth === todayMonth && dayOfMonth === todayDay,
                isWeekend: dayOfWeek === 0 || dayOfWeek === 6
            })
        }
        return result
    }

    Text {
        text: "// CALENDAR"
        color: HoloniightPalette.borderActive
        font.family: AppearanceService.titleFont
        font.pointSize: AppearanceService.titleFontSize * 0.75
        font.letterSpacing: 0.8
    }

    Item {
        Layout.fillWidth: true
        implicitHeight: 32

        Text {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: root.monthLabel(root.viewYear, root.viewMonth)
            font.pointSize: 9.75
            font.bold: true
            color: HoloniightPalette.textPrimary
        }

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            Rectangle {
                id: prevButton

                width: 28
                height: 28
                radius: 6
                color: prevMouseArea.containsMouse ? HoloniightPalette.surface : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "‹"
                    font.pointSize: 10.5
                    color: HoloniightPalette.textPrimary
                }

                MouseArea {
                    id: prevMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.viewMonth === 0) {
                            root.viewMonth = 11
                            root.viewYear--
                        } else {
                            root.viewMonth--
                        }
                    }
                }
            }

            Rectangle {
                id: nextButton

                width: 28
                height: 28
                radius: 6
                color: nextMouseArea.containsMouse ? HoloniightPalette.surface : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "›"
                    font.pointSize: 10.5
                    color: HoloniightPalette.textPrimary
                }

                MouseArea {
                    id: nextMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.viewMonth === 11) {
                            root.viewMonth = 0
                            root.viewYear++
                        } else {
                            root.viewMonth++
                        }
                    }
                }
            }
        }
    }

    Item {
        id: dayHeaderRow

        Layout.fillWidth: true
        implicitHeight: 20

        Repeater {
            model: root.dayHeaders

            Text {
                required property var modelData
                required property int index

                x: index * (dayHeaderRow.width / 7) + (dayHeaderRow.width / 7 - implicitWidth) / 2
                width: dayHeaderRow.width / 7
                horizontalAlignment: Text.AlignHCenter
                text: modelData.toUpperCase()
                font.pointSize: 7.5
                color: HoloniightPalette.accentBlue
            }
        }
    }

    Item {
        id: dayGrid

        Layout.fillWidth: true
        implicitHeight: 5 * 32

        Repeater {
            model: root.dayModel

            delegate: Item {
                required property var modelData
                required property int index

                x: (index % 7) * (dayGrid.width / 7)
                y: Math.floor(index / 7) * 32
                width: dayGrid.width / 7
                height: 32

                Item {
                    id: daySlot

                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 26
                    height: 26

                    MultiEffect {
                        anchors.fill: glowFrame
                        source: glowFrame
                        visible: glowFrame.visible
                        shadowEnabled: true
                        shadowColor: HoloniightPalette.accentViolet
                        shadowBlur: 0.8
                        shadowHorizontalOffset: 0
                        shadowVerticalOffset: 0
                    }

                    Rectangle {
                        id: glowFrame

                        anchors.fill: parent
                        radius: 4
                        color: "transparent"
                        border.color: HoloniightPalette.accentViolet
                        border.width: 1
                        visible: modelData.isToday && root.viewIsCurrentMonth
                    }

                    Text {
                        anchors.fill: parent
                        anchors.rightMargin: 2
                        horizontalAlignment: Text.AlignRight
                        verticalAlignment: Text.AlignVCenter
                        text: modelData.day
                        font.pointSize: 9
                        font.bold: modelData.isToday && root.viewIsCurrentMonth
                        opacity: modelData.isCurrentMonth ? 1.0 : 0.45
                        color: !modelData.isCurrentMonth
                               ? HoloniightPalette.textSecondary
                               : (modelData.isToday && root.viewIsCurrentMonth)
                                 ? HoloniightPalette.accentCyan
                                 : modelData.isWeekend
                                   ? HoloniightPalette.accentViolet
                                   : HoloniightPalette.textPrimary
                    }
                }
            }
        }
    }

    Item { implicitHeight: 4 }
}

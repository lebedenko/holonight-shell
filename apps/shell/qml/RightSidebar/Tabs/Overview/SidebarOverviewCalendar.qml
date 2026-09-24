pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Holonight.Core
import Holonight.Controls

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
    readonly property real dayCellHeight: Math.max(32, dayFontMetrics.height + 8)

    Layout.fillWidth: true
    spacing: 4

    FontMetrics {
        id: dayFontMetrics
        font.family: AppearanceService.uiFont
        font.pointSize: HolonightTheme.captionSize
    }

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

    HnLabel {
        rawText: qsTr("// CALENDAR")
        role: HnTypographyRole.MicroHeader
        color: HoloniightPalette.borderActive
        font.letterSpacing: 0.8
    }

    Item {
        Layout.fillWidth: true
        implicitHeight: Math.max(32, monthText.implicitHeight + 8, navigationRow.implicitHeight)

        HnLabel {
            id: monthText
            anchors.left: parent.left
            anchors.right: navigationRow.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            rawText: root.monthLabel(root.viewYear, root.viewMonth)
            role: HnTypographyRole.Body
            font.bold: true
            color: HoloniightPalette.textPrimary
            elide: Text.ElideRight
        }

        Row {
            id: navigationRow
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            Rectangle {
                id: prevButton

                width: Math.max(28, previousLabel.implicitWidth + 12)
                height: Math.max(28, previousLabel.implicitHeight + 8)
                radius: 6
                color: prevMouseArea.containsMouse ? HoloniightPalette.surface : "transparent"

                HnLabel {
                    id: previousLabel
                    anchors.centerIn: parent
                    rawText: "‹"
                    role: HnTypographyRole.Body
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

                width: Math.max(28, nextLabel.implicitWidth + 12)
                height: Math.max(28, nextLabel.implicitHeight + 8)
                radius: 6
                color: nextMouseArea.containsMouse ? HoloniightPalette.surface : "transparent"

                HnLabel {
                    id: nextLabel
                    anchors.centerIn: parent
                    rawText: "›"
                    role: HnTypographyRole.Body
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
        implicitHeight: Math.max(20, dayFontMetrics.height + 4)

        Repeater {
            model: root.dayHeaders

            HnLabel {
                required property var modelData
                required property int index

                x: index * (dayHeaderRow.width / 7)
                width: dayHeaderRow.width / 7
                horizontalAlignment: Text.AlignHCenter
                rawText: modelData.toUpperCase()
                role: HnTypographyRole.Caption
                color: HoloniightPalette.accentBlue
            }
        }
    }

    Item {
        id: dayGrid

        Layout.fillWidth: true
        implicitHeight: 5 * root.dayCellHeight

        Repeater {
            model: root.dayModel

            delegate: Item {
                required property var modelData
                required property int index

                x: (index % 7) * (dayGrid.width / 7)
                y: Math.floor(index / 7) * root.dayCellHeight
                width: dayGrid.width / 7
                height: root.dayCellHeight

                Item {
                    id: daySlot

                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(Math.max(0, parent.width - 8), Math.max(26, dayText.implicitWidth + 4))
                    height: Math.max(26, dayText.implicitHeight + 4)

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

                    HnLabel {
                        id: dayText
                        anchors.fill: parent
                        anchors.rightMargin: 2
                        horizontalAlignment: Text.AlignRight
                        verticalAlignment: Text.AlignVCenter
                        rawText: String(modelData.day)
                        role: HnTypographyRole.Caption
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

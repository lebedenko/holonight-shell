import QtQuick
import QtQuick.Controls as Controls
import Holonight.Core
import HolonightShell

import "../Controls"

BarSection {
    id: root

    required property string barMonitorName
    readonly property int cornerCut: 12
    readonly property int contentLeftMargin: 24
    readonly property int contentRightMargin: 24
    readonly property int inheritedSectionPadding: 8
    property date currentDateTime: new Date()

    implicitWidth: Math.max(timeLabel.implicitWidth, dateLabel.implicitWidth) + root.contentLeftMargin + root.contentRightMargin

    BarFrame {

        anchors {
            fill: parent
            leftMargin: -root.inheritedSectionPadding
            rightMargin: -root.inheritedSectionPadding
        }
        rightCornerCut: root.cornerCut
        innerGlowColor: HoloniightPalette.accentCyan
        innerGlowOpacity: hoverHandler.hovered ? 0.22 : 0.0

        Behavior on innerGlowOpacity {
            NumberAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }
    }

    Column {
        anchors {
            left: parent.left
            leftMargin: root.contentLeftMargin - root.inheritedSectionPadding
            verticalCenter: parent.verticalCenter
        }
        width: Math.max(timeLabel.implicitWidth, dateLabel.implicitWidth)

        Controls.Label {
            id: timeLabel
            width: parent.width
            horizontalAlignment: Text.AlignRight
            color: HoloniightPalette.accentCyan
            text: Qt.formatDateTime(root.currentDateTime, "HH:mm")
            font.family: AppearanceService.displayFont
            font.pixelSize: AppearanceService.displayFontSize
            font.weight: Font.Light
        }

        Controls.Label {
            id: dateLabel
            width: parent.width
            horizontalAlignment: Text.AlignRight
            color: HoloniightPalette.accentViolet
            opacity: 0.6
            text: Qt.formatDateTime(root.currentDateTime, "ddd d MMM").toUpperCase()
            font.pixelSize: 10
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: root.currentDateTime = new Date()
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: false
        onClicked: SidebarManager.toggle(root.barMonitorName)
    }

    HoverHandler {
        id: hoverHandler
    }

    BarTooltipArea {
        barMonitorName: root.barMonitorName
        title: Qt.formatDateTime(root.currentDateTime, "dddd, d MMMM")
        description: "Local time " + Qt.formatDateTime(root.currentDateTime, "HH:mm:ss") + ".\nClick to open sidebar."
        iconName: "clock"
    }
}

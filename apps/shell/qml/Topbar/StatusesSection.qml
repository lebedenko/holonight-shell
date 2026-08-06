import QtQuick

import QtQuick.Layouts

import HolonightShell

import "../Controls"

BarSection {
    id: root

    required property string barMonitorName
    readonly property int slantCut: 12
    readonly property int contentLeftMargin: 24 + root.slantCut
    readonly property int contentRightMargin: 24 + root.slantCut
    readonly property int inheritedSectionPadding: 8

    implicitWidth: root.contentLeftMargin + statusRow.implicitWidth + root.contentRightMargin

    BarFrame {

        anchors {
            fill: parent
            leftMargin: -root.inheritedSectionPadding
            rightMargin: -root.inheritedSectionPadding
        }
        leftBottomOffset: root.slantCut
    }

    RowLayout {
        id: statusRow

        anchors {
            left: parent.left
            leftMargin: root.contentLeftMargin - root.inheritedSectionPadding
            right: parent.right
            rightMargin: root.contentRightMargin - root.inheritedSectionPadding
            verticalCenter: parent.verticalCenter
        }
        spacing: 0

        NetworkWidget {
            barMonitorName: root.barMonitorName
            Layout.alignment: Qt.AlignVCenter
        }

        AudioWidget {
            barMonitorName: root.barMonitorName
            Layout.alignment: Qt.AlignVCenter
        }

        BatteryWidget {
            barMonitorName: root.barMonitorName
            Layout.alignment: Qt.AlignVCenter
        }

        NotificationsWidget {
            barMonitorName: root.barMonitorName
            Layout.alignment: Qt.AlignVCenter
        }

        KeyboardLayoutWidget {
            barMonitorName: root.barMonitorName
            Layout.alignment: Qt.AlignVCenter
        }
    }

}

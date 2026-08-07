import QtQuick
import QtQuick.Layouts
import HolonightShell
import "../Tray" as Tray
import "../Utility" as Utility

Item {
    id: root

    required property string barMonitorName
    readonly property int primarySectionMargin: -12
    readonly property int statusSectionMargin: 0

    Utility.AppearanceReloadBridge {}

    // BarBackground {
    //     anchors.fill: parent
    // }

    RowLayout {
        anchors {
            fill: parent
            leftMargin: 0
            rightMargin: 0
        }
        spacing: 0

        LogoSection {
            barMonitorName: root.barMonitorName
            Layout.alignment: Qt.AlignVCenter
        }

        WorkspaceSection {
            barMonitorName: root.barMonitorName
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: root.primarySectionMargin
        }

        ActiveWindowSection {
            barMonitorName: root.barMonitorName
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: root.primarySectionMargin
        }

        MprisSection {
            barMonitorName: root.barMonitorName
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: root.primarySectionMargin
        }

        WeatherSection {
            barMonitorName: root.barMonitorName
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: root.primarySectionMargin
        }

        StatusesSection {
            barMonitorName: root.barMonitorName
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: root.primarySectionMargin
        }

        Tray.TraySection {
            barMonitorName: root.barMonitorName
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: root.statusSectionMargin
        }

        ClockSection {
            barMonitorName: root.barMonitorName
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: root.statusSectionMargin
        }
    }
}

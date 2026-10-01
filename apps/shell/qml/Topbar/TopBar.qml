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

        WindowTaskbar {
            id: taskbar
            objectName: "taskbarSection"
            barMonitorName: root.barMonitorName
            Layout.fillWidth: visible
            Layout.minimumWidth: minimumContentWidth
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: root.primarySectionMargin
        }

        ActiveWindowSection {
            id: activeWindowSection
            objectName: "activeWindowSection"
            barMonitorName: root.barMonitorName
            Layout.fillWidth: !taskbar.visible
            Layout.minimumWidth: 0
            Layout.maximumWidth: taskbar.visible ? 320 : Infinity
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: root.primarySectionMargin
        }

        Item {
            objectName: "activeWindowSpacer"
            visible: !taskbar.visible && !activeWindowSection.visible
            Layout.fillWidth: true
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

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Components
import Holonight.Controls

import HolonightShell

Item {
    id: root

    // "apps", "actions", or "" for no filter
    property string activeFilter: ""

    function resetFilter() {
        root.activeFilter = ""
    }

    function filterIndex(): int {
        if (root.activeFilter === "apps")
            return 1
        if (root.activeFilter === "actions")
            return 2
        return 0
    }

    function formatLastUsed(dt) {
        if (!dt || !dt.valid) return ""
        const now = new Date()
        const then = new Date(dt)
        const msPerDay = 86400000
        const diff = (now - then) / msPerDay
        if (diff < 1)  return "Today, " + Qt.formatTime(then, Qt.locale().timeFormat(Locale.ShortFormat))
        if (diff < 2)  return "Yesterday, " + Qt.formatTime(then, Qt.locale().timeFormat(Locale.ShortFormat))
        return Qt.formatDate(then, Qt.locale().dateFormat(Locale.ShortFormat))
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // FILTERS section
        Text {
            Layout.fillWidth: true
            Layout.bottomMargin: 8
            text: "FILTERS"
            color: HoloniightPalette.accentViolet
            font.family: AppearanceService.uiFont
            font.pixelSize: 11
            font.weight: Font.Medium
            font.letterSpacing: 1.2
        }

        HnSegmentedControl {
            id: searchFilterControl

            objectName: "launcherSearchFilterControl"
            model: [
                { value: "all", text: qsTr("All") },
                { value: "applications", text: qsTr("Applications") + "  " + LauncherService.appResultCount },
                { value: "actions", text: qsTr("Actions") + "  " + LauncherService.actionResultCount }
            ]
            currentIndex: root.filterIndex()
            Layout.fillWidth: true
            onActivated: (_, value) => {
                if (value === "applications")
                    root.activeFilter = "apps"
                else if (value === "actions")
                    root.activeFilter = "actions"
                else
                    root.activeFilter = ""
            }
        }

        Connections {
            target: root

            function onActiveFilterChanged(): void {
                searchFilterControl.currentIndex = root.filterIndex()
            }
        }

        Item { Layout.preferredHeight: 20; Layout.fillWidth: true }

        // SELECTED ITEM section
        Text {
            Layout.fillWidth: true
            Layout.bottomMargin: 10
            text: "SELECTED"
            color: HoloniightPalette.accentViolet
            font.family: AppearanceService.uiFont
            font.pixelSize: 11
            font.weight: Font.Medium
            font.letterSpacing: 1.2
            visible: LauncherService.selectedEntryName.length > 0
        }

        Row {
            Layout.fillWidth: true
            Layout.bottomMargin: 6
            spacing: 10
            visible: LauncherService.selectedEntryName.length > 0

            ExternalIcon {
                width: 32
                height: 32
                anchors.verticalCenter: parent.verticalCenter
                iconName: LauncherService.selectedEntryIcon
                iconSize: 32
                fallbackIconName: "application-x-executable"
                preferSemanticTint: false
            }

            Text {
                width: parent.width - 42
                anchors.verticalCenter: parent.verticalCenter
                text: LauncherService.selectedEntryName
                color: HoloniightPalette.textPrimary
                font.family: AppearanceService.uiFont
                font.pixelSize: 14
                font.weight: Font.Medium
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.bottomMargin: 12
            visible: LauncherService.selectedEntryName.length > 0 && lastUsedText.length > 0
            text: lastUsedText
            color: HoloniightPalette.textSecondary
            font.family: AppearanceService.uiFont
            font.pixelSize: 11

            readonly property string lastUsedText: {
                const df = LauncherService.selectedEntryDesktopFile
                if (!df) return ""
                return root.formatLastUsed(RecentAppsTracker.lastUsedFor(df))
            }
        }

        // Desktop actions for selected entry
        Repeater {
            model: LauncherService.selectedEntryActions

            delegate: Item {
                id: actionDelegate
                required property var modelData
                required property int index

                Layout.fillWidth: true
                height: 30

                Rectangle {
                    anchors.fill: parent
                    radius: 5
                    color: actionItemArea.containsMouse ? HoloniightPalette.workspaceActive : "transparent"
                    opacity: 0.7
                }

                Text {
                    anchors {
                        left: parent.left; leftMargin: 8
                        right: parent.right; rightMargin: 8
                        verticalCenter: parent.verticalCenter
                    }
                    text: actionDelegate.modelData.name || ""
                    color: actionItemArea.containsMouse ? HoloniightPalette.accentCyan : HoloniightPalette.textPrimary
                    font.family: AppearanceService.uiFont
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                MouseArea {
                    id: actionItemArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: LauncherService.launchAction(LauncherService.selectedIndex, actionDelegate.index)
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}

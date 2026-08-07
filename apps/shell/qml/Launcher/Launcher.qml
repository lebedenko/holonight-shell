pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Holonight.Core
import Holonight.Controls
import HolonightShell
import "../Controls"

import "../Utility" as Utility

Item {
    id: root

    Utility.AppearanceReloadBridge {}

    focus: true

    readonly property int panelWidth: Math.min(1100, root.width - 32)
    readonly property int panelHeight: Math.min(560, root.height - 96)
    readonly property bool isSearchMode: LauncherService.query.length > 0

    function resetAndFocus() {
        searchField.clearInput()
        LauncherService.setQuery("")
        LauncherService.setActiveCategory("")
        searchPanel.resetFilter()
        root.forceActiveFocus()
        searchField.forceInputFocus()
    }

    function forceReopen() {
        closeAnimation.stop()
        panel.scale = 0.95
        panel.opacity = 0.0
        openAnimation.start()
    }

    function startClose() {
        openAnimation.stop()
        closeAnimation.start()
    }

    Keys.onEscapePressed: root.startClose()

    Connections {
        target: LauncherService
        function onLaunched() {
            root.startClose()
        }
    }

    Component.onCompleted: {
        root.forceReopen()
        root.resetAndFocus()
    }

    ParallelAnimation {
        id: openAnimation
        NumberAnimation {
            target: panel
            property: "scale"
            to: 1.0
            duration: 150
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: panel
            property: "opacity"
            to: 1.0
            duration: 150
            easing.type: Easing.OutCubic
        }
    }

    SequentialAnimation {
        id: closeAnimation
        ParallelAnimation {
            NumberAnimation {
                target: panel
                property: "scale"
                to: 0.95
                duration: 150
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                target: panel
                property: "opacity"
                to: 0.0
                duration: 150
                easing.type: Easing.InCubic
            }
        }
        ScriptAction {
            script: LauncherSurface.notifyHideReady()
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.startClose()
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: "red"
        border.width: 1
        visible: AppearanceService.debugOverlays
        z: 99999
    }

    Item {
        id: panel
        width: root.panelWidth
        height: root.panelHeight
        anchors.centerIn: parent
        opacity: 0.0

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        MultiEffect {
            source: panelFrame
            anchors.fill: panelFrame
            shadowEnabled: true
            shadowColor: HoloniightPalette.accentCyan
            shadowBlur: 0.5
            shadowOpacity: 0.12
            shadowScale: 1.01
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 0
            autoPaddingEnabled: true
        }

        HudFrame {
            id: panelFrame
            anchors.fill: parent
            variant: HudFrame.Popup
            frameStroke: HoloniightPalette.borderActive
        }

        ColumnLayout {
            anchors {
                fill: parent
                margins: 24
            }
            spacing: 16

            LauncherSearchField {
                id: searchField
                Layout.fillWidth: true
                Layout.preferredHeight: 62

                onMoveSelection: function(delta) {
                    LauncherService.moveSelection(delta)
                }
                onLaunchRequested: LauncherService.launchSelected()
                onCloseRequested: root.startClose()
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // Left column — app list / results
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.rightMargin: 16

                    // Browse mode: alphabetical all-apps list
                    ListView {
                        id: browseList
                        anchors.fill: parent
                        visible: !root.isSearchMode
                        clip: true
                        spacing: 0
                        model: LauncherService.results
                        currentIndex: LauncherService.selectedIndex

                        delegate: LauncherResultRow {
                            required property int index
                            required property var model
                            required property string name
                            required property string iconName
                            required property string desktopFile

                            width: ListView.view.width
                            isBestMatch: false
                            highlighted: index === LauncherService.selectedIndex
                            appName: name
                            appSubtitle: model.subtitle
                            appIconName: iconName
                            appDesktopFile: desktopFile
                            onHoveredChanged: {
                                if (hovered)
                                    LauncherService.setSelectedIndex(index)
                            }
                            onActivated: LauncherService.launch(index)
                        }

                        HnEmptyState {
                            objectName: "browseResultsEmptyState"
                            anchors.centerIn: parent
                            visible: LauncherService.resultCount === 0 && !root.isSearchMode
                            titleText: qsTr("No applications in this category")
                        }
                    }

                    // Search mode: results with section headers
                    ListView {
                        id: searchList
                        anchors.fill: parent
                        visible: root.isSearchMode
                        clip: true
                        spacing: 0
                        model: LauncherService.results
                        currentIndex: LauncherService.selectedIndex

                        header: Item {
                            width: searchList.width
                            height: LauncherService.resultCount > 0 ? sectionHeaderBestMatch.height + 8 : 0
                            visible: LauncherService.resultCount > 0

                            Text {
                                id: sectionHeaderBestMatch
                                anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 4 }
                                text: "BEST MATCH"
                                color: HoloniightPalette.accentViolet
                                font.family: AppearanceService.uiFont
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                font.letterSpacing: 1.2
                            }
                        }

                        section.property: "isActionSection"
                        section.delegate: Item {
                            id: sectionDelegate
                            required property string section

                            width: searchList.width
                            height: sectionLabel.implicitHeight + 12
                            visible: sectionDelegate.section.length > 0

                            Text {
                                id: sectionLabel
                                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                                text: sectionDelegate.section
                                color: HoloniightPalette.accentViolet
                                font.family: AppearanceService.uiFont
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                font.letterSpacing: 1.2
                                visible: sectionDelegate.section.length > 0
                            }
                        }

                        delegate: Item {
                            id: searchDelegate
                            required property int index
                            required property string name
                            required property string subtitle
                            required property string iconName
                            required property string desktopFile
                            required property bool isAction
                            required property string actionParent
                            required property string actionExec
                            required property int actionIndex

                            width: ListView.view.width

                            readonly property bool filterHidden: {
                                const flt = searchPanel.activeFilter
                                if (flt === "apps") return searchDelegate.isAction
                                if (flt === "actions") return !searchDelegate.isAction
                                return false
                            }

                            visible: !searchDelegate.filterHidden
                            height: searchDelegate.filterHidden ? 0 : (searchDelegate.isAction ? actionRow.height : appRow.height)

                            LauncherResultRow {
                                id: appRow
                                width: parent.width
                                visible: !searchDelegate.isAction
                                isBestMatch: searchDelegate.index === 0
                                highlighted: searchDelegate.index === LauncherService.selectedIndex
                                appName: searchDelegate.name
                                appSubtitle: searchDelegate.subtitle
                                appIconName: searchDelegate.iconName
                                appDesktopFile: searchDelegate.desktopFile
                                onHoveredChanged: {
                                    if (hovered)
                                        LauncherService.setSelectedIndex(searchDelegate.index)
                                }
                                onActivated: LauncherService.launch(searchDelegate.index)
                            }

                            LauncherActionRow {
                                id: actionRow
                                width: parent.width
                                visible: searchDelegate.isAction
                                highlighted: searchDelegate.index === LauncherService.selectedIndex
                                actionName: searchDelegate.name
                                parentAppName: searchDelegate.actionParent
                                actionExec: searchDelegate.actionExec
                                onActivated: LauncherService.launchAction(searchDelegate.index, searchDelegate.actionIndex)
                            }
                        }

                        HnEmptyState {
                            objectName: "searchResultsEmptyState"
                            anchors.centerIn: parent
                            visible: LauncherService.resultCount === 0 && root.isSearchMode
                            titleText: qsTr("No matches")
                        }
                    }
                }

                HnSeparator {
                    orientation: Qt.Vertical
                    implicitHeight: parent.height
                    fadeMode: HnSeparator.FadeBoth
                    opacity: 0.5
                }

                // Right column — mode-switched panel (fixed 256px)
                Item {
                    Layout.preferredWidth: 256
                    Layout.fillHeight: true
                    Layout.leftMargin: 16

                    LauncherRightPanelBrowse {
                        anchors.fill: parent
                        visible: !root.isSearchMode
                    }

                    LauncherRightPanelSearch {
                        id: searchPanel
                        anchors.fill: parent
                        visible: root.isSearchMode
                    }
                }
            }

            // Footer: keyboard shortcut hints
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 30

                HnSeparator {
                    orientation: Qt.Horizontal
                    width: parent.width
                    fadeMode: HnSeparator.FadeBoth
                    opacity: 0.5
                }

                Row {
                    anchors.centerIn: parent
                    spacing: 24

                    // ↵ Launch
                    Row {
                        spacing: 6
                        anchors.verticalCenter: parent.verticalCenter

                        Item {
                            width: launchKey.width + 10
                            height: launchKey.height + 4
                            anchors.verticalCenter: parent.verticalCenter

                            Rectangle {
                                anchors.fill: parent
                                radius: 3
                                color: HoloniightPalette.borderPassive
                                opacity: 0.4
                            }

                            Text {
                                id: launchKey
                                anchors.centerIn: parent
                                text: "↵"
                                color: HoloniightPalette.textSecondary
                                font.family: AppearanceService.monospaceFont
                                font.pixelSize: 10
                            }
                        }

                        Text {
                            text: "Launch"
                            color: HoloniightPalette.textSecondary
                            font.family: AppearanceService.uiFont
                            font.pixelSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // Ctrl+↵ Launch in Terminal
                    Row {
                        spacing: 6
                        anchors.verticalCenter: parent.verticalCenter

                        Item {
                            width: termKey.width + 10
                            height: termKey.height + 4
                            anchors.verticalCenter: parent.verticalCenter

                            Rectangle {
                                anchors.fill: parent
                                radius: 3
                                color: HoloniightPalette.borderPassive
                                opacity: 0.4
                            }

                            Text {
                                id: termKey
                                anchors.centerIn: parent
                                text: "Ctrl+↵"
                                color: HoloniightPalette.textSecondary
                                font.family: AppearanceService.monospaceFont
                                font.pixelSize: 10
                            }
                        }

                        Text {
                            text: "Launch in Terminal"
                            color: HoloniightPalette.textSecondary
                            font.family: AppearanceService.uiFont
                            font.pixelSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // ↑↓ Navigate
                    Row {
                        spacing: 6
                        anchors.verticalCenter: parent.verticalCenter

                        Item {
                            width: navKey.width + 10
                            height: navKey.height + 4
                            anchors.verticalCenter: parent.verticalCenter

                            Rectangle {
                                anchors.fill: parent
                                radius: 3
                                color: HoloniightPalette.borderPassive
                                opacity: 0.4
                            }

                            Text {
                                id: navKey
                                anchors.centerIn: parent
                                text: "↑↓"
                                color: HoloniightPalette.textSecondary
                                font.family: AppearanceService.monospaceFont
                                font.pixelSize: 10
                            }
                        }

                        Text {
                            text: "Navigate"
                            color: HoloniightPalette.textSecondary
                            font.family: AppearanceService.uiFont
                            font.pixelSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // Esc Close
                    Row {
                        spacing: 6
                        anchors.verticalCenter: parent.verticalCenter

                        Item {
                            width: escKey.width + 10
                            height: escKey.height + 4
                            anchors.verticalCenter: parent.verticalCenter

                            Rectangle {
                                anchors.fill: parent
                                radius: 3
                                color: HoloniightPalette.borderPassive
                                opacity: 0.4
                            }

                            Text {
                                id: escKey
                                anchors.centerIn: parent
                                text: "Esc"
                                color: HoloniightPalette.textSecondary
                                font.family: AppearanceService.uiFont
                                font.pixelSize: 10
                            }
                        }

                        Text {
                            text: "Close"
                            color: HoloniightPalette.textSecondary
                            font.family: AppearanceService.uiFont
                            font.pixelSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }
    }
}

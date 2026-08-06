pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Holonight
import Holonight.Components
import Holonight.Core
import Holonight.Controls
import HolonightShell

Item {
    id: root

    readonly property string activeCategory: LauncherService.activeCategory
    property var recentEntries: RecentAppsTracker.recentEntries(5)
    readonly property bool recentAppsEmpty: recentEntries.length === 0

    ScrollView {
        id: scrollView

        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scrollView.width
            spacing: 0

            Text {
                text: qsTr("RECENT")
                color: HoloniightPalette.accentViolet
                font.family: AppearanceService.uiFont
                font.pixelSize: 11
                font.weight: Font.Medium
                font.letterSpacing: 1.2
                Layout.fillWidth: true
                Layout.bottomMargin: 8
            }

            Repeater {
                id: recentRepeater

                model: root.recentEntries

                delegate: HnListDelegate {
                    id: recentDelegate

                    required property var modelData
                    required property int index
                    readonly property var entryInfo:
                        LauncherService.entryInfoForDesktopFile(recentDelegate.modelData.desktopFile)

                    objectName: "recentApplicationDelegate-" + index
                    title: entryInfo && entryInfo.name ? entryInfo.name : ""
                    sizeRole: HnControlSize.Large
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    onClicked: {
                        if (entryInfo && entryInfo.desktopFile)
                            LauncherService.launchDesktopFile(entryInfo.desktopFile)
                    }

                    leadingContent: Component {
                        ExternalIcon {
                            iconName: recentDelegate.entryInfo && recentDelegate.entryInfo.iconName
                                ? recentDelegate.entryInfo.iconName
                                : ""
                            iconSize: 20
                            fallbackIconName: "application-x-executable"
                            preferSemanticTint: false
                        }
                    }
                }
            }

            HnEmptyState {
                objectName: "recentApplicationsEmptyState"
                visible: root.recentAppsEmpty
                titleText: qsTr("No recent apps")
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? implicitHeight : 0
            }

            Item {
                Layout.preferredHeight: 16
                Layout.fillWidth: true
            }

            HnSeparator {
                orientation: Qt.Horizontal
                implicitWidth: parent.width
                fadeMode: HnSeparator.FadeEnd
                opacity: 0.5
            }

            Item {
                Layout.preferredHeight: 16
                Layout.fillWidth: true
            }

            Text {
                text: qsTr("CATEGORIES")
                color: HoloniightPalette.accentViolet
                font.family: AppearanceService.uiFont
                font.pixelSize: 11
                font.weight: Font.Medium
                font.letterSpacing: 1.2
                Layout.fillWidth: true
                Layout.bottomMargin: 8
            }

            Repeater {
                model: LauncherService.availableCategories()

                delegate: HnNavigationDelegate {
                    id: categoryDelegate

                    required property string modelData
                    required property int index

                    objectName: "launcherCategoryDelegate-" + index
                    title: modelData
                    badgeText: String(LauncherService.countForCategory(modelData))
                    checked: root.activeCategory === modelData
                    sizeRole: HnControlSize.Normal
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    onClicked: LauncherService.setActiveCategory(modelData)

                    leadingContent: Component {
                        LauncherCategoryIcon {
                            category: categoryDelegate.modelData
                            iconColor: categoryDelegate.checked
                                ? HoloniightPalette.accentCyan
                                : HoloniightPalette.textPrimary
                        }
                    }
                }
            }

            Item {
                Layout.preferredHeight: 8
                Layout.fillWidth: true
            }
        }
    }

    Connections {
        target: RecentAppsTracker

        function onRecentChanged(): void {
            root.recentEntries = RecentAppsTracker.recentEntries(5)
        }
    }
}

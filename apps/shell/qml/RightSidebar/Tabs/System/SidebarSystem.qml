import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls

import HolonightShell

Item {
    id: root

    property int preferredWidth: 340
    property int preferredHeight: contentColumn.implicitHeight + 32
    readonly property var failingDiagnostics: visibleDiagnostics(SessionIntegrationService.diagnostics)

    function visibleDiagnostics(diagnostics) {
        const rows = []
        for (const diagnostic of diagnostics) {
            if (diagnostic["status"] === "warning" || diagnostic["status"] === "error") {
                rows.push(diagnostic)
            }
        }
        return rows
    }

    function statusLabel(status) {
        if (status === "error") {
            return qsTr("Needs attention")
        }
        if (status === "warning") {
            return qsTr("Warnings")
        }
        return qsTr("Healthy")
    }

    function indicatorStatus(status) {
        if (status === "error") {
            return HnStatusIndicator.Error
        }
        if (status === "warning") {
            return HnStatusIndicator.Warning
        }
        return HnStatusIndicator.Success
    }

    function cacheGuidance(diagnostic) {
        if (diagnostic["id"] === "xdg-menu-prefix" || diagnostic["id"] === "kde-sycoca") {
            return qsTr("Rebuild application caches after correcting menu prefix or cache metadata issues.")
        }
        return diagnostic["detail"] ?? ""
    }

    Component.onCompleted: SessionIntegrationService.refresh()

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 8

        Text {
            text: "Default Applications"
            color: HoloniightPalette.textPrimary
            font.pixelSize: 14
            font.weight: Font.Medium
            Layout.fillWidth: true
            Layout.bottomMargin: 4
        }

        DefaultAppRow {
            Layout.fillWidth: true
            label: "Browser"
            mimeTypesForFilter: ["text/html", "x-scheme-handler/http", "x-scheme-handler/https"]
            categoriesForFilter: ["WebBrowser"]
            currentDefault: MimeService.defaultBrowser
            onDefaultChanged: (desktopFile, mimeTypes) => MimeService.setDefaultBrowser(desktopFile, mimeTypes)
        }

        DefaultAppRow {
            Layout.fillWidth: true
            label: "Terminal"
            mimeTypesForFilter: ["application/x-terminal-emulator"]
            categoryFallback: "TerminalEmulator"
            currentDefault: MimeService.defaultTerminal
            onDefaultChanged: (desktopFile, mimeTypes) => MimeService.setDefaultTerminal(desktopFile, mimeTypes)
        }

        DefaultAppRow {
            Layout.fillWidth: true
            label: "File Manager"
            mimeTypesForFilter: ["inode/directory"]
            categoriesForFilter: ["FileManager"]
            currentDefault: MimeService.defaultFileManager
            onDefaultChanged: (desktopFile, mimeTypes) => MimeService.setDefaultFileManager(desktopFile, mimeTypes)
        }

        DefaultAppRow {
            Layout.fillWidth: true
            label: "Image Viewer"
            mimeTypesForFilter: ["image/jpeg", "image/png", "image/gif", "image/webp"]
            categoriesForFilter: ["Graphics", "Photography", "Viewer"]
            currentDefault: MimeService.defaultImageViewer
            onDefaultChanged: (desktopFile, mimeTypes) => MimeService.setDefaultImageViewer(desktopFile, mimeTypes)
        }

        DefaultAppRow {
            Layout.fillWidth: true
            label: "Text Editor"
            mimeTypesForFilter: ["text/plain"]
            categoriesForFilter: ["TextEditor"]
            currentDefault: MimeService.defaultTextEditor
            onDefaultChanged: (desktopFile, mimeTypes) => MimeService.setDefaultTextEditor(desktopFile, mimeTypes)
        }

        DefaultAppRow {
            Layout.fillWidth: true
            label: "Video Player"
            mimeTypesForFilter: ["video/mp4", "video/x-matroska", "video/webm"]
            categoriesForFilter: ["AudioVideo", "Player"]
            currentDefault: MimeService.defaultVideoPlayer
            onDefaultChanged: (desktopFile, mimeTypes) => MimeService.setDefaultVideoPlayer(desktopFile, mimeTypes)
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: HoloniightPalette.surface
            Layout.topMargin: 4
            Layout.bottomMargin: 4
        }

        ColumnLayout {
            id: sessionIntegrationSection
            Layout.fillWidth: true
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: qsTr("Session Integration")
                    color: HoloniightPalette.textPrimary
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    Layout.fillWidth: true
                }

                HnStatusIndicator {
                    objectName: "sessionOverallStatus"
                    text: root.statusLabel(SessionIntegrationService.overallStatus)
                    status: root.indicatorStatus(SessionIntegrationService.overallStatus)
                }
            }

            HnLoadingState {
                objectName: "sessionOperationProgress"
                visible: SessionIntegrationService.refreshInProgress
                         || SessionIntegrationService.rebuildInProgress
                running: visible
                titleText: SessionIntegrationService.rebuildInProgress ? qsTr("Rebuilding application caches")
                                                                       : qsTr("Refreshing diagnostics")
                Layout.fillWidth: true
            }

            Text {
                visible: root.failingDiagnostics.length === 0
                text: qsTr("No failing desktop-session checks.")
                color: HoloniightPalette.textMuted
                font.pixelSize: 12
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Repeater {
                model: root.failingDiagnostics

                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        HnStatusIndicator {
                            status: root.indicatorStatus(modelData["status"])
                            text: modelData["title"] ?? qsTr("Diagnostic")
                            Layout.fillWidth: true
                        }
                    }

                    Text {
                        text: root.cacheGuidance(modelData)
                        color: HoloniightPalette.textMuted
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Text {
                        visible: modelData["command"] !== undefined && modelData["command"] !== ""
                        text: modelData["command"] ?? ""
                        color: HoloniightPalette.textSecondary
                        font.pixelSize: 10
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Controls.Button {
                    id: refreshBtn
                    text: SessionIntegrationService.refreshInProgress ? qsTr("Refreshing") : qsTr("Refresh")
                    enabled: !SessionIntegrationService.refreshInProgress
                             && !SessionIntegrationService.rebuildInProgress
                    font.pixelSize: 12
                    Layout.preferredWidth: 92
                    Layout.preferredHeight: 28
                    onClicked: SessionIntegrationService.refresh()

                    contentItem: Text {
                        text: refreshBtn.text
                        color: refreshBtn.enabled ? HoloniightPalette.textPrimary : HoloniightPalette.textMuted
                        font: refreshBtn.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    background: Rectangle {
                        color: refreshBtn.enabled
                            ? HoloniightPalette.surface
                            : Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                                      HoloniightPalette.surface.b, 0.45)
                        radius: 4
                    }
                }

                Controls.Button {
                    id: rebuildBtn
                    text: SessionIntegrationService.rebuildInProgress ? qsTr("Rebuilding") : qsTr("Rebuild")
                    enabled: !SessionIntegrationService.rebuildInProgress
                    font.pixelSize: 12
                    Layout.preferredWidth: 92
                    Layout.preferredHeight: 28
                    onClicked: SessionIntegrationService.rebuildApplicationCaches()

                    contentItem: Text {
                        text: rebuildBtn.text
                        color: rebuildBtn.enabled ? HoloniightPalette.textPrimary : HoloniightPalette.textMuted
                        font: rebuildBtn.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    background: Rectangle {
                        color: rebuildBtn.enabled ? HoloniightPalette.primary : HoloniightPalette.surface
                        radius: 4
                    }
                }

                Item {
                    Layout.fillWidth: true
                }
            }
        }
    }
}

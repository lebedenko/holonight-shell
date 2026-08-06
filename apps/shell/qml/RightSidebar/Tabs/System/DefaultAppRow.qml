import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls

import HolonightShell

// A single role row: label on the left, ComboBox on the right showing apps that declare
// at least one of the role's MIME types. Emits defaultChanged(desktopFile, mimeTypes) on selection.
HnSettingsRow {
    id: root

    objectName: "defaultAppSettingsRow"
    readonly property var appCombo: root.controlItem
    required property string label
    required property var mimeTypesForFilter
    // Optional XDG categories that make the MIME match role-specific. Without this,
    // broad handlers like browsers and editors appear in unrelated default-app rows.
    property var categoriesForFilter: []
    required property string currentDefault
    // Optional XDG category to fall back to when no app declares the role's MIME type.
    // Useful for roles like Terminal where the MIME convention (application/x-terminal-emulator)
    // is Debian-specific and most Arch terminals only declare Categories=TerminalEmulator.
    property string categoryFallback: ""

    signal defaultChanged(string desktopFile, var mimeTypes)

    titleText: root.label

    property var candidates: []

    function refreshCandidates() {
        let found = root.categoriesForFilter.length > 0
            ? LauncherService.defaultAppEntriesForMimeTypesAndCategories(root.mimeTypesForFilter, root.categoriesForFilter)
            : LauncherService.defaultAppEntriesForMimeTypes(root.mimeTypesForFilter)
        if (found.length === 0 && root.categoryFallback !== "") {
            found = LauncherService.defaultAppEntriesForCategory(root.categoryFallback)
        }
        const adapted = []
        for (const entry of found) {
            adapted.push({
                "name": entry["name"],
                "desktopFile": entry["desktopFile"],
                "mimeTypes": entry["mimeTypes"],
                "iconSource": entry["icon"] ? "image://icon/" + entry["icon"] : ""
            })
        }
        root.candidates = adapted
    }

    Component.onCompleted: {
        root.refreshCandidates()
        syncComboIndex()
    }

    onCurrentDefaultChanged: syncComboIndex()

    property Connections launcherConnections: Connections {
        target: LauncherService
        function onResultCountChanged() {
            root.refreshCandidates()
            root.syncComboIndex()
        }
    }

    function syncComboIndex() {
        if (!root.appCombo) {
            return
        }
        for (let i = 0; i < root.candidates.length; ++i) {
            if (root.candidates[i]["desktopFile"] === root.currentDefault) {
                root.appCombo.currentIndex = i
                return
            }
        }
        root.appCombo.currentIndex = -1
    }

    control: Component {
        HnIconComboBox {
            objectName: "defaultAppCombo"
            model: root.candidates
            textRole: "name"
            iconRole: "iconSource"
            implicitWidth: 160
            enabled: root.candidates.length > 0

            onActivated: {
                const entry = root.candidates[currentIndex]
                if (entry !== undefined) {
                    root.defaultChanged(entry["desktopFile"], entry["mimeTypes"] !== undefined ? entry["mimeTypes"] : [])
                }
            }

            displayText: currentIndex >= 0 ? textAt(currentIndex)
                                           : (root.candidates.length === 0 ? qsTr("Not installed")
                                                                         : (root.currentDefault !== ""
                                                                            ? qsTr("Current app unavailable") : ""))
        }
    }

    onAppComboChanged: root.syncComboIndex()
}

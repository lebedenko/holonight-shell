import QtQuick
import Holonight.Core
import Holonight.Controls
import HolonightShell

HnSearchField {
    id: root

    signal moveSelection(int delta)
    signal launchRequested()
    signal closeRequested()

    objectName: "launcherSearchField"
    sizeRole: HnControlSize.Hero
    text: LauncherService.query
    focus: true

    leadingContent: Component {
        Text {
            text: ">"
            color: HoloniightPalette.textPrimary
            font.family: AppearanceService.monospaceFont
            font.pointSize: 16.5
            Accessible.ignored: true
        }
    }

    function forceInputFocus(): void {
        root.forceActiveFocus()
    }

    function clearInput(): void {
        root.clear()
    }

    onTextChanged: {
        if (LauncherService.query !== root.text)
            LauncherService.setQuery(root.text)
    }

    Connections {
        target: LauncherService

        function onQueryChanged(): void {
            if (root.text !== LauncherService.query)
                root.text = LauncherService.query
        }
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Down || event.key === Qt.Key_Tab) {
            root.moveSelection(1)
            event.accepted = true
        } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Backtab) {
            root.moveSelection(-1)
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.launchRequested()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            if (root.length > 0)
                root.clear()
            else
                root.closeRequested()
            event.accepted = true
        }
    }
}

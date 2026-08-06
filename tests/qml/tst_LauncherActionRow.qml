import QtQuick
import QtTest
import Holonight.Controls
import HolonightShell

TestCase {
    id: root

    name: "LauncherActionRowQmlTests"

    Component {
        id: rowComponent

        LauncherActionRow {
            actionName: "New Window"
            parentAppName: "Editor"
            actionExec: "editor --new-window"
        }
    }

    function test_maps_action_contract_to_shared_delegate() {
        const row = createTemporaryObject(rowComponent, null)
        verify(row)
        compare(row.title, "New Window")
        compare(row.subtitle, "Editor")
        compare(row.actionExec, "editor --new-window")
        compare(row.sizeRole, HnControlSize.Large)
        compare(row.dividerVisible, true)
        compare(row.implicitHeight, 60)
        compare(row.selected, false)
        verify(row.contentItem.y + row.contentItem.height <= row.height - row.bottomPadding)
    }

    function test_highlighted_maps_to_shared_selection_state() {
        const row = createTemporaryObject(rowComponent, null, { "highlighted": true })
        verify(row)
        compare(row.selected, true)

        const overlay = findChild(row, "hnSelectableDelegateSelectedOverlay")
        verify(overlay)
        compare(overlay.visible, true)

        row.highlighted = false
        compare(row.selected, false)
        compare(overlay.visible, false)
    }

    function test_click_preserves_activated_signal() {
        const row = createTemporaryObject(rowComponent, null)
        verify(row)

        const activatedSpy = signalSpy.createObject(row, { "target": row, "signalName": "activated" })
        verify(activatedSpy)
        row.clicked()
        compare(activatedSpy.count, 1)
    }

    Component {
        id: signalSpy

        SignalSpy {}
    }
}

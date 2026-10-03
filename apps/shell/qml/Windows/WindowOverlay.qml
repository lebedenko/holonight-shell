import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import Holonight.Core
import HolonightShell

Rectangle {
    id: root
    color: WindowSurface.mode === 3 ? "transparent" : Qt.rgba(0, 0, 0, 0.45)
    focus: true
    property string selectedId: ""
    readonly property var target: WindowPresentation.revision >= 0 ? WindowPresentation.window(WindowSurface.target) : ({})
    readonly property var cards: WindowSurface.mode === 2
        ? WindowPresentation.overview.filter(w => WindowSurface.choices.indexOf(w.windowId) >= 0)
        : WindowPresentation.overview
    function activate(id) {
        const window = WindowPresentation.window(id)
        WindowSurface.hide()
        if (window.minimized) WindowPresentation.command(id, 2)
        WindowPresentation.command(id, 0)
    }
    function move(delta) {
        if (!cards.length) return
        let index = cards.findIndex(w => w.windowId === selectedId)
        index = Math.max(0, Math.min(cards.length - 1, index + delta))
        selectedId = cards[index].windowId
        list.positionViewAtIndex(index, ListView.Contain)
    }
    onCardsChanged: {
        if (!cards.some(w => w.windowId === selectedId)) selectedId = cards.length ? cards[0].windowId : ""
    }
    Connections {
        target: WindowPresentation
        function onChanged() {
            if (WindowSurface.visible && WindowSurface.mode === 1 && !WindowPresentation.window(WindowSurface.target).windowId)
                WindowSurface.hide()
        }
    }
    Keys.onPressed: event => {
        if (WindowSurface.mode === 3) return
        if (event.key === Qt.Key_Escape) {
            WindowSurface.hide()
            event.accepted = true
            return
        }
        if (WindowSurface.mode !== 0 && WindowSurface.mode !== 2) return
        if (event.key === Qt.Key_Down || event.key === Qt.Key_J || event.key === Qt.Key_Right || event.key === Qt.Key_L) move(1)
        else if (event.key === Qt.Key_Up || event.key === Qt.Key_K || event.key === Qt.Key_Left || event.key === Qt.Key_H) move(-1)
        else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && selectedId) activate(selectedId)
        else return
        event.accepted = true
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: WindowSurface.mode === 3 ? Qt.AllButtons : Qt.LeftButton
        onClicked: WindowSurface.hide()
    }
    DesktopMenu {
        id: desktopMenu
        anchors.fill: parent
        visible: WindowSurface.mode === 3
    }
    Rectangle {
        visible: WindowSurface.mode !== 3
        width: Math.min(720, root.width - 40)
        height: Math.min(620, root.height - 80)
        anchors.centerIn: parent
        color: HoloniightPalette.surface
        border.color: HoloniightPalette.borderActive
        radius: 12
        MouseArea { anchors.fill: parent }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12
            Controls.Label {
                text: WindowSurface.mode === 1 ? (root.target.title || "Window") : "Windows"
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Controls.TextField {
                id: search
                objectName: "windowSearch"
                visible: WindowSurface.mode === 0
                Layout.fillWidth: true
                placeholderText: "Search windows · ↓ selects · Escape closes"
                onTextChanged: WindowPresentation.setSearch(text)
                Keys.onDownPressed: { root.move(1); root.forceActiveFocus() }
                Keys.onEscapePressed: WindowSurface.hide()
                onAccepted: { if (root.selectedId) root.activate(root.selectedId) }
            }
            ListView {
                id: list
                visible: WindowSurface.mode === 0 || WindowSurface.mode === 2
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 8
                model: root.cards
                Controls.ScrollBar.vertical: Controls.ScrollBar {}
                delegate: Rectangle {
                    id: card
                    required property var modelData
                    width: list.width
                    height: 64
                    color: root.selectedId === modelData.windowId ? HoloniightPalette.borderActive : HoloniightPalette.surface
                    radius: 6
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: mouse => {
                            root.selectedId = card.modelData.windowId
                            if (mouse.button === Qt.RightButton) WindowSurface.menu(card.modelData.windowId, WindowSurface.screenName)
                            else root.activate(card.modelData.windowId)
                        }
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        AppWindowIcon {
                            appId: card.modelData.appId
                            iconName: LauncherService.iconForAppId(card.modelData.appId)
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                        }
                        Controls.Label {
                            text: (card.modelData.minimized ? "○ " : "") + (card.modelData.title || card.modelData.appId || "Window")
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Controls.Button {
                            text: card.modelData.minimized ? "Restore" : "Minimize"
                            visible: card.modelData.operations.indexOf(card.modelData.minimized ? 2 : 1) >= 0
                            onClicked: WindowPresentation.command(card.modelData.windowId, card.modelData.minimized ? 2 : 1)
                        }
                        Controls.Button {
                            text: "Close"
                            visible: card.modelData.operations.indexOf(7) >= 0
                            onClicked: WindowPresentation.command(card.modelData.windowId, 7)
                        }
                    }
                }
                Controls.Label { anchors.centerIn: parent; visible: root.cards.length === 0; text: "No windows found" }
            }
            Repeater {
                model: WindowSurface.mode === 1 ? [
                    { label: root.target.minimized ? "Restore" : "Minimize", operation: root.target.minimized ? 2 : 1 },
                    { label: root.target.maximized ? "Restore size" : "Maximize", operation: root.target.maximized ? 4 : 3 },
                    { label: root.target.fullscreen ? "Leave fullscreen" : "Fullscreen", operation: root.target.fullscreen ? 6 : 5 },
                    { label: "Close", operation: 7 }
                ] : []
                Controls.Button {
                    required property var modelData
                    text: modelData.label
                    Layout.fillWidth: true
                    visible: !!root.target.operations && root.target.operations.indexOf(modelData.operation) >= 0
                    onClicked: {
                        WindowPresentation.command(WindowSurface.target, modelData.operation)
                        WindowSurface.hide()
                    }
                }
            }
            Controls.Button { text: "Dismiss"; onClicked: WindowSurface.hide() }
        }
    }
    Component.onCompleted: {
        selectedId = cards.length ? cards[0].windowId : ""
        if (WindowSurface.mode === 0) search.forceActiveFocus()
        else if (WindowSurface.mode === 3) desktopMenu.forceActiveFocus()
        else forceActiveFocus()
    }
}

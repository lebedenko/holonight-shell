import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import Holonight.Core
import HolonightShell

Rectangle {
    id: root
    color: "transparent"
    focus: true
    property string selectedId: ""
    readonly property var target: WindowPresentation.revision >= 0 ? WindowPresentation.window(WindowSurface.target) : ({})
    readonly property var cards: WindowSurface.mode === 2
        ? WindowPresentation.overview.filter(w => WindowSurface.choices.indexOf(w.windowId) >= 0)
        : WindowPresentation.overview
    readonly property var actions: WindowSurface.mode === 1 ? [
        { label: target.minimized ? "Restore" : "Minimize", operation: target.minimized ? 2 : 1 },
        { label: target.maximized ? "Restore size" : "Maximize", operation: target.maximized ? 4 : 3 },
        { label: target.fullscreen ? "Leave fullscreen" : "Fullscreen", operation: target.fullscreen ? 6 : 5 },
        { label: "Close", operation: 7 }
    ].filter(action => target.operations && target.operations.indexOf(action.operation) >= 0) : []
    property int actionIndex: 0
    onActionsChanged: actionIndex = Math.max(0, Math.min(actionIndex, actions.length - 1))
    function clamp(value, size, extent) { return Math.max(8, Math.min(value, extent - size - 8)) }
    function dispatchAction() {
        if (!actions.length) return
        WindowPresentation.command(WindowSurface.target, actions[actionIndex].operation)
        WindowSurface.hide()
    }
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
    onSelectedIdChanged: Qt.callLater(() => {
        const index = cards.findIndex(w => w.windowId === selectedId)
        if (index >= 0) list.positionViewAtIndex(index, ListView.Contain)
    })
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
        if (WindowSurface.mode === 1) {
            if (event.key === Qt.Key_Down) actionIndex = Math.min(actions.length - 1, actionIndex + 1)
            else if (event.key === Qt.Key_Up) actionIndex = Math.max(0, actionIndex - 1)
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) dispatchAction()
            else return
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
        acceptedButtons: Qt.AllButtons
        onClicked: WindowSurface.hide()
    }
    DesktopMenu {
        id: desktopMenu
        anchors.fill: parent
        visible: WindowSurface.mode === 3
    }
    Rectangle {
        visible: WindowSurface.mode !== 3
        id: panel
        objectName: "windowMenuPanel"
        width: Math.min(360, Math.max(0, root.width - 16))
        readonly property real preferredY: WindowSurface.besideAnchor ? WindowSurface.anchor.y : WindowSurface.anchor.y + WindowSurface.anchor.height + 4
        readonly property real availableHeight: Math.max(0, root.height - Math.max(8, preferredY) - 8)
        height: Math.min(column.implicitHeight + 16, root.height - 16)
        x: root.clamp(WindowSurface.besideAnchor
            ? (WindowSurface.anchor.x + WindowSurface.anchor.width + 4 + width <= root.width - 8
                ? WindowSurface.anchor.x + WindowSurface.anchor.width + 4 : WindowSurface.anchor.x - width - 4)
            : WindowSurface.anchor.x, width, root.width)
        y: root.clamp(preferredY, height, root.height)
        color: HoloniightPalette.surface
        border.color: HoloniightPalette.borderSubtle
        radius: 8
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }
        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: 8
            spacing: 4
            Controls.Label {
                visible: WindowSurface.mode === 1
                text: root.target.title || "Window"
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
                Keys.onDownPressed: root.move(1)
                Keys.onUpPressed: root.move(-1)
                Keys.onEscapePressed: WindowSurface.hide()
                onAccepted: { if (root.selectedId) root.activate(root.selectedId) }
            }
            ListView {
                id: list
                visible: WindowSurface.mode === 0 || WindowSurface.mode === 2
                Layout.fillWidth: true
                objectName: "windowMenuList"
                Layout.preferredHeight: Math.min(480, Math.max(40, root.cards.length * 40),
                    Math.max(40, panel.availableHeight - 16 - (search.visible ? search.implicitHeight + 4 : 0)))
                clip: true
                spacing: 0
                model: root.cards
                Controls.ScrollBar.vertical: Controls.ScrollBar {}
                delegate: Rectangle {
                    id: card
                    required property var modelData
                    width: list.width
                    height: 40
                    color: root.selectedId === modelData.windowId ? HoloniightPalette.borderActive : HoloniightPalette.surface
                    radius: 6
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        hoverEnabled: true
                        onEntered: root.selectedId = card.modelData.windowId
                        onClicked: mouse => {
                            root.selectedId = card.modelData.windowId
                            if (mouse.button === Qt.RightButton) {
                                const point = card.mapToGlobal(0, 0)
                                WindowSurface.menu(card.modelData.windowId, WindowSurface.screenName,
                                    Qt.rect(point.x, point.y, card.width, card.height), true)
                            }
                            else root.activate(card.modelData.windowId)
                        }
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        AppWindowIcon {
                            appId: card.modelData.appId
                            iconName: LauncherService.iconForAppId(card.modelData.appId)
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                        }
                        Controls.Label {
                            text: (card.modelData.minimized ? "○ " : "") + (card.modelData.title || card.modelData.appId || "Window")
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
                Controls.Label { anchors.centerIn: parent; visible: root.cards.length === 0; text: "No windows found" }
            }
            Repeater {
                model: root.actions
                Controls.ItemDelegate {
                    required property var modelData
                    required property int index
                    objectName: "windowAction" + modelData.operation
                    text: modelData.label
                    focusPolicy: Qt.NoFocus
                    highlighted: root.actionIndex === index
                    hoverEnabled: true
                    onHoveredChanged: { if (hovered) root.actionIndex = index }
                    Layout.preferredHeight: 40
                    Layout.fillWidth: true
                    visible: !!root.target.operations && root.target.operations.indexOf(modelData.operation) >= 0
                    onClicked: {
                        WindowPresentation.command(WindowSurface.target, modelData.operation)
                        WindowSurface.hide()
                    }
                }
            }
        }
    }
    Component.onCompleted: {
        selectedId = cards.length ? cards[0].windowId : ""
        if (WindowSurface.mode === 0) search.forceActiveFocus()
        else if (WindowSurface.mode === 3) desktopMenu.forceActiveFocus()
        else forceActiveFocus()
    }
}

import QtQuick
import QtQuick.Controls.Basic
import Holonight.Core

FocusScope {
    id: root
    objectName: "authenticationPrompt"
    required property var promptModel
    property bool revealed: false
    readonly property bool secret: promptModel.inputMode === 2
    readonly property alias inputControl: field
    readonly property alias revealControl: revealButton
    signal navigate(bool forward)
    signal submit(string value)
    implicitHeight: 50
    implicitWidth: 300

    function submitResponse() {
        if (!field.enabled || !root.visible)
            return
        const response = field.text
        root.clear()
        root.submit(response)
    }
    function forceFocus() { field.forceActiveFocus(Qt.TabFocusReason) }
    function clear() { field.clear(); root.revealed = false }

    TextField {
        id: field
        objectName: "responseField"
        anchors.fill: parent
        activeFocusOnTab: true
        Accessible.role: Accessible.EditableText
        Accessible.name: qsTr("Authentication response")
        Accessible.description: qsTr("Authentication response")
        echoMode: root.secret && !root.revealed ? TextInput.Password : TextInput.Normal
        inputMethodHints: root.secret
                          ? Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                          : Qt.ImhNone
        font.pixelSize: 19
        color: HoloniightPalette.textPrimary
        leftPadding: 18
        rightPadding: revealButton.visible ? 58 : 18
        selectByMouse: !root.secret
        enabled: root.promptModel.lifecycleState === 2
        background: Rectangle {
            radius: 5
            color: HoloniightPalette.background
            border.color: field.activeFocus ? HoloniightPalette.accentCyan : HoloniightPalette.borderActive
            border.width: field.activeFocus ? 2 : 1
        }
        onSelectedTextChanged: {
            if (root.secret && selectedText.length > 0)
                deselect()
        }
        Keys.onTabPressed: function(event) { root.navigate(true); event.accepted = true }
        Keys.onBacktabPressed: function(event) { root.navigate(false); event.accepted = true }
        Keys.onReturnPressed: function(event) { root.submitResponse(); event.accepted = true }
        Keys.onEnterPressed: function(event) { root.submitResponse(); event.accepted = true }
        Keys.onPressed: function(event) {
            if (root.secret && (event.matches(StandardKey.SelectAll)
                    || event.matches(StandardKey.Copy) || event.matches(StandardKey.Cut))) {
                field.deselect()
                event.accepted = true
            }
        }
        Keys.onEscapePressed: function(event) {
            root.clear()
            root.promptModel.cancel()
            event.accepted = true
        }
    }
    Button {
        id: revealButton
        objectName: "revealButton"
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 6
        width: 40
        height: 38
        visible: root.secret
        enabled: field.enabled
        checkable: true
        checked: root.revealed
        focusPolicy: Qt.StrongFocus
        Accessible.name: root.revealed ? qsTr("Hide password") : qsTr("Show password")
        contentItem: AuthenticationIcon {
            kind: root.revealed ? "hidden" : "eye"
            color: revealButton.activeFocus ? HoloniightPalette.accentCyan : HoloniightPalette.textMuted
        }
        background: Rectangle {
            color: "transparent"
            radius: 4
            border.width: revealButton.activeFocus ? 2 : 0
            border.color: HoloniightPalette.accentCyan
        }
        onClicked: root.revealed = !root.revealed
        Keys.onTabPressed: function(event) { root.navigate(true); event.accepted = true }
        Keys.onBacktabPressed: function(event) { root.navigate(false); event.accepted = true }
        Keys.onReturnPressed: function(event) { revealButton.clicked(); event.accepted = true }
        Keys.onEnterPressed: function(event) { revealButton.clicked(); event.accepted = true }
    }
    Connections {
        target: root.promptModel
        function onClearSensitiveInput() { root.clear() }
        function onLifecycleStateChanged() { root.clear() }
        function onPromptChanged() { root.clear() }
        function onRequestTokenChanged() { root.clear() }
        function onSessionGenerationChanged() { root.clear() }
    }
    Component.onDestruction: root.clear()
}

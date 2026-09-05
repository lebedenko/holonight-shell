pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Holonight.Core

ComboBox {
    id: root
    objectName: "identitySelector"
    required property var promptModel
    signal navigate(bool forward)
    Keys.onTabPressed: function(event) { root.popup.close(); root.navigate(true); event.accepted = true }
    Keys.onBacktabPressed: function(event) { root.popup.close(); root.navigate(false); event.accepted = true }
    readonly property var account: promptModel.selectedAccount
    readonly property string accountName: account.fullName || account.username || account.displayLabel || ""
    implicitHeight: 82
    model: promptModel.identities
    textRole: "displayLabel"
    valueRole: "stableId"
    enabled: promptModel.lifecycleState === 1 && count > 1
    focusPolicy: enabled ? Qt.StrongFocus : Qt.NoFocus
    Accessible.name: qsTr("Authentication account: %1").arg(accountName)
    leftPadding: 16
    rightPadding: 48

    function selectPreferred() {
        currentIndex = indexOfValue(root.promptModel.selectedIdentity)
    }
    onCountChanged: Qt.callLater(selectPreferred)
    Component.onCompleted: Qt.callLater(selectPreferred)
    Connections {
        target: root.promptModel
        function onSelectedIdentityChanged() { root.selectPreferred() }
    }
    onActivated: root.promptModel.selectIdentity(currentValue)

    background: Rectangle {
        radius: 6
        color: HoloniightPalette.surfaceRaised
        border.color: root.activeFocus ? HoloniightPalette.accentCyan : HoloniightPalette.borderPassive
        border.width: root.activeFocus ? 2 : 1
    }
    contentItem: RowLayout {
        spacing: 18
        Rectangle {
            Layout.preferredWidth: 56
            Layout.preferredHeight: 56
            radius: 28
            color: HoloniightPalette.background
            border.color: HoloniightPalette.borderActive
            Image {
                id: avatar
                objectName: "accountAvatar"
                anchors.fill: parent
                anchors.margins: 3
                readonly property string localSource: String(root.account.avatarUrl || "")
                source: localSource.startsWith("file:///") ? localSource : ""
                sourceSize.width: 56
                sourceSize.height: 56
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                visible: status === Image.Ready
            }
            AuthenticationIcon {
                objectName: "accountAvatarFallback"
                anchors.centerIn: parent
                kind: "user"
                size: 30
                visible: avatar.status !== Image.Ready
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Label {
                objectName: "accountName"
                Layout.fillWidth: true
                text: root.accountName
                textFormat: Text.PlainText
                font.pixelSize: 20
                elide: Text.ElideRight
                color: HoloniightPalette.textPrimary
            }
            Label {
                objectName: "accountUsername"
                Layout.fillWidth: true
                text: root.account.username || ""
                visible: text.length > 0 && text !== root.accountName
                textFormat: Text.PlainText
                font.pixelSize: 17
                elide: Text.ElideRight
                color: HoloniightPalette.textMuted
            }
        }
    }
    indicator: AuthenticationIcon {
        x: root.width - width - 20
        y: (root.height - height) / 2
        visible: root.enabled
        kind: "chevron"
        size: 20
        color: HoloniightPalette.textMuted
    }
    delegate: ItemDelegate {
        id: option
        required property string stableId
        required property string displayLabel
        required property string username
        required property string fullName
        required property int index
        width: root.popup.width
        highlighted: root.highlightedIndex === index
        text: fullName || username || displayLabel
        contentItem: ColumnLayout {
            spacing: 3
            Label {
                Layout.fillWidth: true
                text: option.text
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: HoloniightPalette.textPrimary
            }
            Label {
                Layout.fillWidth: true
                visible: text.length > 0 && text !== option.text
                text: option.username
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: HoloniightPalette.textMuted
            }
        }
    }
}

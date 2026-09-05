pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Holonight as H
import Holonight.Core
import Holonight.Controls

H.ComboBox {
    id: root
    objectName: "identitySelector"
    required property var promptModel
    signal navigate(bool forward)
    Keys.onTabPressed: function(event) { root.popup.close(); root.navigate(true); event.accepted = true }
    Keys.onBacktabPressed: function(event) { root.popup.close(); root.navigate(false); event.accepted = true }
    readonly property var account: promptModel.selectedAccount
    readonly property string accountName: account.fullName || account.username || account.displayLabel || ""
    implicitHeight: 82
    delegateHeight: 82
    model: promptModel.identities
    textRole: "displayLabel"
    valueRole: "stableId"
    enabled: promptModel.lifecycleState === 1 && count > 1
    focusPolicy: enabled ? Qt.StrongFocus : Qt.NoFocus
    Accessible.name: qsTr("Authentication account: %1").arg(accountName)
    leftPadding: 16
    rightPadding: 48

    function confirmDisplayedIdentity() {
        if (enabled && !popup.visible && currentIndex >= 0)
            promptModel.selectIdentity(currentValue)
    }
    Keys.onReturnPressed: function(event) {
        if (root.popup.visible) { event.accepted = false; return }
        root.confirmDisplayedIdentity()
        event.accepted = true
    }
    Keys.onEnterPressed: function(event) {
        if (root.popup.visible) { event.accepted = false; return }
        root.confirmDisplayedIdentity()
        event.accepted = true
    }

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
        HnAvatar {
            objectName: "accountAvatar"
            Layout.preferredWidth: 56
            Layout.preferredHeight: 56
            readonly property string localSource: String(root.account.avatarUrl || "")
            source: localSource.startsWith("file:///") ? localSource : ""
            fallbackSource: Qt.resolvedUrl("user-avatar.svg")
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
    delegate: H.ItemDelegate {
        id: option
        required property string stableId
        required property string displayLabel
        required property string username
        required property string fullName
        required property url avatarUrl
        required property int index
        width: root.popup.availableWidth
        height: root.delegateHeight
        highlighted: root.highlightedIndex === index
        text: option.fullName || option.username || option.displayLabel
        contentItem: RowLayout {
            spacing: 18
            HnAvatar {
                objectName: "identityOptionAvatar"
                Layout.preferredWidth: 56
                Layout.preferredHeight: 56
                source: String(option.avatarUrl).startsWith("file:///") ? option.avatarUrl : ""
                fallbackSource: Qt.resolvedUrl("user-avatar.svg")
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Label {
                    Layout.fillWidth: true
                    text: option.text
                    font.pixelSize: 20
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: HoloniightPalette.textPrimary
                }
                Label {
                    Layout.fillWidth: true
                    visible: text.length > 0 && text !== option.text
                    text: option.username
                    font.pixelSize: 17
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: HoloniightPalette.textMuted
                }
            }
        }
    }
}

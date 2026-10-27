import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import Holonight.Core

ColumnLayout {
    id: root
    required property var controller
    readonly property var request: controller.interaction
    readonly property string kind: request.kind || ""
    readonly property bool inputNeeded: kind === "pin-entry" || kind === "passkey-entry"
    readonly property bool displayOnly: kind === "pin-display" || kind === "passkey-display"
    visible: !!request.id
    spacing: 8

    HnLabel {
        Layout.fillWidth: true
        role: HnTypographyRole.Title
        rawText: qsTr("Bluetooth pairing")
    }
    HnLabel {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        rawText: root.inputNeeded ? qsTr("Enter the code from your device") : root.displayOnly ? qsTr("Type this code on your device, then press Enter") : root.kind === "confirmation" ? qsTr("Does this code match the one on your device?") : root.kind === "service-authorization" ? qsTr("Allow service: %1?").arg(root.request.service || "") : qsTr("Allow this device to pair?")
    }
    HnLabel {
        Layout.fillWidth: true
        visible: !!root.request.code
        role: HnTypographyRole.Title
        rawText: root.request.code || ""
    }
    HnLabel {
        Layout.fillWidth: true
        visible: root.kind === "passkey-display"
        rawText: qsTr("%1 of 6 digits entered").arg(root.request.entered || 0)
    }
    Controls.TextField {
        id: input
        objectName: "bluetoothPairingInput"
        Layout.fillWidth: true
        visible: root.inputNeeded
        maximumLength: root.kind === "passkey-entry" ? 6 : 16
        placeholderText: qsTr("Pairing code")
        validator: RegularExpressionValidator {
            regularExpression: root.kind === "passkey-entry" ? /^[0-9]{1,6}$/ : /^.{1,16}$/
        }
        onAccepted: if (acceptableInput && text.length > 0)
            root.controller.respond(true, text)
    }
    Connections {
        target: root.controller
        function onPairingRequested() {
            input.clear();
            if (root.inputNeeded)
                input.forceActiveFocus();
        }
    }
    RowLayout {
        Controls.Button {
            objectName: "bluetoothPairingAccept"
            text: qsTr("Accept")
            visible: !root.displayOnly
            enabled: !root.inputNeeded || (root.kind === "passkey-entry" ? /^[0-9]{1,6}$/.test(input.text) : input.text.length > 0)
            onClicked: root.controller.respond(true, input.text)
        }
        Controls.Button {
            objectName: "bluetoothPairingCancel"
            text: qsTr("Cancel")
            onClicked: root.controller.cancelPairing()
        }
    }
}

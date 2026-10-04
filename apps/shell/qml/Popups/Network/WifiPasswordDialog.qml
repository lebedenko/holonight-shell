import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls

Controls.Popup {
    id: root

    property string ssid: ""
    property int row: -1
    property string passwordText: ""

    signal accepted(int row, string password)

    modal: true
    focus: true
    width: Math.min(parent ? parent.width - 40 : 360, 360)
    implicitHeight: form.implicitHeight + 32 + topPadding + bottomPadding
    height: Math.min(implicitHeight, parent ? parent.height : implicitHeight)
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside
    onOpened: passwordForm.forceActiveFocus()
    onClosed: root.passwordText = ""

    Controls.ScrollView {
        id: dialogScroll
        anchors.fill: parent
        anchors.margins: 16
        contentWidth: availableWidth

        ColumnLayout {
            id: form

            width: dialogScroll.availableWidth
            spacing: 8

            HnLabel {
                Layout.fillWidth: true
                rawText: root.ssid
                role: HnTypographyRole.Subheading
                color: HoloniightPalette.textPrimary
                font.bold: true
                elide: Text.ElideRight
            }

            HnFormField {
                id: passwordForm

                objectName: "passwordForm"
                Layout.fillWidth: true
                labelText: qsTr("Password")
                required: true
                hasError: root.passwordText.length === 0 && activeFocus
                errorText: qsTr("Password is required")

                control: Component {
                    Controls.TextField {
                        id: passwordField

                        objectName: "passwordField"
                        text: root.passwordText
                        echoMode: TextInput.Password
                        placeholderText: qsTr("Enter password")
                        onTextEdited: root.passwordText = text
                        onAccepted: submitButton.clicked()
                    }

                }

            }

            RowLayout {
                Layout.fillWidth: true

                Item {
                    Layout.fillWidth: true
                }

                Controls.Button {
                    objectName: "cancelButton"
                    text: qsTr("Cancel")
                    onClicked: root.close()
                }

                Controls.Button {
                    id: submitButton

                    objectName: "connectButton"
                    text: qsTr("Connect")
                    enabled: root.passwordText.length > 0
                    onClicked: {
                        root.accepted(root.row, root.passwordText);
                        root.passwordText = "";
                        root.close();
                    }
                }

            }

        }

    }

    background: HnSurfaceFrame {
        surfaceRole: HnSurfaceRole.Popup
        fillColor: HoloniightPalette.surfaceRaised
        borderColor: HoloniightPalette.borderPassive
        borderWidth: HnMetrics.borderWidth
    }

}

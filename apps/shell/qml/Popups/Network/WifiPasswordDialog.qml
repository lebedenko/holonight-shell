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
  height: 204
  closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside

  background: HnSurfaceFrame {
    surfaceRole: HnSurfaceRole.Popup
    fillColor: HoloniightPalette.surfaceRaised
    borderColor: HoloniightPalette.borderPassive
    borderWidth: HnMetrics.borderWidth
  }

  ColumnLayout {
    anchors.fill: parent
    anchors.margins: 16
    spacing: 10

    Text {
      Layout.fillWidth: true
      text: root.ssid
      color: HoloniightPalette.textPrimary
      font.pixelSize: 16
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
      Item { Layout.fillWidth: true }
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
          root.accepted(root.row, root.passwordText)
          root.passwordText = ""
          root.close()
        }
      }
    }
  }

  onOpened: passwordForm.forceActiveFocus()
  onClosed: root.passwordText = ""
}

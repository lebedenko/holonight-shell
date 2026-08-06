import QtQuick
import QtTest
import Holonight.Controls

import HolonightShell

TestCase {
  id: root

  name: "WifiPasswordDialog"
  when: windowShown

  Component {
    id: dialogComponent

    WifiPasswordDialog {
      parent: root
      ssid: "Secured Wi-Fi"
      row: 4
    }
  }

  function test_uses_form_field_and_focuses_password_control() {
    const dialog = createTemporaryObject(dialogComponent, root)
    verify(dialog)

    const form = findChild(dialog, "passwordForm")
    const field = findChild(dialog, "passwordField")
    verify(form)
    verify(field)
    verify(form instanceof HnFormField)

    dialog.open()
    tryCompare(dialog, "opened", true)
    tryVerify(() => field.activeFocus)
    compare(findChild(dialog, "connectButton").enabled, false)
    dialog.close()
  }

  function test_submit_validates_and_emits_row_and_password() {
    const dialog = createTemporaryObject(dialogComponent, root)
    verify(dialog)

    const spy = signalSpy.createObject(dialog, { "target": dialog, "signalName": "accepted" })
    dialog.passwordText = "correct horse"
    const connectButton = findChild(dialog, "connectButton")
    compare(connectButton.enabled, true)
    connectButton.clicked()

    compare(spy.count, 1)
    compare(spy.signalArguments[0][0], 4)
    compare(spy.signalArguments[0][1], "correct horse")
    compare(dialog.passwordText, "")
  }

  function test_cancel_closes_without_accepting() {
    const dialog = createTemporaryObject(dialogComponent, root)
    verify(dialog)

    const spy = signalSpy.createObject(dialog, { "target": dialog, "signalName": "accepted" })
    dialog.passwordText = "discard me"
    dialog.open()
    tryCompare(dialog, "opened", true)
    findChild(dialog, "cancelButton").clicked()
    tryCompare(dialog, "opened", false)
    compare(spy.count, 0)
    compare(dialog.passwordText, "")
  }

  Component {
    id: signalSpy

    SignalSpy {}
  }
}

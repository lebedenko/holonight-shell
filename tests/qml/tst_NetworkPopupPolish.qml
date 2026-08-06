import QtQuick
import QtTest
import Holonight.Core
import Holonight.Controls

import HolonightShell

TestCase {
  id: root

  name: "NetworkPopupPolish"
  when: windowShown

  Component {
    id: contentComponent

    NetworkPopupContent {
      width: 560
      height: 760
    }
  }

  Component {
    id: actionsComponent

    NetworkActionRow {
      width: 560
      height: 134
    }
  }

  function test_rescan_uses_secondary_resting_emphasis() {
    const content = createTemporaryObject(contentComponent, root)
    verify(content)

    compare(findChild(content, "rescanIcon").iconColor, HoloniightPalette.textSecondary)
    compare(findChild(content, "rescanLabel").color, HoloniightPalette.textSecondary)
  }

  function test_current_connection_separator_keeps_pixel_height() {
    const content = createTemporaryObject(contentComponent, root)
    verify(content)

    const separator = findChild(content, "currentConnectionSeparator")
    verify(separator)
    verify(separator.height >= 1)
    compare(separator.color.a, content.sectionSeparatorOpacity)
  }

  function test_actions_use_shared_delegates_and_preserve_signals() {
    const actions = createTemporaryObject(actionsComponent, root)
    verify(actions)

    const settingsAction = findChild(actions, "networkSettingsAction")
    const infoAction = findChild(actions, "networkInfoAction")
    verify(settingsAction)
    verify(infoAction)
    verify(settingsAction instanceof HnActionDelegate)
    verify(infoAction instanceof HnActionDelegate)
    compare(infoAction.enabled, true)

    const settingsSpy = signalSpy.createObject(actions,
                                               { "target": actions, "signalName": "settingsRequested" })
    const infoSpy = signalSpy.createObject(actions,
                                           { "target": actions, "signalName": "infoRequested" })
    settingsAction.clicked()
    infoAction.clicked()
    compare(settingsSpy.count, 1)
    compare(infoSpy.count, 1)
  }

  Component {
    id: signalSpy

    SignalSpy {}
  }
}

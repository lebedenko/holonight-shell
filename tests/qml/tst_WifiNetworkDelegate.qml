import QtQuick
import QtTest
import Holonight.Core
import Holonight.Components
import Holonight.Controls

import HolonightShell

TestCase {
  id: root

  name: "WifiNetworkDelegate"
  when: windowShown

  Component {
    id: listComponent

    ListView {
      id: listView

      width: 320
      height: 128
      model: ListModel {
        ListElement {
          ssid: "First"
          strength: 80
          secured: true
          known: true
          connected: false
        }
        ListElement {
          ssid: "Second"
          strength: 60
          secured: true
          known: true
          connected: false
        }
      }
      delegate: WifiNetworkDelegate {
        networkCount: listView.count
      }
    }
  }

  Component {
    id: tierListComponent

    ListView {
      id: tierList

      width: 320
      height: 192
      model: ListModel {
        ListElement {
          ssid: "Weak"
          strength: 25
          secured: true
          known: false
          connected: false
        }
        ListElement {
          ssid: "Medium"
          strength: 55
          secured: true
          known: true
          connected: false
        }
        ListElement {
          ssid: "Strong"
          strength: 87
          secured: true
          known: true
          connected: true
        }
      }
      delegate: WifiNetworkDelegate {
        networkCount: tierList.count
      }
    }
  }

  function test_separator_is_visible_between_networks() {
    const list = createTemporaryObject(listComponent, root)
    verify(list)
    tryCompare(list, "count", 2)

    const firstDelegate = list.itemAtIndex(0)
    verify(firstDelegate)
    compare(firstDelegate.index, 0)
    compare(firstDelegate.connected, false)
    compare(firstDelegate.networkCount, 2)
    compare(firstDelegate.showSeparator, true)
    compare(firstDelegate.dividerVisible, true)
  }

  function test_connected_row_is_current_and_uses_balanced_selection() {
    const list = createTemporaryObject(tierListComponent, root)
    verify(list)
    tryCompare(list, "count", 3)

    const connectedDelegate = list.itemAtIndex(2)
    verify(connectedDelegate)
    verify(connectedDelegate instanceof HnListDelegate)
    compare(connectedDelegate.subtitle, "Current")
    compare(connectedDelegate.checked, true)
    compare(connectedDelegate.selectionStyle, HnListDelegate.AccentEdge)
  }

  function test_signal_quality_uses_three_palette_tiers() {
    const list = createTemporaryObject(tierListComponent, root)
    verify(list)
    tryCompare(list, "count", 3)

    compare(list.itemAtIndex(0).signalQualityColor, HoloniightPalette.borderUrgent)
    compare(list.itemAtIndex(1).signalQualityColor, HoloniightPalette.accentBlue)
    compare(list.itemAtIndex(2).signalQualityColor, HoloniightPalette.accentCyan)
  }

  function test_signal_percentage_is_tightly_grouped() {
    const list = createTemporaryObject(tierListComponent, root)
    verify(list)
    tryCompare(list, "count", 3)

    const connectedDelegate = list.itemAtIndex(2)
    const strengthText = findChild(connectedDelegate, "strengthText")
    const signalIcon = findChild(connectedDelegate, "signalIcon")
    verify(strengthText)
    verify(signalIcon)
    compare(signalIcon.x - (strengthText.x + strengthText.width), 8)
  }

  function test_unknown_secured_network_requests_password() {
    const list = createTemporaryObject(tierListComponent, root)
    verify(list)
    tryCompare(list, "count", 3)

    const delegate = list.itemAtIndex(0)
    const spy = signalSpy.createObject(delegate,
                                       { "target": delegate, "signalName": "passwordRequested" })
    delegate.clicked()
    compare(spy.count, 1)
    compare(spy.signalArguments[0][0], 0)
    compare(spy.signalArguments[0][1], "Weak")
  }

  Component {
    id: signalSpy

    SignalSpy {}
  }
}

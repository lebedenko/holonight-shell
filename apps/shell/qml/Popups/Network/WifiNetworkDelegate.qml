pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Components
import Holonight.Controls
import HolonightShell

import "../../Controls"

HnListDelegate {
  id: root

  required property int index
  required property string ssid
  required property int strength
  required property bool secured
  required property bool known
  required property bool connected
  required property int networkCount

  property int row: root.index
  readonly property bool showSeparator: !root.connected && root.index < root.networkCount - 1
  readonly property color signalQualityColor: root.strength >= 75
                                               ? HoloniightPalette.accentCyan
                                               : root.strength >= 40
                                                 ? HoloniightPalette.accentBlue
                                                 : HoloniightPalette.borderUrgent
  signal passwordRequested(int row, string ssid)

  width: ListView.view ? ListView.view.width : 560
  implicitHeight: 64
  title: root.ssid
  subtitle: root.connected
            ? qsTr("Current")
            : (root.known ? qsTr("Known") : (root.secured ? qsTr("Secured") : qsTr("Open")))
  checked: root.connected
  selectionStyle: HnListDelegate.AccentEdge
  sizeRole: HnControlSize.Large
  dividerVisible: root.showSeparator
  onClicked: {
    if (root.connected) {
      NetworkService.disconnectActive()
    } else if (root.secured && !root.known) {
      root.passwordRequested(root.row, root.ssid)
    } else {
      NetworkService.connectNetwork(root.row)
    }
  }

  leadingContent: Component {
    BarIcon {
      width: 32
      height: 32
      name: "wifi_online"
      glowEnabled: root.connected
      glowOpacity: 0.14
      glowColor: root.signalQualityColor
    }
  }

  trailingContent: Component {
    RowLayout {
      spacing: 8

      Text {
        objectName: "strengthText"

        Layout.preferredWidth: 44
        text: qsTr("%1%").arg(root.strength)
        color: root.signalQualityColor
        horizontalAlignment: Text.AlignRight
        font.pixelSize: 14
        font.weight: root.connected ? Font.Medium : Font.Normal
      }

      SignalStrengthIcon {
        objectName: "signalIcon"

        Layout.preferredWidth: 20
        Layout.preferredHeight: 16
        signalStrength: root.strength
        activeColor: root.signalQualityColor
      }

      NetworkPopupIcon {
        objectName: "securityIcon"

        Layout.preferredWidth: 20
        Layout.preferredHeight: 20
        visible: root.secured
        name: "lock"
        iconColor: root.connected ? HoloniightPalette.textPrimary : HoloniightPalette.textSecondary
        strokeWidth: 1.6
      }
    }
  }
}

import QtQuick
import QtQuick.Layouts
import Holonight.Core

import HolonightShell

Item {
  id: root

  Layout.fillWidth: true
  Layout.fillHeight: true

  signal passwordRequested(int row, string ssid)

  ListView {
    id: listView
    anchors.fill: parent
    clip: true
    spacing: 0
    model: NetworkService.wifiNetworks
    visible: NetworkService.available && NetworkService.wifiEnabled && count > 0

    delegate: WifiNetworkDelegate {
      networkCount: listView.count
      onPasswordRequested: (row, ssid) => root.passwordRequested(row, ssid)
    }
  }

  HnLabel {
    anchors.centerIn: parent
    width: parent.width - 32
    horizontalAlignment: Text.AlignHCenter
    rawText: {
      if (!NetworkService.available) return qsTr("NetworkManager unavailable")
      if (!NetworkService.wifiHardwareEnabled) return qsTr("Wi-Fi hardware disabled")
      if (!NetworkService.wifiEnabled) return qsTr("Wi-Fi is turned off")
      if (NetworkService.scanning) return qsTr("Scanning for Wi-Fi networks")
      return qsTr("No Wi-Fi networks found")
    }
    role: HnTypographyRole.Body
    color: NetworkService.available && NetworkService.wifiEnabled ? HoloniightPalette.textSecondary : HoloniightPalette.error
    wrapMode: Text.Wrap
    visible: !listView.visible
  }
}

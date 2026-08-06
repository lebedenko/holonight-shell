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

  Text {
    anchors.centerIn: parent
    width: parent.width - 32
    horizontalAlignment: Text.AlignHCenter
    text: {
      if (!NetworkService.available) return qsTr("NetworkManager unavailable")
      if (!NetworkService.wifiHardwareEnabled) return qsTr("Wi-Fi hardware disabled")
      if (!NetworkService.wifiEnabled) return qsTr("Wi-Fi is turned off")
      if (NetworkService.scanning) return qsTr("Scanning for Wi-Fi networks")
      return qsTr("No Wi-Fi networks found")
    }
    color: NetworkService.available && NetworkService.wifiEnabled ? HoloniightPalette.textSecondary : HoloniightPalette.error
    font.pixelSize: 14
    wrapMode: Text.Wrap
    visible: !listView.visible
  }
}

import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Holonight.Core
import HolonightShell

import "../../Controls"

RowLayout {
  id: root

  Layout.fillWidth: true
  Layout.preferredHeight: 42
  spacing: 14

  BarIcon {
    Layout.preferredWidth: 28
    Layout.preferredHeight: 28
    name: NetworkService.wifiEnabled ? "wifi_online" : "wifi_offline"
    signalStrength: NetworkService.strength
    glowEnabled: NetworkService.wifiEnabled
    glowOpacity: 0.18
  }

  Text {
    Layout.fillWidth: true
    text: qsTr("NETWORK")
    color: HoloniightPalette.textPrimary
    font.pixelSize: 16
    font.weight: Font.DemiBold
    elide: Text.ElideRight
  }

  Item {
    Layout.preferredWidth: 52
    Layout.preferredHeight: 28
    opacity: NetworkService.available && NetworkService.wifiHardwareEnabled ? 1.0 : 0.5

    MultiEffect {
      source: switchTrack
      anchors.fill: switchTrack
      visible: NetworkService.wifiEnabled
      shadowEnabled: true
      shadowColor: HoloniightPalette.accentCyan
      shadowBlur: 0.6
      shadowOpacity: 0.24
      shadowScale: 1.05
      autoPaddingEnabled: true
    }

    Rectangle {
      id: switchTrack
      anchors.fill: parent
      radius: height / 2
      color: NetworkService.wifiEnabled
             ? Qt.rgba(HoloniightPalette.accentBlue.r, HoloniightPalette.accentBlue.g,
                       HoloniightPalette.accentBlue.b, 0.34)
             : Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                       HoloniightPalette.surface.b, 0.5)
      border.width: 1
      border.color: NetworkService.wifiEnabled ? HoloniightPalette.accentCyan : HoloniightPalette.borderPassive
    }

    Rectangle {
      width: 22
      height: 22
      radius: 11
      x: NetworkService.wifiEnabled ? parent.width - width - 3 : 3
      anchors.verticalCenter: parent.verticalCenter
      color: HoloniightPalette.textPrimary

      Behavior on x {
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
      }
    }

    MouseArea {
      anchors.fill: parent
      enabled: NetworkService.available && NetworkService.wifiHardwareEnabled
      cursorShape: Qt.PointingHandCursor
      onClicked: NetworkService.setWifiEnabled(!NetworkService.wifiEnabled)
    }
  }
}

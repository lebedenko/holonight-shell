import QtQuick
import QtQuick.Layouts
import Holonight.Core
import HolonightShell

import "../../Controls"

Item {
  id: root

  readonly property string activeBandText: formatBand(NetworkService.activeFrequencyMhz)
  readonly property string activeLinkSpeedText: formatLinkSpeed(NetworkService.activeLinkSpeedMbps)

  Layout.fillWidth: true
  Layout.preferredHeight: 160

  function formatBand(frequencyMhz) {
    if (frequencyMhz >= 2400 && frequencyMhz <= 2500) return qsTr("2.4 GHz")
    if (frequencyMhz >= 4900 && frequencyMhz <= 5900) return qsTr("5 GHz")
    if (frequencyMhz >= 5925 && frequencyMhz <= 7125) return qsTr("6 GHz")
    if (frequencyMhz > 0) return qsTr("%1 MHz").arg(frequencyMhz)
    return qsTr("Unavailable")
  }

  function formatLinkSpeed(speedMbps) {
    return speedMbps > 0 ? qsTr("%1 Mbps").arg(speedMbps) : qsTr("Unavailable")
  }

  function connectionSubtitle() {
    if (!NetworkService.available) return qsTr("NetworkManager unavailable")
    if (NetworkService.type === 2) return NetworkService.vpnActive ? qsTr("Ethernet, VPN secured") : qsTr("Ethernet connected")
    if (NetworkService.type === 1) {
      const connectedText = NetworkService.activeFrequencyMhz > 0
                            ? qsTr("Connected · %1").arg(root.activeBandText)
                            : qsTr("Connected")
      return NetworkService.vpnActive ? qsTr("%1 · VPN secured").arg(connectedText) : connectedText
    }
    return NetworkService.connectionStatus.length > 0 ? NetworkService.connectionStatus : qsTr("Disconnected")
  }

  ColumnLayout {
    anchors.fill: parent
    spacing: 10

    Text {
      Layout.fillWidth: true
      text: qsTr("CURRENT CONNECTION")
      color: HoloniightPalette.accentBlue
      font.pointSize: 9
      font.weight: Font.DemiBold
      elide: Text.ElideRight
    }

    RowLayout {
      Layout.fillWidth: true
      Layout.preferredHeight: 68
      spacing: 14

      Rectangle {
        Layout.preferredWidth: 68
        Layout.preferredHeight: 68
        radius: 7
        color: Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                       HoloniightPalette.surface.b, 0.26)
        border.width: 1
        border.color: NetworkService.online ? HoloniightPalette.borderActive : HoloniightPalette.borderPassive

        BarIcon {
          anchors.centerIn: parent
          width: 40
          height: 40
          name: !NetworkService.available || !NetworkService.online || NetworkService.type === 0
                ? "wifi_offline"
                : NetworkService.type === 2 ? "network-wired-symbolic" : "wifi_online"
          glowEnabled: NetworkService.online
          glowColor: NetworkService.vpnActive ? HoloniightPalette.accentViolet : HoloniightPalette.accentCyan
          glowOpacity: 0.18
        }
      }

      ColumnLayout {
        Layout.fillWidth: true
        spacing: 5

        Text {
          Layout.fillWidth: true
          text: NetworkService.activeConnectionName.length > 0
                ? NetworkService.activeConnectionName
                : (NetworkService.online ? qsTr("Connected") : qsTr("No active connection"))
          color: NetworkService.online ? HoloniightPalette.textPrimary : HoloniightPalette.textSecondary
          font.pointSize: 13.5
          font.weight: Font.Medium
          elide: Text.ElideRight
        }

        Text {
          objectName: "connectionSubtitle"

          Layout.fillWidth: true
          text: root.connectionSubtitle()
          color: NetworkService.vpnActive ? HoloniightPalette.accentViolet : HoloniightPalette.textMuted
          font.pointSize: 10.5
          elide: Text.ElideRight
        }
      }

      Item {
        id: signalGroup
        Layout.preferredWidth: signalPercent.implicitWidth + 4 + 22
        Layout.preferredHeight: 28
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

        SignalStrengthIcon {
          id: cardSignalBars
          anchors {
            right: signalPercent.left
            rightMargin: 4
            verticalCenter: parent.verticalCenter
          }
          width: 22
          height: 18
          signalStrength: NetworkService.online ? NetworkService.strength : 0
        }

        Text {
          id: signalPercent
          anchors {
            right: parent.right
            verticalCenter: parent.verticalCenter
          }
          text: qsTr("%1%").arg(NetworkService.strength)
          color: NetworkService.online ? HoloniightPalette.accentCyan : HoloniightPalette.textSecondary
          font.pointSize: 13.5
          font.weight: Font.Medium
        }
      }
    }

    RowLayout {
      Layout.fillWidth: true
      Layout.preferredHeight: 54
      spacing: 10

      StatTile {
        icon: "ip"
        label: qsTr("IP ADDRESS")
        value: NetworkService.activeIp4Address.length > 0 ? NetworkService.activeIp4Address : qsTr("Resolving")
      }

      StatTile {
        icon: "speed"
        label: qsTr("LINK SPEED")
        value: root.activeLinkSpeedText
      }

      StatTile {
        icon: "band"
        label: qsTr("BAND")
        value: root.activeBandText
      }
    }
  }

  component StatTile: Rectangle {
    id: tile

    property string icon: ""
    property string label: ""
    property string value: ""

    Layout.fillWidth: true
    Layout.preferredHeight: 54
    radius: 6
    color: Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                   HoloniightPalette.surface.b, 0.22)
    border.width: 1
    border.color: Qt.rgba(HoloniightPalette.borderPassive.r, HoloniightPalette.borderPassive.g,
                          HoloniightPalette.borderPassive.b, 0.5)

    Canvas {
      id: statIcon
      anchors {
        left: parent.left
        leftMargin: 12
        verticalCenter: parent.verticalCenter
      }
      width: 24
      height: 24

      onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.strokeStyle = HoloniightPalette.accentCyan
        ctx.lineWidth = 1.8
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        if (tile.icon === "ip") {
          ctx.beginPath()
          ctx.arc(12, 12, 9.5, 0, Math.PI * 2)
          ctx.moveTo(12, 2.5)
          ctx.bezierCurveTo(7.5, 6, 7.5, 18, 12, 21.5)
          ctx.moveTo(12, 2.5)
          ctx.bezierCurveTo(16.5, 6, 16.5, 18, 12, 21.5)
          ctx.moveTo(4, 7)
          ctx.bezierCurveTo(8.5, 9.5, 15.5, 9.5, 20, 7)
          ctx.moveTo(4, 17)
          ctx.bezierCurveTo(8.5, 14.5, 15.5, 14.5, 20, 17)
          ctx.stroke()
          return
        }

        if (tile.icon === "speed") {
          ctx.beginPath()
          ctx.arc(12, 14, 8, Math.PI, Math.PI * 2)
          ctx.moveTo(12, 14)
          ctx.lineTo(17, 9)
          ctx.stroke()
          return
        }

        ctx.beginPath()
        ctx.arc(12, 12, 3, 0, Math.PI * 2)
        ctx.moveTo(7, 7)
        ctx.arc(12, 12, 7, Math.PI * 1.25, Math.PI * 1.75)
        ctx.moveTo(4, 4)
        ctx.arc(12, 12, 11, Math.PI * 1.25, Math.PI * 1.75)
        ctx.stroke()
      }
    }

    Column {
      anchors {
        left: statIcon.right
        right: parent.right
        verticalCenter: parent.verticalCenter
        leftMargin: 10
        rightMargin: 10
      }
      spacing: 2

      Text {
        width: parent.width
        text: tile.label
        color: HoloniightPalette.accentBlue
        font.pointSize: 7.5
        font.weight: Font.DemiBold
        elide: Text.ElideRight
      }

      Text {
        width: parent.width
        text: tile.value
        color: HoloniightPalette.textSecondary
        font.pointSize: 9
        elide: Text.ElideRight
      }
    }
  }
}

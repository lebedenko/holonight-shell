import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls

import HolonightShell

Item {
  id: root

  readonly property real sectionSeparatorOpacity: 0.48
  readonly property real sectionSeparatorThickness: 1 / Screen.devicePixelRatio

  WifiPasswordDialog {
    id: passwordDialog
    anchors.centerIn: parent
    onAccepted: (row, password) => NetworkService.connectNetworkWithPassword(row, password)
  }

  ColumnLayout {
    anchors.fill: parent
    spacing: 12

    NetworkToggleRow {}

    HnSeparator {
      thickness: root.sectionSeparatorThickness
      centerOpacity: root.sectionSeparatorOpacity
      fadeMode: HnSeparator.Solid
      Layout.fillWidth: true
      Layout.minimumHeight: root.sectionSeparatorThickness
      Layout.preferredHeight: root.sectionSeparatorThickness
    }

    NetworkCurrentCard {}

    Text {
      Layout.fillWidth: true
      visible: NetworkService.lastError.length > 0
      text: NetworkService.lastError
      color: NetworkService.lastError === qsTr("Password required") ? HoloniightPalette.textSecondary : HoloniightPalette.error
      font.pointSize: 9
      elide: Text.ElideRight
    }

    Rectangle {
      objectName: "currentConnectionSeparator"

      Layout.fillWidth: true
      Layout.minimumHeight: 1
      Layout.preferredHeight: 1
      color: Qt.rgba(HoloniightPalette.borderPassive.r,
                     HoloniightPalette.borderPassive.g,
                     HoloniightPalette.borderPassive.b,
                     root.sectionSeparatorOpacity)
    }

    RowLayout {
      Layout.fillWidth: true
      Layout.preferredHeight: 20
      spacing: 12

      Text {
        Layout.fillWidth: true
        text: qsTr("WI-FI NETWORKS")
        color: HoloniightPalette.accentBlue
        font.pointSize: 9
        font.weight: Font.DemiBold
        elide: Text.ElideRight
      }

      Item {
        id: rescanButton

        implicitWidth: rescanRow.implicitWidth
        implicitHeight: rescanRow.implicitHeight
        opacity: NetworkService.available && NetworkService.wifiEnabled ? 1.0 : 0.55

        RowLayout {
          id: rescanRow
          anchors.fill: parent
          spacing: 5

          NetworkPopupIcon {
            objectName: "rescanIcon"

            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            name: "refresh"
            iconColor: !NetworkService.available || !NetworkService.wifiEnabled || NetworkService.scanning
                       ? HoloniightPalette.textSecondary
                       : rescanHover.hovered ? HoloniightPalette.accentCyan : HoloniightPalette.textSecondary
            strokeWidth: 1.8
          }

          Text {
            objectName: "rescanLabel"

            text: NetworkService.scanning ? qsTr("Scanning") : qsTr("Rescan")
            color: !NetworkService.available || !NetworkService.wifiEnabled || NetworkService.scanning
                   ? HoloniightPalette.textSecondary
                   : rescanHover.hovered ? HoloniightPalette.accentCyan : HoloniightPalette.textSecondary
            font.pointSize: 9
            font.weight: Font.Medium
          }
        }

        MouseArea {
          anchors.fill: parent
          enabled: NetworkService.available && NetworkService.wifiEnabled && !NetworkService.scanning
          cursorShape: Qt.PointingHandCursor
          onClicked: NetworkService.rescanWifi()
        }

        HoverHandler { id: rescanHover }
      }
    }

    WifiNetworkList {
      Layout.fillWidth: true
      Layout.fillHeight: true
      Layout.minimumHeight: 128
      Layout.preferredHeight: 320
      Layout.maximumHeight: 320
      onPasswordRequested: (row, ssid) => {
        passwordDialog.row = row
        passwordDialog.ssid = ssid
        passwordDialog.open()
      }
    }

    HnSeparator {
      thickness: root.sectionSeparatorThickness
      centerOpacity: root.sectionSeparatorOpacity
      fadeMode: HnSeparator.Solid
      Layout.fillWidth: true
      Layout.minimumHeight: root.sectionSeparatorThickness
      Layout.preferredHeight: root.sectionSeparatorThickness
    }

    Text {
      Layout.fillWidth: true
      text: qsTr("ACTIONS")
      color: HoloniightPalette.accentBlue
      font.pointSize: 9
      font.weight: Font.DemiBold
      elide: Text.ElideRight
    }

    NetworkActionRow {
      Layout.fillWidth: true
      Layout.preferredHeight: 134
      onSettingsRequested: NetworkService.openNetworkSettings()
      onInfoRequested: root.infoOpen = !root.infoOpen
    }

    Text {
      Layout.fillWidth: true
      Layout.preferredHeight: root.infoOpen ? implicitHeight : 0
      visible: root.infoOpen
      text: qsTr("Connection: %1\nIPv4: %2")
            .arg(NetworkService.activeConnectionName.length > 0 ? NetworkService.activeConnectionName : qsTr("Unknown"))
            .arg(NetworkService.activeIp4Address.length > 0 ? NetworkService.activeIp4Address : qsTr("Unavailable"))
      color: HoloniightPalette.textSecondary
      font.pointSize: 9
      elide: Text.ElideRight
    }
  }

  property bool infoOpen: false
}

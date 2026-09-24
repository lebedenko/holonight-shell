import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls

import HolonightShell

Item {
  id: root

  readonly property real sectionSeparatorOpacity: 0.48

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
      opacity: root.sectionSeparatorOpacity
      fadeMode: HnSeparator.Solid
      Layout.fillWidth: true
    }

    NetworkCurrentCard {}

    HnLabel {
      Layout.fillWidth: true
      visible: NetworkService.lastError.length > 0
      rawText: NetworkService.lastError
      role: HnTypographyRole.Caption
      color: NetworkService.lastError === qsTr("Password required") ? HoloniightPalette.textSecondary : HoloniightPalette.error
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

      HnLabel {
        Layout.fillWidth: true
        rawText: qsTr("WI-FI NETWORKS")
        role: HnTypographyRole.MicroHeader
        color: HoloniightPalette.accentBlue
        font.family: AppearanceService.uiFont
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

          HnLabel {
            objectName: "rescanLabel"

            rawText: NetworkService.scanning ? qsTr("Scanning") : qsTr("Rescan")
            role: HnTypographyRole.Caption
            color: !NetworkService.available || !NetworkService.wifiEnabled || NetworkService.scanning
                   ? HoloniightPalette.textSecondary
                   : rescanHover.hovered ? HoloniightPalette.accentCyan : HoloniightPalette.textSecondary
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
      opacity: root.sectionSeparatorOpacity
      fadeMode: HnSeparator.Solid
      Layout.fillWidth: true
    }

    HnLabel {
      Layout.fillWidth: true
      rawText: qsTr("ACTIONS")
      role: HnTypographyRole.MicroHeader
      color: HoloniightPalette.accentBlue
      font.family: AppearanceService.uiFont
      elide: Text.ElideRight
    }

    NetworkActionRow {
      Layout.fillWidth: true
      Layout.preferredHeight: 134
      onSettingsRequested: NetworkService.openNetworkSettings()
      onInfoRequested: root.infoOpen = !root.infoOpen
    }

    HnLabel {
      Layout.fillWidth: true
      Layout.preferredHeight: root.infoOpen ? implicitHeight : 0
      visible: root.infoOpen
      rawText: qsTr("Connection: %1\nIPv4: %2")
            .arg(NetworkService.activeConnectionName.length > 0 ? NetworkService.activeConnectionName : qsTr("Unknown"))
            .arg(NetworkService.activeIp4Address.length > 0 ? NetworkService.activeIp4Address : qsTr("Unavailable"))
      role: HnTypographyRole.Caption
      color: HoloniightPalette.textSecondary
      elide: Text.ElideRight
    }
  }

  property bool infoOpen: false
}

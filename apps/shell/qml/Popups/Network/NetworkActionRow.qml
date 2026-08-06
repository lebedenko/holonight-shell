import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls

import HolonightShell

ColumnLayout {
  id: root

  signal settingsRequested()
  signal infoRequested()

  spacing: 6

  HnActionDelegate {
    objectName: "networkSettingsAction"

    Layout.fillWidth: true
    Layout.fillHeight: true
    sizeRole: HnControlSize.Large
    title: qsTr("Network Settings")
    description: qsTr("Open NetworkManager settings")
    iconSource: "qrc:/HolonightShell/common/network-settings.svg"
    onClicked: root.settingsRequested()
  }

  HnActionDelegate {
    objectName: "networkInfoAction"

    Layout.fillWidth: true
    Layout.fillHeight: true
    sizeRole: HnControlSize.Large
    title: qsTr("Connection Information")
    description: qsTr("Details about this connection")
    iconSource: "qrc:/HolonightShell/common/network-info.svg"
    enabled: NetworkService.activeConnectionName.length > 0
             || NetworkService.activeIp4Address.length > 0
    onClicked: root.infoRequested()
  }
}

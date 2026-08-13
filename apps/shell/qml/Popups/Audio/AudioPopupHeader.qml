import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls
import Holonight.Components

import HolonightShell

// Popup header row: speaker icon + semantic title on the left and page-aware settings navigation
// on the right. holonight-settings owns validation of the page key.
Item {
  id: root

  property Item nextTabItem: null
  property alias settingsButtonItem: settingsButton

  implicitHeight: 40

  RowLayout {
    anchors.fill: parent
    spacing: 10

    ExternalIcon {
      objectName: "headerSpeakerIcon"

      iconName: "audio-volume-high"
      iconSize: 22
      tintColor: HoloniightPalette.textPrimary
      Layout.preferredWidth: 22
      Layout.preferredHeight: 22
    }

    HnLabel {
      rawText: qsTr("AUDIO")
      role: HnTypographyRole.MicroHeader
    }

    Item { Layout.fillWidth: true }

    HnIconButton {
      id: settingsButton
      objectName: "headerSettingsGear"

      icon.source: "qrc:/HolonightShell/common/network-settings.svg"
      icon.color: HoloniightPalette.textSecondary
      Accessible.name: qsTr("Open Audio settings")
      activeFocusOnTab: true
      KeyNavigation.tab: root.nextTabItem
      KeyNavigation.priority: KeyNavigation.BeforeItem
      Layout.preferredWidth: implicitWidth
      Layout.preferredHeight: implicitHeight
      onClicked: {
        SettingsNavigationService.openPage("audio")
        StatusPopupSurface.hide()
      }
    }
  }
}

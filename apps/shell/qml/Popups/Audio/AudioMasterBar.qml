import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Components

import HolonightShell

// Master volume strip pinned to the bottom of the popup. Controls the current default output
// sink via AudioService.setVolume. Stays in sync automatically: AudioService.volume shadows
// the default sink and updates whenever the default changes or its volume echoes back from PA.
Item {
  id: root

  implicitHeight: 56

  readonly property int masterVolume: AudioService.volume

  Rectangle {
    anchors.fill: parent
    color: "transparent"
    Rectangle {
      anchors.top: parent.top
      anchors.left: parent.left
      anchors.right: parent.right
      height: 1
      color: HoloniightPalette.borderPassive
    }
  }

  RowLayout {
    anchors.fill: parent
    anchors.leftMargin: 16
    anchors.rightMargin: 16
    spacing: 16

    ExternalIcon {
      iconName: AudioService.muted ? "audio-volume-muted" : "audio-volume-high"
      iconSize: 22
      tintColor: AudioService.muted ? HoloniightPalette.textMuted : HoloniightPalette.accentCyan
      Layout.preferredWidth: 22
      Layout.preferredHeight: 22
    }

    Text {
      text: qsTr("Master Volume")
      color: HoloniightPalette.textPrimary
      font.pointSize: 10.5
      font.bold: true
    }

    AudioVolumeSlider {
      Layout.fillWidth: true
      Layout.alignment: Qt.AlignVCenter
      value: root.masterVolume
      accentColor: HoloniightPalette.accentCyan
      onValueChanging: (value) => AudioService.setVolume(value)
      onValueCommitted: (value) => AudioService.setVolume(value)
    }

    Text {
      Layout.preferredWidth: 44
      horizontalAlignment: Text.AlignRight
      text: root.masterVolume + "%"
      color: HoloniightPalette.textMuted
      font.pointSize: 10.5
    }
  }
}

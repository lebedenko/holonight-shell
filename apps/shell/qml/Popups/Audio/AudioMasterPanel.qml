import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Holonight.Core

import HolonightShell

// Master volume hero panel (REQ-F-2001/2002): glowing circular icon badge on the left; "MASTER
// VOLUME" label + large percentage readout above a horizontal slider; a separate small square
// mute button on the far right. Slider drives AudioService.setVolume; mute button drives the new
// AudioService.setDefaultOutputMuted invokable, with immediate icon feedback once mutedChanged
// fires.
Item {
  id: root

  implicitHeight: 88

  readonly property int masterVolume: AudioService.volume
  readonly property bool masterMuted: AudioService.muted

  RowLayout {
    anchors.fill: parent
    spacing: 16

    Item {
      id: badge

      Layout.preferredWidth: 64
      Layout.preferredHeight: 64
      Layout.alignment: Qt.AlignVCenter

      MultiEffect {
        source: badgeCircle
        anchors.fill: badgeCircle
        shadowEnabled: true
        shadowColor: HoloniightPalette.accentCyan
        shadowBlur: 0.7
        shadowOpacity: 0.5
        shadowScale: 1.15
        autoPaddingEnabled: true
      }

      Rectangle {
        id: badgeCircle
        objectName: "masterVolumeBadge"

        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.color: HoloniightPalette.accentCyan
        border.width: 1.5

        AudioTintedIcon {
          anchors.centerIn: parent
          iconName: root.masterMuted ? "audio-volume-muted" : "audio-volume-high"
          iconSize: 28
          tintColor: HoloniightPalette.accentCyan
        }
      }
    }

    ColumnLayout {
      Layout.fillWidth: true
      Layout.alignment: Qt.AlignVCenter
      spacing: 6

      Text {
        text: qsTr("MASTER VOLUME")
        color: HoloniightPalette.accentBlue
        font.pointSize: 9
        font.bold: true
        font.capitalization: Font.AllUppercase
      }

      RowLayout {
        Layout.fillWidth: true
        spacing: 16

        Text {
          objectName: "masterVolumePercentageText"

          Layout.preferredWidth: 64
          text: root.masterVolume + "%"
          color: HoloniightPalette.textPrimary
          font.pointSize: 19
        }

        AudioVolumeSlider {
          objectName: "masterVolumeSlider"

          Layout.fillWidth: true
          Layout.alignment: Qt.AlignVCenter
          value: root.masterVolume
          accentColor: HoloniightPalette.accentCyan
          onValueChanging: (value) => AudioService.setVolume(value)
          onValueCommitted: (value) => AudioService.setVolume(value)
        }
      }
    }

    Rectangle {
      objectName: "masterMuteButton"

      Layout.preferredWidth: 40
      Layout.preferredHeight: 40
      Layout.alignment: Qt.AlignVCenter
      radius: 8
      color: "transparent"
      border.color: "transparent"

      AudioTintedIcon {
        anchors.centerIn: parent
        iconName: root.masterMuted ? "audio-volume-muted" : "audio-volume-high"
        iconSize: 20
        tintColor: root.masterMuted ? HoloniightPalette.textMuted : HoloniightPalette.textPrimary
      }

      MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: AudioService.setDefaultOutputMuted(!AudioService.muted)
      }
    }
  }
}

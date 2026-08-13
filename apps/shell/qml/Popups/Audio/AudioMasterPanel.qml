import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Holonight.Core
import Holonight.Controls
import Holonight.Components

import HolonightShell

// Master volume hero panel (REQ-F-2001/2002): glowing circular icon badge on the left; "MASTER
// VOLUME" label + large percentage readout above a horizontal slider; a separate small square
// mute button on the far right. Slider drives AudioService.setVolume; mute button drives the new
// AudioService.setDefaultOutputMuted invokable, with immediate icon feedback once mutedChanged
// fires.
Item {
  id: root

  implicitHeight: 72

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
      spacing: 4

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

          Layout.preferredWidth: 48
          text: root.masterVolume + "%"
          color: HoloniightPalette.textPrimary
          font.pointSize: 14
        }

        AudioVolumeSlider {
          objectName: "masterVolumeSlider"

          Layout.fillWidth: true
          Layout.alignment: Qt.AlignVCenter
          value: root.masterVolume
          accentColor: HoloniightPalette.accentCyan
          muted: root.masterMuted
          accessibleName: qsTr("Master volume")
          onValueChanging: (value) => AudioService.setVolume(value)
          onValueCommitted: (value) => AudioService.setVolume(value)
          onMuteRequested: AudioService.setDefaultOutputMuted(!root.masterMuted)
        }
      }
    }

    HnIconButton {
      id: muteButton
      objectName: "masterMuteButton"

      Layout.preferredWidth: 44
      Layout.preferredHeight: 44
      Layout.alignment: Qt.AlignVCenter
      sizeRole: HnControlSize.Large
      icon.source: root.masterMuted ? "image://icon/audio-volume-muted" : "image://icon/audio-volume-high"
      activeFocusOnTab: true
      Accessible.name: root.masterMuted ? qsTr("Unmute master output") : qsTr("Mute master output")
      onClicked: AudioService.setDefaultOutputMuted(!root.masterMuted)

      contentItem: Item {
        ExternalIcon {
          objectName: "masterMuteButtonIcon"
          anchors.centerIn: parent
          width: 22
          height: 22
          iconName: root.masterMuted ? "audio-volume-muted" : "audio-volume-high"
          iconSize: 22
          tintColor: root.masterMuted ? HoloniightPalette.textMuted : HoloniightPalette.textPrimary
        }
      }

      background: Rectangle {
        color: muteButton.down ? HoloniightPalette.surface
                               : (muteButton.hovered ? HoloniightPalette.surfaceHover : "transparent")
        radius: 6
        border.color: muteButton.visualFocus ? HoloniightPalette.borderFocus : HoloniightPalette.borderPassive
        border.width: muteButton.visualFocus ? HnMetrics.focusBorderWidth : 1
      }
    }
  }
}

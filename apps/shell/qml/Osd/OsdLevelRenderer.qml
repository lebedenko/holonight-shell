import QtQuick
import QtQuick.Layouts
import Holonight.Core

import HolonightShell

// Level-kind OSD content: icon, channel label, animated progress bar and a value readout.
//
// Pure content. The card background, glow and entrance/exit animation belong to OsdView.qml,
// which centers this item inside the layer surface — so the two renderers stay focused on what
// differs between them and the surface look has a single definition.
//
// REQ-F-023: when muted the bar stays at the real volume position and is only dimmed; it never
// collapses to 0. Only the readout text changes.
Item {
  id: root

  required property string channel
  required property int value
  required property bool muted

  // REQ-F-020: exactly these four audio names, no intermediate tiers. REQ-F-021 routes brightness
  // to UtilityIcon's glyph. Muted overrides the tier, but only ever for audio — brightness has no
  // mute state, and BrightnessChannelSource always reports muted=false.
  readonly property string iconName: {
    if (root.channel === "screen-brightness")
      return "brightness";
    if (root.muted)
      return "audio-volume-muted";
    if (root.value >= 67)
      return "audio-volume-high";
    if (root.value >= 34)
      return "audio-volume-medium";
    return "audio-volume-low";
  }

  readonly property string channelLabel: root.channel === "screen-brightness" ? qsTr("Brightness") : qsTr("Volume")
  readonly property string valueText: root.muted ? qsTr("Muted") : root.value + "%"
  readonly property color resolvedFillColor: root.muted ? HoloniightPalette.textDisabled : HoloniightPalette.accentCyan

  readonly property int contentPadding: 20

  implicitWidth: content.implicitWidth + root.contentPadding * 2
  implicitHeight: content.implicitHeight + root.contentPadding * 2

  // REQ-F-017/NF-003: a same-channel update slides the bar to its new position rather than
  // replaying the entrance animation.
  Behavior on value {
    NumberAnimation {
      duration: 100
      easing.type: Easing.OutQuad
    }
  }

  RowLayout {
    id: content

    anchors.centerIn: parent
    spacing: 14

    BarIcon {
      objectName: "levelIcon"
      name: root.iconName
      Layout.preferredWidth: 26
      Layout.preferredHeight: 26
    }

    ColumnLayout {
      spacing: 7

      RowLayout {
        spacing: 12

        Layout.fillWidth: true

        Text {
          objectName: "channelLabel"
          text: root.channelLabel
          textFormat: Text.PlainText
          color: HoloniightPalette.textMuted
          font.pixelSize: 13
        }

        Item {
          Layout.fillWidth: true
        }

        Text {
          objectName: "valueLabel"
          text: root.valueText
          textFormat: Text.PlainText
          color: root.muted ? HoloniightPalette.textDisabled : HoloniightPalette.textPrimary
          font.pixelSize: 13
          font.bold: true
        }
      }

      Rectangle {
        objectName: "barTrack"

        radius: height / 2
        color: HoloniightPalette.surfaceRaised

        Layout.preferredWidth: 196
        Layout.preferredHeight: 6

        Rectangle {
          objectName: "barFill"

          anchors.left: parent.left
          anchors.verticalCenter: parent.verticalCenter
          width: parent.width * Math.max(0, Math.min(100, root.value)) / 100.0
          height: parent.height
          radius: parent.radius
          color: root.resolvedFillColor
        }
      }
    }
  }
}

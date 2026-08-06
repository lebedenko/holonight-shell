import QtQuick
import QtQuick.Layouts
import Holonight.Core

import HolonightShell

// Selection-kind OSD content: icon, a large short label and a smaller full label. No progress bar --
// a keyboard layout has no magnitude to show (REQ-F-015).
//
// Pure content, like OsdLevelRenderer: the card background, glow and entrance/exit animation belong
// to OsdView.qml, which centers this item inside the layer surface.
//
// REQ-F-017: an update while the OSD is already visible must not replay the entrance. Each label
// instead fades out, swaps its text and fades back in, so the surface itself never moves.
Item {
  id: root

  required property string shortLabel
  required property string fullLabel

  // What is on screen right now. Lags `shortLabel`/`fullLabel` by one fade-out so the text swap
  // happens while the label is invisible.
  property string displayShortLabel: ""
  property string displayFullLabel: ""

  // Guards the change handlers against firing during construction, when the required properties are
  // first assigned and there is nothing to fade from yet.
  property bool ready: false

  // REQ-F-015: the short label carries the glance value, so it is the large one. Both are declared
  // in points, in the same unit, so "full label is smaller" is a direct comparison.
  readonly property int shortLabelPointSize: 32
  readonly property int fullLabelPointSize: 13

  readonly property int contentPadding: 20
  readonly property int labelWidth: 196
  readonly property int fadeDuration: 90

  implicitWidth: content.implicitWidth + root.contentPadding * 2
  implicitHeight: content.implicitHeight + root.contentPadding * 2

  Component.onCompleted: {
    root.displayShortLabel = root.shortLabel;
    root.displayFullLabel = root.fullLabel;
    root.ready = true;
  }

  onShortLabelChanged: {
    if (root.ready && root.displayShortLabel !== root.shortLabel)
      shortText.opacity = 0;
  }

  onFullLabelChanged: {
    if (root.ready && root.displayFullLabel !== root.fullLabel)
      fullText.opacity = 0;
  }

  RowLayout {
    id: content

    anchors.centerIn: parent
    spacing: 14

    BarIcon {
      objectName: "selectionIcon"
      name: "keyboard"
      Layout.preferredWidth: 32
      Layout.preferredHeight: 32
    }

    ColumnLayout {
      spacing: 2

      Layout.preferredWidth: root.labelWidth

      Text {
        id: shortText

        objectName: "shortLabelText"
        text: root.displayShortLabel
        textFormat: Text.PlainText
        elide: Text.ElideRight
        color: HoloniightPalette.textPrimary
        font.pointSize: root.shortLabelPointSize
        font.bold: true
        opacity: 1

        Layout.fillWidth: true

        Behavior on opacity {
          NumberAnimation {
            id: shortFade

            duration: root.fadeDuration
            easing.type: Easing.OutQuad
            // Swap the text at the bottom of the fade, then run the same Behavior back up. Reading
            // root.shortLabel here (not a captured value) means a second change arriving mid-fade
            // still lands on the newest label.
            onRunningChanged: {
              if (shortFade.running || shortText.opacity > 0)
                return;
              root.displayShortLabel = root.shortLabel;
              shortText.opacity = 1;
            }
          }
        }
      }

      Text {
        id: fullText

        objectName: "fullLabelText"
        text: root.displayFullLabel
        textFormat: Text.PlainText
        elide: Text.ElideRight
        color: HoloniightPalette.textMuted
        font.pointSize: root.fullLabelPointSize
        opacity: 1

        Layout.fillWidth: true

        Behavior on opacity {
          NumberAnimation {
            id: fullFade

            duration: root.fadeDuration
            easing.type: Easing.OutQuad
            onRunningChanged: {
              if (fullFade.running || fullText.opacity > 0)
                return;
              root.displayFullLabel = root.fullLabel;
              fullText.opacity = 1;
            }
          }
        }
      }
    }
  }
}

import QtQuick
import Holonight.Core

import HolonightShell

// Live microphone level meter (REQ-F-8002/8003): a row of small vertical bars. Each bar occupies
// a 100/barCount %-wide band of the 0-100 level range. The popup root exclusively owns the
// monitoring lifecycle; this component is presentation-only.
Item {
  id: root

  property int level: AudioService.inputLevel
  property color accentColor: HoloniightPalette.accentCyan
  readonly property int barCount: 10
  readonly property real barSpacing: 3
  readonly property real barWidth: Math.max(2, (root.width - (root.barCount - 1) * root.barSpacing) / root.barCount)

  implicitWidth: 100
  implicitHeight: 18

  Row {
    anchors.fill: parent
    spacing: root.barSpacing
    layoutDirection: Qt.LeftToRight

    Repeater {
      model: root.barCount

      delegate: Item {
        id: barSlot

        required property int index

        readonly property bool lit: root.level >= (barSlot.index + 1) * (100 / root.barCount)

        width: root.barWidth
        height: root.height

        Rectangle {
          objectName: "inputLevelBar"

          anchors.bottom: parent.bottom
          width: parent.width
          height: parent.height
          radius: width / 2
          color: barSlot.lit ? root.accentColor : HoloniightPalette.borderPassive
        }
      }
    }
  }
}

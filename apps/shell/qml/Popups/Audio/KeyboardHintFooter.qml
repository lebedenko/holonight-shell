import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls

import HolonightShell

// The footer mirrors the commands supported by the currently focused control.
Item {
  id: root

  property real separatorBleed: 0
  property var focusItem: null
  readonly property string focusContext: root.focusItem && root.focusItem.keyboardHintType
      ? root.focusItem.keyboardHintType : "other"

  implicitHeight: 48

  Rectangle {
    anchors.fill: parent
    color: HoloniightPalette.surfaceRaised

    HnSeparator {
      objectName: "audioFooterSeparator"
      anchors.top: parent.top
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.leftMargin: -root.separatorBleed
      anchors.rightMargin: -root.separatorBleed
      fadeMode: HnSeparator.Solid
    }
  }

  RowLayout {
    anchors.centerIn: parent
    spacing: 16

    RowLayout {
      objectName: "navigateHint"
      visible: root.focusContext === "deviceRow"
      spacing: 6
      HnKeyHint { objectName: "navigationKeyHint"; keyGroups: [[Qt.Key_Up], [Qt.Key_Down]] }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: qsTr("Navigate"); color: HoloniightPalette.textMuted }
    }

    RowLayout {
      objectName: "selectHint"
      visible: root.focusContext === "deviceRow"
      spacing: 6
      HnKeyHint { objectName: "enterKeyHint"; keyGroups: [[Qt.Key_Return]] }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: qsTr("Select"); color: HoloniightPalette.textMuted }
    }

    RowLayout {
      objectName: "adjustHint"
      visible: root.focusContext === "slider"
      spacing: 6
      HnKeyHint { keyGroups: [[Qt.Key_Left], [Qt.Key_Right]] }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: qsTr("Adjust"); color: HoloniightPalette.textMuted }
    }

    RowLayout {
      objectName: "minMaxHint"
      visible: root.focusContext === "slider"
      spacing: 6
      HnKeyHint { keyGroups: [[Qt.Key_Home], [Qt.Key_End]] }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: qsTr("Min/Max"); color: HoloniightPalette.textMuted }
    }

    RowLayout {
      objectName: "expandHint"
      visible: root.focusContext === "summaryRow"
      spacing: 6
      HnKeyHint { keyGroups: [[Qt.Key_Return]] }
      HnLabel {
        Layout.alignment: Qt.AlignVCenter
        role: HnTypographyRole.Caption
        rawText: root.focusItem && root.focusItem.expanded ? qsTr("Collapse") : qsTr("Expand")
        color: HoloniightPalette.textMuted
      }
    }

    RowLayout {
      objectName: "muteHint"
      visible: root.focusContext !== "other"
      spacing: 6
      HnKeyHint { objectName: "muteKeyHint"; keyGroups: [[Qt.Key_M]] }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: qsTr("Mute"); color: HoloniightPalette.textMuted }
    }

    RowLayout {
      objectName: "tabFocusHint"
      visible: root.focusContext === "other"
      spacing: 6
      HnKeyHint { objectName: "tabKeyHint"; keyGroups: [[Qt.Key_Tab]] }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: qsTr("Focus"); color: HoloniightPalette.textMuted }
    }
  }
}

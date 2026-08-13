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

  component FooterKeyHint: HnKeyHint {
    id: keyHint

    property bool navigationKey: false

    Layout.alignment: Qt.AlignVCenter
    topPadding: 3
    bottomPadding: 3
    leftPadding: navigationKey ? 10 : 9
    rightPadding: navigationKey ? 10 : 9
    font.pointSize: HolonightTheme.monospaceFontSize + (navigationKey ? 1 : 0)
    font.bold: navigationKey

    background: Rectangle {
      color: HoloniightPalette.surfaceRaised
      radius: HnAppearance.roundedRadius(HnSurfaceRole.Control, width, height, HnAppearance.revision)
      border.width: HnMetrics.borderWidth
      border.color: HoloniightPalette.borderPassive
    }
  }

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
      thickness: HnMetrics.separatorWidth
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
      FooterKeyHint { objectName: "navigationKeyHint"; text: "↑ ↓"; navigationKey: true }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: qsTr("Navigate"); color: HoloniightPalette.textMuted }
    }

    RowLayout {
      objectName: "selectHint"
      visible: root.focusContext === "deviceRow"
      spacing: 6
      FooterKeyHint { objectName: "enterKeyHint"; text: "Enter" }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: qsTr("Select"); color: HoloniightPalette.textMuted }
    }

    RowLayout {
      objectName: "adjustHint"
      visible: root.focusContext === "slider"
      spacing: 6
      FooterKeyHint { text: "← →"; navigationKey: true }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: qsTr("Adjust"); color: HoloniightPalette.textMuted }
    }

    RowLayout {
      objectName: "minMaxHint"
      visible: root.focusContext === "slider"
      spacing: 6
      FooterKeyHint { text: "Home End" }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: qsTr("Min/Max"); color: HoloniightPalette.textMuted }
    }

    RowLayout {
      objectName: "expandHint"
      visible: root.focusContext === "summaryRow"
      spacing: 6
      FooterKeyHint { text: "Enter" }
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
      FooterKeyHint { objectName: "muteKeyHint"; text: "M" }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: qsTr("Mute"); color: HoloniightPalette.textMuted }
    }

    RowLayout {
      objectName: "tabFocusHint"
      visible: root.focusContext === "other"
      spacing: 6
      FooterKeyHint { objectName: "tabKeyHint"; text: "Tab" }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: qsTr("Focus"); color: HoloniightPalette.textMuted }
    }
  }
}

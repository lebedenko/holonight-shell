import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls

import HolonightShell

// Static keyboard-hint footer (REQ-F-10001/10002): three hint chips — "Tab + ↔ Navigate",
// "Enter Select", "M Mute". Purely visual; no Keys.onPressed or shortcut handlers are attached
// here or anywhere else as part of this redesign — existing Tab focus traversal is unmodified.
Item {
  id: root

  property real separatorBleed: 0

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
    spacing: 24

    RowLayout {
      objectName: "navigateHint"
      spacing: 6
      FooterKeyHint { objectName: "tabKeyHint"; text: "Tab" }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: "+"; color: HoloniightPalette.textMuted }
      FooterKeyHint { objectName: "navigationKeyHint"; text: "← →"; navigationKey: true }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: qsTr("Navigate"); color: HoloniightPalette.textMuted }
    }

    RowLayout {
      objectName: "selectHint"
      spacing: 6
      FooterKeyHint { objectName: "enterKeyHint"; text: "Enter" }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: qsTr("Select"); color: HoloniightPalette.textMuted }
    }

    RowLayout {
      objectName: "muteHint"
      spacing: 6
      FooterKeyHint { objectName: "muteKeyHint"; text: "M" }
      HnLabel { Layout.alignment: Qt.AlignVCenter; role: HnTypographyRole.Caption; rawText: qsTr("Mute"); color: HoloniightPalette.textMuted }
    }
  }
}

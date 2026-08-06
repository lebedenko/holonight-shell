import QtQuick
import Holonight.Core
import Holonight.Controls

import HolonightShell

// Scrollable list of application stream rows (Applications tab).
ListView {
  id: root

  property string emptyText: qsTr("No applications are playing audio")

  clip: true
  spacing: 2
  boundsBehavior: Flickable.StopAtBounds

  delegate: AudioStreamDelegate {}

  HnEmptyState {
    objectName: "audioStreamEmptyState"

    parent: root
    anchors.centerIn: parent
    width: parent.width
    visible: root.count === 0
    titleText: root.emptyText
  }

  // Minimal scroll indicator (no QtQuick.Controls dependency).
  Rectangle {
    visible: root.contentHeight > root.height
    width: 4
    radius: 2
    color: HoloniightPalette.borderPassive
    anchors.right: parent.right
    anchors.rightMargin: 2
    height: Math.max(24, root.height * root.visibleArea.heightRatio)
    y: root.visibleArea.yPosition * root.height
  }
}

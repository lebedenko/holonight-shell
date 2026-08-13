import QtQuick
import Holonight.Core
import Holonight.Controls

import HolonightShell

// Scrollable list of device rows. Used for both the Output (cyan) and Input (violet) tabs;
// the accent and the set-default target are chosen by the caller via accentColor / isInput.
ListView {
  id: root

  required property color accentColor
  required property bool isInput
  property string emptyText: qsTr("No devices found")

  clip: true
  spacing: 2
  boundsBehavior: Flickable.StopAtBounds
  currentIndex: -1

  function focusRelativeTo(item: Item, delta: int): void {
    const index = root.indexAt(1, item.y + item.height / 2);
    const target = root.itemAtIndex(Math.max(0, Math.min(root.count - 1, index + delta)));
    if (target)
      target.forceActiveFocus(Qt.TabFocusReason);
  }

  delegate: AudioDeviceDelegate {
    accentColor: root.accentColor
    isInputDevice: root.isInput
    navigationList: root
  }

  HnEmptyState {
    objectName: "audioDeviceEmptyState"

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

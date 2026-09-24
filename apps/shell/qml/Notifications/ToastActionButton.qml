import QtQuick

import HolonightShell
import Holonight.Core
import Holonight.Controls

// Outlined HUD-style action button: transparent fill, accent-colored border (HoloNight design
// calls for outlined, not filled, buttons). A tap invokes the notification action over D-Bus.
Item {
  id: root

  required property int notifId
  required property color accentColor
  required property string actionKey
  required property string label

  implicitWidth: Math.max(92, labelText.implicitWidth + 28)
  implicitHeight: Math.max(28, labelText.implicitHeight + 10)

  Rectangle {
    anchors.fill: parent
    radius: 5
    color: hover.hovered
      ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.16)
      : "transparent"
    border.color: root.accentColor
    border.width: 1
    opacity: hover.hovered ? 1.0 : 0.78

    Behavior on opacity {
      NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }

    HnLabel {
      id: labelText
      anchors {
        left: parent.left
        right: parent.right
        verticalCenter: parent.verticalCenter
        leftMargin: 11
        rightMargin: 11
      }
      rawText: root.label
      role: HnTypographyRole.Caption
      color: root.accentColor
      elide: Text.ElideRight
      horizontalAlignment: Text.AlignHCenter
      font.weight: Font.Medium
    }
  }

  HoverHandler {
    id: hover
  }

  TapHandler {
    onTapped: NotificationService.invokeAction(root.notifId, root.actionKey)
  }
}

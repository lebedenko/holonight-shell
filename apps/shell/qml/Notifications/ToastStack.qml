pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell
import "../Utility" as Utility

// Root item loaded by NotificationToastSurface for a single monitor. Lays out this monitor's
// visible toasts (filtered by NotificationService) top-down, right-aligned, with smooth reflow
// when toasts are added or removed. Each ToastItem owns its own fade + slide-down entry.
Item {
  id: root

  required property string monitorName

  Utility.AppearanceReloadBridge {}

  readonly property int toastWidth: 430
  readonly property int toastSpacing: 10
  // Transparent margin around the toast column so the accent glow is not clipped at the
  // surface edge. NotificationToastSurface sizes the layer surface to contentHeight, so this
  // margin is the only non-toast region that can capture input — kept small on purpose.
  readonly property int glowMargin: 16

  // Driven up to NotificationToastSurface (via the changed signal) to resize the layer surface
  // to exactly the visible toasts plus glow margin, so empty screen area stays click-through.
  readonly property int contentHeight: column.implicitHeight + (root.glowMargin * 2)

  implicitWidth: root.toastWidth + (root.glowMargin * 2)
  implicitHeight: root.contentHeight

  Rectangle {
    anchors.fill: parent
    color: "transparent"
    border.color: "red"
    border.width: 1
    visible: AppearanceService.debugOverlays
    z: 99999
  }

  Column {
    id: column
    anchors.right: parent.right
    anchors.top: parent.top
    anchors.rightMargin: root.glowMargin
    anchors.topMargin: root.glowMargin
    width: root.toastWidth
    spacing: root.toastSpacing

    // Smooth upward/downward reflow when a toast above is removed or a new one pushes the stack.
    move: Transition {
      NumberAnimation { properties: "y"; duration: 160; easing.type: Easing.OutCubic }
    }

    Repeater {
      model: NotificationService.visibleModelForMonitor(root.monitorName)

      delegate: ToastItem {
        width: column.width
      }
    }
  }
}

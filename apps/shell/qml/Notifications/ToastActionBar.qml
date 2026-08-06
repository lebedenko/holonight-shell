pragma ComponentBehavior: Bound
import QtQuick

// Wrapping grid of outlined action buttons for a toast: one ToastActionButton per
// displayable {key, label} entry in the notification's actions list.
Flow {
  id: root

  required property int notifId
  required property color accentColor
  required property var actions

  spacing: 8

  Repeater {
    model: root.actions
    delegate: ToastActionButton {
      required property var modelData

      width: Math.min(implicitWidth, root.width)
      notifId: root.notifId
      accentColor: root.accentColor
      actionKey: modelData.key ?? ""
      label: modelData.label ?? ""
    }
  }
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Holonight.Core

import HolonightShell

// Current-default-device summary row (REQ-F-5001/6001): icon + name + metadata subtitle +
// DEFAULT pill + chevron. Shared by both the Output and Input AudioDeviceSection instances —
// the only difference between them is the optional trailingContent slot (null for Output, an
// InputLevelMeter for Input). Clicking anywhere on the row toggles the section's expanded state
// via toggled(); the parent (AudioPopupContent, through AudioDeviceSection) owns the actual state.
Item {
  id: root

  required property string iconName
  required property string title
  required property string subtitle
  required property bool expanded
  required property color accentColor
  property Component trailingContent: null
  property string fallbackIconName: "audio-card"
  property bool showDefaultBadge: true

  signal toggled()

  implicitHeight: 64

  Rectangle {
    objectName: "currentDeviceRowFrame"

    anchors.fill: parent
    radius: 6
    color: "transparent"
    border.color: HoloniightPalette.borderPassive
    border.width: 1
  }

  MouseArea {
    anchors.fill: parent
    cursorShape: Qt.PointingHandCursor
    onClicked: root.toggled()
  }

  RowLayout {
    anchors.fill: parent
    anchors.leftMargin: 16
    anchors.rightMargin: 16
    spacing: 12

    Rectangle {
      objectName: "currentDeviceIconFrame"

      Layout.preferredWidth: 44
      Layout.preferredHeight: 44
      Layout.alignment: Qt.AlignVCenter
      radius: 6
      color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.12)
      border.color: root.accentColor
      border.width: 1

      AudioTintedIcon {
        objectName: "currentDeviceIcon"

        anchors.centerIn: parent
        iconName: root.iconName
        fallbackIconName: root.fallbackIconName
        iconSize: 24
        tintColor: root.accentColor
      }
    }

    ColumnLayout {
      Layout.fillWidth: true
      spacing: 2

      Text {
        Layout.fillWidth: true
        text: root.title
        color: HoloniightPalette.textPrimary
        font.pointSize: 11
        font.bold: true
        elide: Text.ElideRight
      }

      Text {
        Layout.fillWidth: true
        text: root.subtitle
        color: HoloniightPalette.textMuted
        font.pointSize: 8.5
        elide: Text.ElideRight
        visible: text.length > 0
      }
    }

    Loader {
      active: root.trailingContent !== null
      visible: active
      sourceComponent: root.trailingContent
      Layout.alignment: Qt.AlignVCenter
    }

    Rectangle {
      objectName: "currentDeviceDefaultPill"

      visible: root.showDefaultBadge
      Layout.preferredWidth: defaultPillText.implicitWidth + 16
      Layout.preferredHeight: 20
      Layout.alignment: Qt.AlignVCenter
      radius: 10
      color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.18)
      border.color: root.accentColor
      border.width: 1

      Text {
        id: defaultPillText
        anchors.centerIn: parent
        text: qsTr("DEFAULT")
        color: root.accentColor
        font.pointSize: 7.5
        font.bold: true
      }
    }

    NetworkPopupIcon {
      objectName: "currentDeviceChevron"

      name: "chevron"
      iconColor: HoloniightPalette.textSecondary
      rotation: root.expanded ? -90 : 90
      Layout.preferredWidth: 20
      Layout.preferredHeight: 20
      Layout.alignment: Qt.AlignVCenter

      Behavior on rotation {
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
      }
    }
  }
}

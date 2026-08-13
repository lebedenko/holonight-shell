pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Components
import Holonight.Controls

import HolonightShell

// One application stream row (sink-input / source-output): app icon, application name,
// media title subtitle, volume slider, percentage, disabled "..." placeholder. Application
// streams always use the cyan accent (no per-section accent, no default concept).
HnListDelegate {
  id: root

  required property var model

  width: ListView.view ? ListView.view.width : implicitWidth
  implicitWidth: 480
  implicitHeight: 64

  readonly property color accentColor: HoloniightPalette.accentCyan
  readonly property int volumePct: root.model.volume ?? 0
  readonly property string iconName: root.model.iconName ?? ""
  readonly property string appName: root.model.application && root.model.application.length > 0
      ? root.model.application : root.model.name

  title: root.appName
  subtitle: root.model.name && root.model.name.length > 0 && root.model.name !== root.appName ? root.model.name : ""
  leadingContentAlignment: Qt.AlignVCenter
  sizeRole: HnControlSize.Large

  leadingContent: Component {
    // Symbolic app icons follow the active palette; full-color app icons are preserved.
    ExternalIcon {
      objectName: "streamIcon"

      iconName: root.iconName
      fallbackIconName: "audio-x-generic"
      iconSize: 28
      tintColor: root.accentColor
      width: 28
      height: 28
    }
  }

  trailingContent: Component {
    RowLayout {
      spacing: 12

      AudioVolumeSlider {
        objectName: "streamVolumeSlider"

        Layout.preferredWidth: root.width < 520 ? 100 : 180
        Layout.alignment: Qt.AlignVCenter
        value: root.volumePct
        accentColor: root.accentColor
        muted: root.model.muted ?? false
        accessibleName: qsTr("%1 volume").arg(root.appName)
        onValueChanging: (value) => AudioService.setStreamVolume(root.model.streamId, value)
        onValueCommitted: (value) => AudioService.setStreamVolume(root.model.streamId, value)
        onMuteRequested: AudioService.setStreamMuted(root.model.streamId, !root.model.muted)
      }

      Text {
        objectName: "streamVolumeText"

        Layout.preferredWidth: 40
        horizontalAlignment: Text.AlignRight
        text: root.volumePct + "%"
        color: HoloniightPalette.textMuted
        font.pointSize: 9.75
      }

      Item {
        objectName: "streamMoreOptionsButton"

        visible: root.width >= 460
        Layout.preferredWidth: visible ? 28 : 0
        Layout.preferredHeight: 28
        Layout.alignment: Qt.AlignVCenter

        Text {
          anchors.centerIn: parent
          text: "⋯"
          color: HoloniightPalette.textDisabled
          font.pointSize: 14
          font.bold: true
        }
      }
    }
  }
}

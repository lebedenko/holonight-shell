pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Components
import Holonight.Controls

import HolonightShell

// One device row (output sink or input source): icon, name/description, DEFAULT badge,
// mute toggle, volume slider, percentage. Clicking the row (away from the mute button and
// slider) makes this device the default sink/source.
HnListDelegate {
  id: root

  required property var model
  required property color accentColor
  required property bool isInputDevice

  width: ListView.view ? ListView.view.width : implicitWidth
  implicitWidth: 480
  implicitHeight: 64

  readonly property bool isDefault: root.model.isDefault ?? false
  readonly property bool isMuted: root.model.muted ?? false
  readonly property int volumePct: root.model.volume ?? 0

  title: root.model.description && root.model.description.length > 0 ? root.model.description : root.model.name
  subtitle: root.model.description && root.model.description.length > 0 ? root.model.name : ""
  subtitlePresentation: HnListDelegate.SingleLine
  leadingContentAlignment: Qt.AlignVCenter
  checked: root.isDefault
  sizeRole: HnControlSize.Large

  onClicked: {
    if (root.isDefault)
      return;
    if (root.isInputDevice)
      AudioService.setDefaultInputByName(root.model.name);
    else
      AudioService.setDefaultOutputByName(root.model.name);
  }

  leadingContent: Component {
    ExternalIcon {
      objectName: "deviceIcon"

      iconName: root.isInputDevice ? "audio-input-microphone" : "audio-card"
      iconSize: 28
      tintColor: root.accentColor
      width: 28
      height: 28
    }
  }

  trailingContent: Component {
    RowLayout {
      spacing: 12

      Rectangle {
        visible: root.isDefault
        Layout.preferredWidth: badgeText.implicitWidth + 16
        Layout.preferredHeight: 20
        radius: 10
        color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.18)
        border.color: root.accentColor
        border.width: 1

        Text {
          id: badgeText
          anchors.centerIn: parent
          text: qsTr("DEFAULT")
          color: root.accentColor
          font.pixelSize: 10
          font.bold: true
        }
      }

      Item {
        objectName: "deviceMuteButton"

        Layout.preferredWidth: 28
        Layout.preferredHeight: 28

        ExternalIcon {
          anchors.centerIn: parent
          iconName: root.isMuted ? "audio-volume-muted" : "audio-volume-high"
          iconSize: 20
          tintColor: root.isMuted ? HoloniightPalette.textMuted : root.accentColor
          opacity: root.isMuted ? 0.6 : 1.0
        }
        MouseArea {
          anchors.fill: parent
          cursorShape: Qt.PointingHandCursor
          onClicked: {
            if (root.isInputDevice)
              AudioService.setInputDeviceMuted(root.model.deviceId, !root.isMuted);
            else
              AudioService.setDeviceMuted(root.model.deviceId, !root.isMuted);
          }
        }
      }

      AudioVolumeSlider {
        objectName: "deviceVolumeSlider"

        Layout.preferredWidth: 180
        Layout.alignment: Qt.AlignVCenter
        value: root.volumePct
        accentColor: root.accentColor
        onValueChanging: (value) => {
          if (root.isInputDevice)
            AudioService.setInputDeviceVolume(root.model.deviceId, value);
          else
            AudioService.setDeviceVolume(root.model.deviceId, value);
        }
        onValueCommitted: (value) => {
          if (root.isInputDevice)
            AudioService.setInputDeviceVolume(root.model.deviceId, value);
          else
            AudioService.setDeviceVolume(root.model.deviceId, value);
        }
      }

      Text {
        objectName: "deviceVolumeText"

        Layout.preferredWidth: 40
        horizontalAlignment: Text.AlignRight
        text: root.volumePct + "%"
        color: HoloniightPalette.textMuted
        font.pixelSize: 13
      }
    }
  }
}

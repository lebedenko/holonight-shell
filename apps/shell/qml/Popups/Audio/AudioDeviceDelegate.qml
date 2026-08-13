pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import Holonight.Core
import Holonight.Controls

import HolonightShell

import "AudioMetadataFormat.js" as AudioMetadataFormat

// One device row (output sink or input source): radio-checkmark, icon, name/description,
// metadata subtitle, volume slider, percentage. Clicking the row (away from the
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
  readonly property int volumePct: root.model.volume ?? 0

  title: root.model.displayName && root.model.displayName.length > 0
      ? root.model.displayName
      : (root.model.description && root.model.description.length > 0 ? root.model.description : root.model.name)
  subtitle: AudioMetadataFormat.formatDeviceMetadata(root.model.busType, root.model.channelCount,
                                                     root.model.sampleRate, root.model.codec)
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
    RowLayout {
      spacing: 8

      Rectangle {
        objectName: "deviceRadioIndicator"

        Layout.preferredWidth: 16
        Layout.preferredHeight: 16
        Layout.alignment: Qt.AlignVCenter
        radius: 8
        color: root.isDefault ? root.accentColor : "transparent"
        border.color: root.isDefault ? root.accentColor : HoloniightPalette.borderPassive
        border.width: 1.5

        Shape {
          anchors.centerIn: parent
          width: 16
          height: 16
          visible: root.isDefault
          preferredRendererType: Shape.CurveRenderer

          ShapePath {
            fillColor: "transparent"
            strokeColor: HoloniightPalette.textInverse
            strokeWidth: 1.6
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg { path: "M4 8l3 3 5-6" }
          }
        }
      }

      AudioTintedIcon {
        objectName: "deviceIcon"

        iconName: root.model.iconName ?? ""
        fallbackIconName: root.isInputDevice ? "audio-input-microphone" : "audio-card"
        iconSize: 28
        tintColor: root.isDefault ? root.accentColor : HoloniightPalette.textSecondary
        Layout.preferredWidth: 28
        Layout.preferredHeight: 28
      }
    }
  }

  trailingContent: Component {
    RowLayout {
      spacing: 12

      AudioVolumeSlider {
        objectName: "deviceVolumeSlider"

        Layout.preferredWidth: root.width < 520 ? 88 : 180
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
        color: root.isDefault ? root.accentColor : HoloniightPalette.textMuted
        font.pointSize: 9.75
      }
    }
  }
}

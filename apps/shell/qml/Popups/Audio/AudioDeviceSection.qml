pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Holonight.Core

import HolonightShell

import "AudioMetadataFormat.js" as AudioMetadataFormat

// Output/Input accordion section (REQ-F-5xxx/6xxx/7xxx), parameterized by `isInput` rather than
// duplicated into two near-identical files (§2.1 of DESIGN.md). Composes the shared
// AudioCurrentDeviceRow summary (with a live InputLevelMeter only when isInput) + a plural
// section label (visible only when expanded) + the existing AudioDeviceList, unchanged. Per
// REQ-C-15006, the device list itself never scrolls — it renders at full natural height so the
// single outer popup column is the only scroller.
ColumnLayout {
  id: root

  required property bool isInput
  required property bool expanded

  signal expandRequested()

  readonly property var model: root.isInput ? AudioService.inputs : AudioService.outputs
  readonly property color accentColor: root.isInput ? HoloniightPalette.accentViolet : HoloniightPalette.accentCyan
  readonly property string currentDeviceLabel: root.isInput ? qsTr("INPUT DEVICE") : qsTr("OUTPUT DEVICE")
  readonly property string sectionLabel: root.isInput ? qsTr("INPUT DEVICES") : qsTr("OUTPUT DEVICES")
  readonly property var defaultDeviceInfo: root.model.defaultDevice

  spacing: 8

  Component {
    id: inputMeterComponent
    RowLayout {
      spacing: 8

      InputLevelMeter {
        Layout.preferredWidth: 100
        Layout.preferredHeight: 18
      }

      HnLabel {
        objectName: "inputDeviceVolumePercentage"
        Layout.preferredWidth: 40
        horizontalAlignment: Text.AlignRight
        rawText: qsTr("%1%").arg(root.defaultDeviceInfo.volume ?? 0)
        role: HnTypographyRole.Caption
      }
    }
  }

  Text {
    objectName: "audioCurrentDeviceLabel"

    Layout.fillWidth: true
    Layout.leftMargin: 16
    text: root.currentDeviceLabel
    color: root.accentColor
    font.pointSize: 8.25
    font.bold: true
    font.capitalization: Font.AllUppercase
  }

  AudioCurrentDeviceRow {
    objectName: "audioCurrentDeviceRow"

    Layout.fillWidth: true

    iconName: root.defaultDeviceInfo.iconName ?? ""
    title: (root.defaultDeviceInfo.displayName && root.defaultDeviceInfo.displayName.length > 0)
        ? root.defaultDeviceInfo.displayName
        : ((root.defaultDeviceInfo.description && root.defaultDeviceInfo.description.length > 0)
            ? root.defaultDeviceInfo.description : (root.defaultDeviceInfo.name ?? ""))
    subtitle: AudioMetadataFormat.formatDeviceMetadata(root.defaultDeviceInfo.busType,
        root.defaultDeviceInfo.channelCount, root.defaultDeviceInfo.sampleRate, root.defaultDeviceInfo.codec)
    expanded: root.expanded
    accentColor: root.accentColor
    trailingContent: root.isInput ? inputMeterComponent : null
    fallbackIconName: root.isInput ? "audio-input-microphone" : "audio-speakers"
    showDefaultBadge: true

    onToggled: root.expandRequested()
  }

  Text {
    objectName: "audioDeviceSectionLabel"

    Layout.fillWidth: true
    Layout.leftMargin: 16
    visible: root.expanded
    text: root.sectionLabel
    color: root.accentColor
    font.pointSize: 8.25
    font.bold: true
    font.capitalization: Font.AllUppercase
  }

  AudioDeviceList {
    id: deviceList
    objectName: "audioDeviceList"

    Layout.fillWidth: true
    visible: root.expanded
    interactive: false
    implicitHeight: deviceList.count === 0 ? 72 : deviceList.contentHeight
    model: root.model
    accentColor: root.accentColor
    isInput: root.isInput
  }
}

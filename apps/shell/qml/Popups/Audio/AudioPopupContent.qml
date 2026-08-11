import QtQuick
import QtQuick.Layouts
import Holonight.Core

import HolonightShell

// Audio popup body (mini-pavucontrol). Left tab rail selects one of three sections — Output
// Devices, Input Devices, Applications — shown one at a time in the main area. A Master Volume
// strip is pinned to the bottom across all tabs. Falls back to an "unavailable" message when
// the audio service is not connected (REQ-F-032).
Item {
  id: root

  property int currentTab: 0

  readonly property var sectionTitles: [qsTr("Output Devices"), qsTr("Input Devices"), qsTr("Applications")]

  ColumnLayout {
    anchors.fill: parent
    spacing: 0

    RowLayout {
      Layout.fillWidth: true
      Layout.fillHeight: true
      spacing: 0

      AudioTabSidebar {
        Layout.preferredWidth: 180
        Layout.fillHeight: true
        currentTab: root.currentTab
        onTabSelected: (index) => { root.currentTab = index; }
      }

      Rectangle {
        Layout.preferredWidth: 1
        Layout.fillHeight: true
        color: HoloniightPalette.borderPassive
      }

      Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        // Unavailable state (REQ-F-032).
        Text {
          anchors.centerIn: parent
          visible: !AudioService.available
          text: qsTr("Audio service unavailable")
          color: HoloniightPalette.textSecondary
          font.pointSize: 12
        }

        ColumnLayout {
          anchors.fill: parent
          anchors.margins: 16
          spacing: 12
          visible: AudioService.available

          Text {
            Layout.fillWidth: true
            text: root.sectionTitles[root.currentTab]
            color: HoloniightPalette.textPrimary
            font.pointSize: 13.5
            font.bold: true
          }

          StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentTab

            AudioDeviceList {
              model: AudioService.outputs
              accentColor: HoloniightPalette.accentCyan
              isInput: false
              emptyText: qsTr("No output devices found")
            }

            AudioDeviceList {
              model: AudioService.inputs
              accentColor: HoloniightPalette.accentViolet
              isInput: true
              emptyText: qsTr("No input devices found")
            }

            AudioStreamList {
              model: AudioService.playbackStreams
              emptyText: qsTr("No applications are playing audio")
            }
          }
        }
      }
    }

    Rectangle {
      Layout.fillWidth: true
      Layout.preferredHeight: 1
      color: HoloniightPalette.borderPassive
    }

    AudioMasterBar {
      Layout.fillWidth: true
      Layout.preferredHeight: 56
    }
  }
}

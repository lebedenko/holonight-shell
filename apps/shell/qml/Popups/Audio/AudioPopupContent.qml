import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls

import HolonightShell

// Audio popup body, redesigned as a single scrollable column (REQ-F-1001/1002): header ->
// Master Volume hero -> Output accordion -> Input accordion (with live mic meter) ->
// Application Volume (4-row cap) -> keyboard-hint footer. Replaces the old tab-sidebar layout;
// AudioTabSidebar.qml is deleted (no tab-selection UI remains). Falls back to an "unavailable"
// message when the audio service is not connected.
Item {
  id: root

  // The output and input sections expand independently. Defaults keep the initial presentation
  // compact while exposing output devices immediately. StatusPopupSurface::show() destroys and
  // rebuilds this whole tree on every open, so nothing survives between opens.
  property bool outputExpanded: true
  property bool inputExpanded: false

  readonly property real sectionSeparatorThickness: 1 / Screen.devicePixelRatio
  readonly property real separatorBleed: 16

  Component.onCompleted: AudioService.startInputLevelMonitoring()
  Component.onDestruction: AudioService.stopInputLevelMonitoring()

  // Unavailable state.
  Text {
    anchors.centerIn: parent
    visible: !AudioService.available
    text: qsTr("Audio service unavailable")
    color: HoloniightPalette.textSecondary
    font.pointSize: 12
  }

  ColumnLayout {
    id: pinnedHeader
    objectName: "audioPopupPinnedHeader"

    anchors.top: parent.top
    anchors.left: parent.left
    anchors.right: parent.right
    visible: AudioService.available
    spacing: 0

    AudioPopupHeader {
      id: popupHeader
      Layout.fillWidth: true
      Layout.leftMargin: 16
      Layout.rightMargin: 16
      Layout.topMargin: 8
      Layout.bottomMargin: 16
      nextTabItem: masterPanel.volumeSlider
    }

    HnSeparator {
      objectName: "audioHeaderSeparator"
      thickness: root.sectionSeparatorThickness
      fadeMode: HnSeparator.Solid
      Layout.fillWidth: true
      Layout.leftMargin: -root.separatorBleed
      Layout.rightMargin: -root.separatorBleed
      Layout.minimumHeight: root.sectionSeparatorThickness
      Layout.preferredHeight: root.sectionSeparatorThickness
    }

    AudioMasterPanel {
      id: masterPanel
      objectName: "audioMasterPanel"
      Layout.fillWidth: true
      Layout.leftMargin: 16
      Layout.rightMargin: 16
      Layout.topMargin: 16
      Layout.bottomMargin: 16
      previousTabItem: popupHeader.settingsButtonItem
    }

    HnSeparator {
      objectName: "audioHeroSeparator"
      thickness: root.sectionSeparatorThickness
      fadeMode: HnSeparator.Solid
      Layout.fillWidth: true
      Layout.leftMargin: -root.separatorBleed
      Layout.rightMargin: -root.separatorBleed
      Layout.minimumHeight: root.sectionSeparatorThickness
      Layout.preferredHeight: root.sectionSeparatorThickness
    }
  }

  Flickable {
    id: viewport
    objectName: "audioPopupViewport"

    anchors.top: pinnedHeader.bottom
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottom: footer.top
    visible: AudioService.available
    clip: true
    contentWidth: width
    contentHeight: column.height
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick
    interactive: contentHeight > height

    ColumnLayout {
      id: column

      width: viewport.width
      spacing: 16

      AudioDeviceSection {
        objectName: "outputDeviceSection"

        Layout.fillWidth: true
        Layout.topMargin: 16
        isInput: false
        expanded: root.outputExpanded
        onExpandRequested: root.outputExpanded = !root.outputExpanded
      }

      HnSeparator {
        objectName: "audioOutputSeparator"
        thickness: root.sectionSeparatorThickness
        fadeMode: HnSeparator.Solid
        Layout.fillWidth: true
        Layout.leftMargin: -root.separatorBleed
        Layout.rightMargin: -root.separatorBleed
        Layout.minimumHeight: root.sectionSeparatorThickness
        Layout.preferredHeight: root.sectionSeparatorThickness
      }

      AudioApplicationsSection { Layout.fillWidth: true }

      HnSeparator {
        objectName: "audioApplicationsSeparator"
        thickness: root.sectionSeparatorThickness
        fadeMode: HnSeparator.Solid
        Layout.fillWidth: true
        Layout.leftMargin: -root.separatorBleed
        Layout.rightMargin: -root.separatorBleed
        Layout.minimumHeight: root.sectionSeparatorThickness
        Layout.preferredHeight: root.sectionSeparatorThickness
      }

      AudioDeviceSection {
        objectName: "inputDeviceSection"

        Layout.fillWidth: true
        isInput: true
        expanded: root.inputExpanded
        Layout.bottomMargin: 8
        onExpandRequested: root.inputExpanded = !root.inputExpanded
      }
    }
  }

  // Pinned below the scrollable viewport (REQ-F-10001's "pinned at the bottom" option, matching
  // the design mockup) rather than scrolling as the last column entry — KeyboardHintFooter draws
  // its own top divider/background so it reads correctly detached from the scroll content.
  KeyboardHintFooter {
    id: footer

    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    visible: AudioService.available
    separatorBleed: root.separatorBleed
    focusItem: root.Window.window ? root.Window.window.activeFocusItem : null
  }
}

import QtQuick
import QtQuick.Layouts
import Holonight.Core

import HolonightShell

// Application Volume section (REQ-F-9001/9002): "APPLICATION VOLUME" label + "Show All" toggle,
// wrapping the unchanged AudioStreamList in a height-capped clip container (4 rows by default).
// Toggling showAll only changes a height number — no model reset, no row destruction (§8.3 of
// DESIGN.md). `showAll` resets to false on every popup open by construction: the whole popup QML
// tree is destroyed and rebuilt each time StatusPopupSurface::show() runs (§3), so there is
// nothing to reset explicitly.
ColumnLayout {
  id: root

  property bool showAll: false
  property var model: AudioService.playbackStreams

  spacing: 8

  RowLayout {
    Layout.fillWidth: true
    spacing: 8

    HnLabel {
      Layout.fillWidth: true
      Layout.leftMargin: 16
      rawText: qsTr("APPLICATION VOLUME")
      role: HnTypographyRole.MicroHeader
      color: HoloniightPalette.accentBlue
      font.family: AppearanceService.uiFont
    }

    Item {
      objectName: "showAllToggle"

      Layout.preferredWidth: showAllRow.implicitWidth
      Layout.preferredHeight: showAllRow.implicitHeight
      Layout.rightMargin: 16
      visible: streamList.count > 4

      RowLayout {
        id: showAllRow
        spacing: 4

        HnLabel {
          rawText: qsTr("Show All")
          role: HnTypographyRole.Caption
          color: HoloniightPalette.accentBlue
        }

        NetworkPopupIcon {
          name: "chevron"
          iconColor: HoloniightPalette.accentBlue
          rotation: root.showAll ? -90 : 90
          Layout.preferredWidth: 16
          Layout.preferredHeight: 16

          Behavior on rotation {
            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
          }
        }
      }

      MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.showAll = !root.showAll
      }
    }
  }

  Item {
    id: clipContainer
    objectName: "audioApplicationsClipContainer"

    Layout.fillWidth: true
    clip: true
    readonly property real collapsedHeight: 4 * 64 + 3 * streamList.spacing
    implicitHeight: streamList.count === 0 ? 32
                                          : root.showAll ? streamList.contentHeight
                                                         : Math.min(streamList.contentHeight, collapsedHeight)

    Behavior on implicitHeight {
      NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
    }

    AudioStreamList {
      id: streamList
      objectName: "audioApplicationsStreamList"

      interactive: false
      anchors.top: parent.top
      width: parent.width
      height: streamList.contentHeight
      model: root.model
      visible: count > 0
    }

    HnLabel {
      objectName: "audioApplicationsEmptyText"
      anchors.centerIn: parent
      visible: streamList.count === 0
      rawText: qsTr("No applications are playing audio")
      role: HnTypographyRole.Caption
      color: HoloniightPalette.textMuted
    }
  }
}

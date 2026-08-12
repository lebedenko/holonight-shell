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

    Text {
      Layout.fillWidth: true
      Layout.leftMargin: 16
      text: qsTr("APPLICATION VOLUME")
      color: HoloniightPalette.accentBlue
      font.pointSize: 8.25
      font.bold: true
      font.capitalization: Font.AllUppercase
    }

    Item {
      objectName: "showAllToggle"

      Layout.preferredWidth: showAllRow.implicitWidth
      Layout.preferredHeight: showAllRow.implicitHeight
      Layout.rightMargin: 16

      RowLayout {
        id: showAllRow
        spacing: 4

        Text {
          text: qsTr("Show All")
          color: HoloniightPalette.accentBlue
          font.pointSize: 9
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
    implicitHeight: streamList.count === 0 ? 72
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
    }
  }
}

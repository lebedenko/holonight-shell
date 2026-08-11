pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls

import HolonightShell

// Left tab rail. Three mutually-exclusive tabs rendered by the shared navigation delegate.
Item {
  id: root

  property int currentTab: 0
  signal tabSelected(int index)

  implicitWidth: 180

  readonly property var tabLabels: [qsTr("Output Devices"), qsTr("Input Devices"), qsTr("Applications")]

  ColumnLayout {
    anchors.fill: parent
    anchors.topMargin: 8
    spacing: 4

    Text {
      Layout.fillWidth: true
      Layout.leftMargin: 16
      Layout.bottomMargin: 8
      text: qsTr("AUDIO")
      color: HoloniightPalette.textSecondary
      font.pointSize: 8.25
      font.bold: true
    }

    Repeater {
      id: tabRepeater
      model: root.tabLabels

      delegate: HnNavigationDelegate {
        id: tabDelegate

        required property int index
        required property string modelData

        objectName: "audioTabDelegate-" + tabDelegate.index
        Layout.fillWidth: true
        Layout.preferredHeight: 40
        title: tabDelegate.modelData
        checked: root.currentTab === tabDelegate.index
        activeFocusOnTab: true
        onClicked: root.tabSelected(tabDelegate.index)

        Keys.onTabPressed: (event) => {
          const nextIndex = (tabDelegate.index + 1) % tabRepeater.count;
          tabRepeater.itemAt(nextIndex).forceActiveFocus(Qt.TabFocusReason);
          event.accepted = true;
        }
        Keys.onBacktabPressed: (event) => {
          const previousIndex = (tabDelegate.index + tabRepeater.count - 1) % tabRepeater.count;
          tabRepeater.itemAt(previousIndex).forceActiveFocus(Qt.BacktabFocusReason);
          event.accepted = true;
        }
      }
    }

    Item {
      Layout.fillWidth: true
      Layout.fillHeight: true
    }
  }
}

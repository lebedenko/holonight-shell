import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import Holonight.Core

import HolonightShell

Item {
    id: root

    property int preferredWidth: 320
    property int preferredHeight: contentColumn.implicitHeight + 32

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 12

        Row {
            spacing: 12
            Layout.fillWidth: true

            KeepAwakeAction {
                id: keepAwake
                anchors.verticalCenter: parent.verticalCenter
            }

            Controls.Label {
                anchors.verticalCenter: keepAwake.verticalCenter
                text: "Keep Awake"
                color: HoloniightPalette.textPrimary
                font.pixelSize: 14
                font.weight: Font.Medium
            }
        }

        ChargeLimitRow {
            Layout.fillWidth: true
        }

        InhibitorSection {
            Layout.fillWidth: true
        }

        BrightnessSlider {
            Layout.fillWidth: true
        }
    }
}

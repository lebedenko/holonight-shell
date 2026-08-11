import QtQuick
import QtQuick.Controls as Controls
import Holonight.Core

Item {
    id: root

    property int preferredWidth: 300
    property int preferredHeight: 340

    Controls.Label {
        anchors.centerIn: parent
        text: "Media"
        color: HoloniightPalette.textPrimary
        font.pointSize: 10.5
    }
}

import QtQuick
import QtQuick.Controls as Controls
import Holonight.Core

Item {
    id: root

    property int preferredWidth: 400
    property int preferredHeight: 480

    Controls.Label {
        anchors.centerIn: parent
        text: "Calendar"
        color: HoloniightPalette.textPrimary
        font.pointSize: 10.5
    }
}

import QtQuick
import Holonight.Core
import Holonight.Controls

Item {
    id: root

    property int preferredWidth: 300
    property int preferredHeight: 340

    HnLabel {
        anchors.centerIn: parent
        rawText: qsTr("Media")
        role: HnTypographyRole.Body
        color: HoloniightPalette.textPrimary
    }
}

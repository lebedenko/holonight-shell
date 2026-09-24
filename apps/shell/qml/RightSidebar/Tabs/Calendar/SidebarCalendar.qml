import QtQuick
import Holonight.Core
import Holonight.Controls

Item {
    id: root

    property int preferredWidth: 400
    property int preferredHeight: 480

    HnLabel {
        anchors.centerIn: parent
        rawText: qsTr("Calendar")
        role: HnTypographyRole.Body
        color: HoloniightPalette.textPrimary
    }
}

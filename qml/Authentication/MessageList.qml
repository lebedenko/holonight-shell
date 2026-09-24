import QtQuick
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Controls

ListView {
    id: root
    required property var promptModel
    visible: count > 0
    clip: true
    Controls.ScrollBar.vertical: Controls.ScrollBar {}
    implicitHeight: Math.min(contentHeight, 160)
    model: promptModel.messages
    boundsBehavior: Flickable.StopAtBounds
    delegate: HnLabel {
        required property var model
        required property int severity
        width: ListView.view.width
        rawText: model.text
        role: HnTypographyRole.Body
        wrapMode: Text.Wrap
        color: severity === 1 ? HoloniightPalette.error : HoloniightPalette.textSecondary
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }
}

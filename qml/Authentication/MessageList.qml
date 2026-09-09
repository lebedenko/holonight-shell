import QtQuick
import QtQuick.Controls as Controls
import Holonight.Core

ListView {
    id: root
    required property var promptModel
    visible: count > 0
    clip: true
    Controls.ScrollBar.vertical: Controls.ScrollBar {}
    implicitHeight: Math.min(contentHeight, 160)
    model: promptModel.messages
    boundsBehavior: Flickable.StopAtBounds
    delegate: Controls.Label {
        required property var model
        required property int severity
        width: ListView.view.width
        text: model.text
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        color: severity === 1 ? HoloniightPalette.error : HoloniightPalette.textSecondary
        font.pointSize: 12
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }
}

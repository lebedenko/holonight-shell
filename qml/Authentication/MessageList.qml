import QtQuick
import QtQuick.Controls.Basic
import Holonight.Core

ListView {
    id: root
    required property var promptModel
    visible: count > 0
    clip: true
    ScrollBar.vertical: ScrollBar {}
    implicitHeight: Math.min(contentHeight, 160)
    model: promptModel.messages
    boundsBehavior: Flickable.StopAtBounds
    delegate: Label {
        required property var model
        required property int severity
        width: ListView.view.width
        text: model.text
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        color: severity === 1 ? HoloniightPalette.error : HoloniightPalette.textSecondary
        font.pixelSize: 16
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }
}

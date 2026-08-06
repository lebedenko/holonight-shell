import QtQuick
import QtQuick.Layouts
import Holonight.Core

import HolonightShell

ColumnLayout {
    id: root
    visible: SuspendInhibitorService.inhibitorCount > 0
    Layout.fillWidth: true
    spacing: 4

    Text {
        text: qsTr("Sleep inhibitors")
        color: HoloniightPalette.textPrimary
        font.family: AppearanceService.uiFont
        font.pixelSize: 12
        font.weight: Font.Medium
        opacity: 0.7
        Layout.fillWidth: true
    }

    Repeater {
        model: SuspendInhibitorService.inhibitorModel
        delegate: RowLayout {
            id: inhibitorRow
            required property string who
            required property string why
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: inhibitorRow.who
                color: HoloniightPalette.textPrimary
                font.family: AppearanceService.uiFont
                font.pixelSize: 12
                font.weight: Font.Medium
                elide: Text.ElideRight
                Layout.preferredWidth: 100
            }

            Text {
                text: inhibitorRow.why
                color: HoloniightPalette.textMuted
                font.family: AppearanceService.uiFont
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }
}

import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls

import HolonightShell

ColumnLayout {
    id: root
    visible: SuspendInhibitorService.inhibitorCount > 0
    Layout.fillWidth: true
    spacing: 4

    HnLabel {
        rawText: qsTr("Sleep inhibitors")
        role: HnTypographyRole.Caption
        color: HoloniightPalette.textPrimary
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

            HnLabel {
                rawText: inhibitorRow.who
                role: HnTypographyRole.Caption
                color: HoloniightPalette.textPrimary
                font.weight: Font.Medium
                elide: Text.ElideRight
                Layout.preferredWidth: 100
            }

            HnLabel {
                rawText: inhibitorRow.why
                role: HnTypographyRole.Caption
                color: HoloniightPalette.textMuted
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }
}

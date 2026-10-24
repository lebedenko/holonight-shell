import QtQuick
import QtQuick.Layouts
import HolonightShell
import Holonight.Core
import "../Status/PopupMetrics.js" as PopupMetrics

RowLayout {
    id: root
    required property string title
    required property url iconSource
    required property string timeText
    readonly property real naturalWidth: 48 + spacing + Math.max(titleMetrics.advanceWidth, timeMetrics.advanceWidth)
    spacing: PopupMetrics.rowGap

    TextMetrics {
        id: titleMetrics
        font: titleLabel.font
        text: titleLabel.text
    }
    TextMetrics {
        id: timeMetrics
        font: timeLabel.font
        text: timeLabel.text
    }

    Image {
        source: root.iconSource
        Layout.preferredWidth: 48
        Layout.preferredHeight: 48
        sourceSize: Qt.size(48, 48)
        fillMode: Image.PreserveAspectFit
    }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: PopupMetrics.textGap
        HnLabel {
            Layout.fillWidth: true
            id: titleLabel
            font.family: AppearanceService.uiFont
            rawText: root.title
            role: HnTypographyRole.MicroHeader
            color: HoloniightPalette.textSecondary
        }
        HnLabel {
            Layout.fillWidth: true
            id: timeLabel
            rawText: root.timeText
            role: HnTypographyRole.Caption
            color: HoloniightPalette.textPrimary
        }
    }
}

import QtQuick
import QtQuick.Layouts
import Holonight.Core
import "../Status/PopupMetrics.js" as PopupMetrics

RowLayout {
    id: root
    required property string title
    required property url iconSource
    required property string timeText
    spacing: PopupMetrics.rowGap

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
            rawText: root.title
            role: HnTypographyRole.MicroHeader
            color: HoloniightPalette.textSecondary
        }
        HnLabel {
            Layout.fillWidth: true
            rawText: root.timeText
            role: HnTypographyRole.Caption
            color: HoloniightPalette.textPrimary
        }
    }
}

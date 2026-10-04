import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls
import "PopupMetrics.js" as PopupMetrics

Item {
    id: root

    property string title: ""
    property string subtitle: ""
    property string iconSource: ""
    property Component trailingContent
    readonly property Item trailingItem: trailing.status === Loader.Ready ? trailing.item as Item : null

    implicitWidth: row.implicitWidth
    implicitHeight: Math.max(HnMetrics.headerHeight, row.implicitHeight)

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: PopupMetrics.rowGap

        HnIcon {
            visible: root.iconSource.length > 0
            source: root.iconSource
            normalColor: HoloniightPalette.textPrimary
            Layout.preferredWidth: HnMetrics.appTitleIconSize
            Layout.preferredHeight: HnMetrics.appTitleIconSize
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: PopupMetrics.textGap
            HnLabel {
                objectName: "popupTitle"
                Layout.fillWidth: true
                rawText: root.title
                role: HnTypographyRole.MicroHeader
                color: HoloniightPalette.textPrimary
                elide: Text.ElideRight
            }
            HnLabel {
                Layout.fillWidth: true
                visible: root.subtitle.length > 0
                rawText: root.subtitle
                role: HnTypographyRole.Caption
                color: HoloniightPalette.textMuted
                elide: Text.ElideRight
            }
        }
        Loader {
            id: trailing
            sourceComponent: root.trailingContent
        }
    }
}

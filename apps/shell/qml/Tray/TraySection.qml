pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell
import Holonight.Core
import "../Controls"

import "../Topbar"

BarSection {
    id: root

    required property string barMonitorName
    readonly property int itemSize: 34
    readonly property int itemSpacing: 8
    readonly property int maxVisible: TrayModel.maxVisible
    readonly property int contentLeftMargin: 24
    readonly property int contentRightMargin: 24
    readonly property int inheritedSectionPadding: 8

    // Computed tray ordering: urgent items first (leftmost), then non-urgent.
    // TrayModel.revision is the tracked dependency that triggers re-evaluation on any model change.
    // Each entry: { key: string, row: int }
    readonly property var orderedItems: {
        var _rev = TrayModel.revision  // tracked dependency — forces re-evaluation on model change
        var urgent = []
        var nonUrgent = []
        var count = TrayModel.rowCount()
        for (var row = 0; row < count; row++) {
            var idx = TrayModel.index(row, 0)
            var status = TrayModel.data(idx, TrayModel.StatusRole)
            var hasUnread = TrayModel.data(idx, TrayModel.HasUnreadRole) ?? false
            if (status === "Passive") { continue }
            var entry = ({ key: TrayModel.data(idx, TrayModel.ItemKeyRole), row: row })
            if (status === "NeedsAttention" || hasUnread) { urgent.push(entry) } else { nonUrgent.push(entry) }
        }
        return urgent.concat(nonUrgent)
    }

    readonly property var slotItems: root.orderedItems.slice(0, root.maxVisible)
    readonly property var displayedItems: root.expanded ? root.orderedItems : root.slotItems

    readonly property int nonPassiveCount: root.orderedItems.length
    readonly property int overflowCount: root.nonPassiveCount - root.slotItems.length
    readonly property bool hasItems: root.nonPassiveCount > 0
    readonly property bool overflowed: root.overflowCount > 0
    property bool expanded: false

    implicitWidth: trayRow.implicitWidth > 0
        ? root.contentLeftMargin + trayRow.implicitWidth + root.contentRightMargin
        : 0

    opacity: root.hasItems ? 1.0 : 0.0
    Behavior on opacity {
        NumberAnimation { duration: 100; easing.type: Easing.OutCubic }
    }

    onOverflowedChanged: {
        if (!root.overflowed) {
            root.expanded = false
        }
    }

    BarFrame {
        anchors {
            fill: parent
            leftMargin: -root.inheritedSectionPadding
            rightMargin: -root.inheritedSectionPadding
        }
        visible: root.hasItems
    }

    Row {
        id: trayRow
        anchors {
            left: parent.left
            leftMargin: root.contentLeftMargin - root.inheritedSectionPadding
            right: parent.right
            rightMargin: root.contentRightMargin - root.inheritedSectionPadding
            verticalCenter: parent.verticalCenter
        }
        spacing: root.itemSpacing

        Repeater {
            model: root.displayedItems
            delegate: TrayItem {
                required property var modelData

                size: root.itemSize
                barMonitorName: root.barMonitorName

                // Consolidate all role reads into a single computed object that tracks
                // TrayModel.revision. This is necessary because the Repeater's early-exit
                // optimisation (QVariant deep-equal) prevents delegate recreation when only
                // item data changes without slot-assignment changes (e.g. GetAll arriving
                // after fetchToolTip already inserted the item).  Reading _rev here means
                // this expression re-evaluates on every model mutation, not just slot changes.
                readonly property var _roles: {
                    var _rev = TrayModel.revision  // tracked dep
                    var idx = TrayModel.index(modelData.row, 0)
                    return {
                        service:             TrayModel.data(idx, TrayModel.ServiceRole) ?? "",
                        objectPath:          TrayModel.data(idx, TrayModel.ObjectPathRole) ?? "",
                        iconName:            TrayModel.data(idx, TrayModel.IconNameRole) ?? "",
                        attentionIconName:   TrayModel.data(idx, TrayModel.AttentionIconNameRole) ?? "",
                        status:              TrayModel.data(idx, TrayModel.StatusRole) ?? "",
                        iconPixmapUrl:       TrayModel.data(idx, TrayModel.IconPixmapUrlRole) ?? "",
                        attentionPixmapUrl:  TrayModel.data(idx, TrayModel.AttentionPixmapUrlRole) ?? "",
                        title:               TrayModel.data(idx, TrayModel.TitleRole) ?? "",
                        itemKey:             TrayModel.data(idx, TrayModel.ItemKeyRole) ?? "",
                        tooltipTitle:        TrayModel.data(idx, TrayModel.TooltipTitleRole) ?? "",
                        tooltipDescription:  TrayModel.data(idx, TrayModel.TooltipDescriptionRole) ?? "",
                        tooltipIconName:     TrayModel.data(idx, TrayModel.TooltipIconNameRole) ?? "",
                        hasUnread:           TrayModel.data(idx, TrayModel.HasUnreadRole) ?? false
                    }
                }

                service:             _roles.service
                objectPath:          _roles.objectPath
                iconName:            _roles.iconName
                attentionIconName:   _roles.attentionIconName
                status:              _roles.status
                iconPixmapUrl:       _roles.iconPixmapUrl
                attentionPixmapUrl:  _roles.attentionPixmapUrl
                title:               _roles.title
                itemKey:             _roles.itemKey
                tooltipTitle:        _roles.tooltipTitle
                tooltipDescription:  _roles.tooltipDescription
                tooltipIconName:     _roles.tooltipIconName
                hasUnread:           _roles.hasUnread

                overflowVisible: true
            }
        }

        Item {
            id: overflowButton

            width: root.itemSize
            height: root.itemSize
            visible: root.overflowed
            scale: overflowHover.hovered ? 1.04 : 1.0
            Accessible.role: Accessible.Button
            Accessible.name: "More tray items"
            Accessible.description: "Show " + root.overflowCount + " more tray items."
            Accessible.ignored: !overflowButton.visible

            Behavior on scale {
                NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
            }

            Rectangle {
                anchors.fill: parent
                radius: 7
                color: overflowHover.hovered || root.expanded
                    ? Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                              HoloniightPalette.surface.b, 0.72)
                    : Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                              HoloniightPalette.surface.b, 0.36)
                border.color: overflowHover.hovered || root.expanded
                    ? Qt.rgba(HoloniightPalette.borderActive.r, HoloniightPalette.borderActive.g,
                              HoloniightPalette.borderActive.b, 0.16)
                    : Qt.rgba(HoloniightPalette.borderPassive.r, HoloniightPalette.borderPassive.g,
                              HoloniightPalette.borderPassive.b, 0.1)
                border.width: 1

                Behavior on color {
                    ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
                }

                Behavior on border.color {
                    ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
                }
            }

            Text {
                anchors.centerIn: parent
                text: root.expanded ? "x" : "+" + root.overflowCount
                color: overflowHover.hovered ? HoloniightPalette.accentCyan : HoloniightPalette.textSecondary
                font.pointSize: 9
                font.weight: Font.DemiBold
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onClicked: root.expanded = !root.expanded
            }

            HoverHandler { id: overflowHover }

            BarTooltipArea {
                barMonitorName: root.barMonitorName
                title: root.expanded ? "Hide tray items" : "More tray items"
                description: root.expanded ? "Hide additional tray items."
                             : "Show " + root.overflowCount + " more tray items."
                iconName: "window"
            }
        }
    }

}

import QtQuick
import QtQuick.Controls as Controls
import HolonightShell
import Holonight.Core

import "../Controls"

BarSection {
    id: root

    required property string barMonitorName
    readonly property int iconSize: 32
    readonly property int cornerCut: 12
    readonly property int slantCut: 12
    readonly property int contentLeftMargin: 24
    readonly property int contentRightMargin: 24 + root.slantCut
    readonly property int inheritedSectionPadding: 8
    readonly property color frameAccent: HoloniightPalette.accentCyan

    implicitWidth: root.contentLeftMargin + logoIcon.width + logoRow.spacing + logoLabel.implicitWidth
                            + root.contentRightMargin

    BarFrame {

        anchors {
            fill: parent
            leftMargin: -root.inheritedSectionPadding
            rightMargin: -root.inheritedSectionPadding
        }
        leftCornerCut: root.cornerCut
        rightBottomOffset: root.slantCut
    }

    Row {
        id: logoRow
        anchors {
            left: parent.left
            leftMargin: root.contentLeftMargin - root.inheritedSectionPadding
            right: parent.right
            rightMargin: root.contentRightMargin - root.inheritedSectionPadding
            verticalCenter: parent.verticalCenter
        }
        spacing: 6

        // qmllint disable import unresolved-type
        HnIcon {
            id: logoIcon
            objectName: "logoIcon"
            size: root.iconSize
            tinted: SystemInfoService.logoTinted
            source: SystemInfoService.logoSource
            normalColor: HoloniightPalette.accentBlue
        }
        // qmllint enable import unresolved-type

        Controls.Label {
            id: logoLabel

            anchors.verticalCenter: logoIcon.verticalCenter
            text: SystemInfoService.displayName.toUpperCase()
            color: HoloniightPalette.textPrimary
            maximumLineCount: 1
        }
    }

    BarTooltipArea {
        barMonitorName: root.barMonitorName
        title: SystemInfoService.displayName
        description: "Running Holonight shell on " + SystemInfoService.name + "."
        iconName: "system"
    }
}

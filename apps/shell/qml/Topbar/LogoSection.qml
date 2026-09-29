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
        id: logoFrame
        objectName: "logoFrame"

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
            rendering: SystemInfoService.logoTinted ? HnIcon.Semantic : HnIcon.Original
            source: SystemInfoService.logoSource
            normalColor: HoloniightPalette.accentBlue
        }
        // qmllint enable import unresolved-type

        Controls.Label {
            id: logoLabel
            objectName: "logoLabel"

            anchors.verticalCenter: logoIcon.verticalCenter
            text: SystemInfoService.displayName.toUpperCase()
            color: HoloniightPalette.textPrimary
            maximumLineCount: 1
        }
    }

    BarTooltipArea {
        id: tooltipArea
        barMonitorName: root.barMonitorName
        title: SystemInfoService.displayName
        description: qsTr("Running Holonight shell on %1.\nClick to toggle launcher").arg(SystemInfoService.name)
        iconName: "system"
    }

    MouseArea {
        readonly property real borderInset: logoFrame.frameInset + logoFrame.strokeWidth / 2

        anchors {
            fill: logoFrame
            leftMargin: borderInset + logoFrame.leftCornerCut
            rightMargin: borderInset + logoFrame.rightBottomOffset + logoFrame.cornerRadius
            topMargin: borderInset
            bottomMargin: borderInset
        }
        acceptedButtons: Qt.LeftButton
        onClicked: {
            tooltipArea.dismissForClick()
            LauncherSurface.toggle(root.barMonitorName)
        }
    }
}

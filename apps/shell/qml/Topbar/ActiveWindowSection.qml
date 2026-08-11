import QtQuick
import QtQuick.Controls as Controls
import Holonight.Core
import HolonightShell

import "../Controls"

BarSection {
    id: root
    implicitWidth: 0

    required property string barMonitorName
    readonly property int slantCut: 12
    readonly property int contentLeftMargin: 24 + root.slantCut
    readonly property int contentRightMargin: 24 + root.slantCut
    readonly property int inheritedSectionPadding: 8
    property string localTitle: ""
    property string localCategory: ""
    property string localAppClass: ""
    readonly property string displayTitle: root.localTitle.length > 0 ? root.localTitle : "Desktop"

    Connections {
        target: ActiveWindowService
        function onMonitorWindowChanged(monitorName) {
            if (monitorName === root.barMonitorName) {
                root.localTitle = ActiveWindowService.titleForMonitor(root.barMonitorName)
                root.localCategory = ActiveWindowService.categoryForMonitor(root.barMonitorName)
                root.localAppClass = ActiveWindowService.appClassForMonitor(root.barMonitorName)
            }
        }
    }

    Component.onCompleted: {
        root.localTitle = ActiveWindowService.titleForMonitor(root.barMonitorName)
        root.localCategory = ActiveWindowService.categoryForMonitor(root.barMonitorName)
        root.localAppClass = ActiveWindowService.appClassForMonitor(root.barMonitorName)
    }

    BarFrame {

        anchors {
            fill: parent
            leftMargin: -root.inheritedSectionPadding
            rightMargin: -root.inheritedSectionPadding
        }
        leftTopOffset: root.slantCut
        rightTopOffset: root.slantCut
    }

    Column {
        anchors {
            left: parent.left
            leftMargin: root.contentLeftMargin - root.inheritedSectionPadding
            right: parent.right
            rightMargin: root.contentRightMargin - root.inheritedSectionPadding
            verticalCenter: parent.verticalCenter
        }
        spacing: 8

        Controls.Label {
            text: "// ACTIVE WINDOW"
            color: HoloniightPalette.borderActive
            font.family: AppearanceService.titleFont
            font.pointSize: AppearanceService.titleFontSize * 0.75
        }

        Row {
            spacing: 10
            width: parent.width

            AppWindowIcon {
                id: appIcon

                category: root.localCategory
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 1
            }

            Controls.Label {
                id: activeTitleLabel

                text: root.displayTitle
                elide: Text.ElideRight
                width: Math.max(0, parent.width - appIcon.width - parent.spacing)
                color: HoloniightPalette.textPrimary

                Rectangle {
                    id: titleFade

                    readonly property color fadeColor: HoloniightPalette.surface

                    visible: activeTitleLabel.truncated
                    anchors {
                        top: parent.top
                        right: parent.right
                        bottom: parent.bottom
                    }
                    width: Math.min(40, parent.width)

                    gradient: Gradient {
                        orientation: Gradient.Horizontal

                        GradientStop {
                            position: 0.0
                            color: Qt.rgba(titleFade.fadeColor.r, titleFade.fadeColor.g, titleFade.fadeColor.b, 0.0)
                        }

                        GradientStop {
                            position: 1.0
                            color: titleFade.fadeColor
                        }
                    }
                }
            }
        }
    }

    BarTooltipArea {
        barMonitorName: root.barMonitorName
        title: root.displayTitle
        description: root.localAppClass.length > 0
            ? "Focused app: " + root.localAppClass + "."
            : "No active window information."
        iconName: "window"
    }
}

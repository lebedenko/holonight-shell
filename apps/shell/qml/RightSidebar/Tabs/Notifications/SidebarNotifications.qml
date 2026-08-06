import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Effects
import QtQuick.Layouts
import Holonight.Core
import Holonight.Components
import Holonight.Controls

import HolonightShell

Item {
    id: root

    property int preferredWidth: 380
    property int preferredHeight: contentColumn.implicitHeight + 32

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 8

        // ── DND toggle row ────────────────────────────────────────────────
        Text {
            text: "Do Not Disturb"
            color: HoloniightPalette.textPrimary
            font.pixelSize: 14
            font.weight: Font.Medium
            Layout.fillWidth: true
            Layout.bottomMargin: 4
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Item {
                implicitWidth: 44
                implicitHeight: 44

                readonly property bool dndActive: NotificationService.dndEnabled

                MultiEffect {
                    source: dndDisc
                    anchors.fill: dndDisc
                    visible: parent.dndActive
                    shadowEnabled: true
                    shadowColor: HoloniightPalette.error
                    shadowBlur: 0.6
                    shadowOpacity: 0.5
                    shadowScale: 1.03
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: 0
                    autoPaddingEnabled: true
                }

                Rectangle {
                    id: dndDisc
                    anchors.fill: parent
                    radius: width / 2
                    color: parent.dndActive
                        ? Qt.rgba(HoloniightPalette.error.r, HoloniightPalette.error.g,
                                  HoloniightPalette.error.b, 0.16)
                        : Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                                  HoloniightPalette.surface.b,
                                  dndHover.hovered ? 0.55 : 0.3)
                    border.width: parent.dndActive ? 1.5 : 1
                    border.color: parent.dndActive
                        ? HoloniightPalette.error
                        : dndHover.hovered
                            ? Qt.rgba(HoloniightPalette.error.r, HoloniightPalette.error.g,
                                      HoloniightPalette.error.b, 0.45)
                            : Qt.rgba(HoloniightPalette.borderPassive.r, HoloniightPalette.borderPassive.g,
                                      HoloniightPalette.borderPassive.b, 0.6)

                    Behavior on color {
                        ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
                    }
                    Behavior on border.color {
                        ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "🔕"
                        font.pixelSize: 18
                    }
                }

                HoverHandler { id: dndHover }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: false
                    cursorShape: Qt.PointingHandCursor
                    onClicked: NotificationService.dndEnabled = !NotificationService.dndEnabled
                }
            }

            ColumnLayout {
                spacing: 2

                Text {
                    text: NotificationService.dndEnabled ? "On — non-critical muted" : "Off"
                    color: NotificationService.dndEnabled ? HoloniightPalette.error : HoloniightPalette.textPrimary
                    font.pixelSize: 13
                    font.weight: Font.Medium

                    Behavior on color {
                        ColorAnimation { duration: 120 }
                    }
                }

                Text {
                    text: "Critical alerts always break through"
                    color: HoloniightPalette.textSecondary
                    font.pixelSize: 11
                }
            }
        }

        // ── Daemon conflict diagnostic ────────────────────────────────────
        RowLayout {
            visible: NotificationService.daemonConflict
            Layout.fillWidth: true
            spacing: 8
            Layout.topMargin: 4

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: HoloniightPalette.surface
                visible: false  // spacer line hidden; topMargin provides gap
            }

            Text {
                text: "⚠"
                color: HoloniightPalette.error
                font.pixelSize: 14
            }

            Text {
                text: "Notification daemon conflict: "
                    + (NotificationService.daemonConflictOwner !== "" ? NotificationService.daemonConflictOwner : "unknown")
                    + "\nStop this service to enable HoloNight notifications."
                color: HoloniightPalette.textPrimary
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        // ── Per-app rules ─────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: HoloniightPalette.surface
            Layout.topMargin: 8
            Layout.bottomMargin: 4
        }

        Text {
            text: "App Notifications"
            color: HoloniightPalette.textPrimary
            font.pixelSize: 14
            font.weight: Font.Medium
            Layout.fillWidth: true
        }

        HnEmptyState {
            objectName: "notificationEmptyState"
            visible: NotificationRuleModel.rowCount() === 0
            titleText: qsTr("No apps seen yet")
            Layout.fillWidth: true
        }

        Repeater {
            model: NotificationRuleModel

            delegate: HnSettingsRow {
                id: appRuleDelegate

                objectName: "notificationRuleRow"
                required property string appName
                required property string displayName
                required property string displayIcon
                required property bool ruleEnabled
                required property int urgencyFilter
                required property int index

                Layout.fillWidth: true
                titleText: appRuleDelegate.displayName
                leadingContent: Component {
                    ExternalIcon {
                        iconName: appRuleDelegate.displayIcon
                        iconSize: 20
                        tintColor: HoloniightPalette.textSecondary
                        fallbackIconName: "application-x-executable"
                        preferSemanticTint: false
                        width: 20
                        height: 20
                    }
                }

                control: Component {
                    RowLayout {
                        spacing: 8

                        Controls.Switch {
                            id: enableSwitch
                            objectName: "notificationRuleEnabled"
                            checked: appRuleDelegate.ruleEnabled
                            onToggled: NotificationRuleModel.setEnabled(appRuleDelegate.index, checked)
                        }

                        HnIconComboBox {
                            id: urgencyCombo
                            objectName: "notificationUrgencyCombo"
                            implicitWidth: 130
                            currentIndex: appRuleDelegate.urgencyFilter
                            model: [qsTr("All"), qsTr("Block Low"), qsTr("Block Normal"),
                                qsTr("Block Low+Normal")]
                            enabled: appRuleDelegate.ruleEnabled
                            onActivated: NotificationRuleModel.setUrgencyFilter(appRuleDelegate.index,
                                                                                urgencyCombo.currentIndex)
                        }
                    }
                }
            }
        }
    }
}

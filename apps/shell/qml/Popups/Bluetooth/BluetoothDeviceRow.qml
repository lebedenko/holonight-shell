import Holonight.Core
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts

Item {
    id: root

    required property var record
    property bool expanded: false
    property bool adapterBusy: false
    readonly property string action: record.connected ? "disconnect" : record.paired ? "connect" : "pair"
    readonly property string actionText: record.busy ? qsTr("Working…") : record.connected ? qsTr("Disconnect") : record.paired ? qsTr("Connect") : qsTr("Pair")
    readonly property bool hasBattery: record.battery !== undefined && record.battery !== null

    signal focusEntered()
    signal expansionRequested()
    signal actionRequested(string action)

    implicitHeight: rowLayout.implicitHeight

    HoverHandler {
        id: rowHover
    }

    Rectangle {
        objectName: "rowBackground-" + root.record.id
        anchors.fill: parent
        radius: 8
        color: rowHover.hovered ? Qt.alpha(HoloniightPalette.accentBlue, 0.1) : root.expanded ? Qt.alpha(HoloniightPalette.accentBlue, 0.04) : "transparent"
        border.width: root.expanded || mainButton.activeFocus ? 1 : 0
        border.color: mainButton.activeFocus ? HoloniightPalette.accentBlue : HoloniightPalette.borderPassive
    }

    ColumnLayout {
        id: rowLayout
        anchors.fill: parent
        spacing: 4

        Item {
            Layout.fillWidth: true
            implicitHeight: 64

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                Controls.ItemDelegate {
                    id: mainButton

                    objectName: "device-" + root.record.id
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredHeight: 64
                    padding: 0
                    enabled: !root.record.busy && !root.adapterBusy
                    Accessible.name: root.record.name + ", " + status.rawText
                    Controls.ToolTip.visible: hovered && deviceName.truncated
                    Controls.ToolTip.text: root.record.name
                    onActiveFocusChanged: {
                        if (activeFocus) {
                            root.focusEntered();
                        }
                    }
                    onClicked: {
                        if (root.record.connected || root.record.paired)
                            root.expansionRequested();

                    }

                    background: Item {
                    }

                    contentItem: RowLayout {
                        spacing: 16

                        HnIcon {
                            name: {
                                const icon = root.record.icon || "bluetooth";
                                const known = ["audio-headphones", "audio-headset", "input-keyboard", "input-mouse", "input-gaming", "phone", "bluetooth", "computer"];
                                return known.indexOf(icon) >= 0 ? icon + "-symbolic" : icon;
                            }
                            size: 32
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 4

                            HnLabel {
                                id: deviceName
                                objectName: "name-" + root.record.id
                                Layout.fillWidth: true
                                rawText: root.record.name
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Rectangle {
                                    visible: root.record.connected || root.record.paired
                                    implicitWidth: 8
                                    implicitHeight: 8
                                    radius: 4
                                    color: root.record.connected ? HoloniightPalette.success : HoloniightPalette.textMuted
                                }

                                HnIcon {
                                    visible: !root.record.connected && !root.record.paired
                                    name: "network-cellular-signal-good-symbolic"
                                    size: 14
                                }

                                HnLabel {
                                    id: status

                                    Layout.fillWidth: true
                                    role: HnTypographyRole.Caption
                                    color: HoloniightPalette.textSecondary
                                    elide: Text.ElideRight
                                    rawText: root.record.busy ? qsTr("Working…") : root.record.connected ? qsTr("Connected") : root.record.paired ? qsTr("Paired · Disconnected") : root.record.rssi !== undefined && root.record.rssi !== null ? qsTr("%1 dBm").arg(root.record.rssi) : qsTr("Signal unknown")
                                }

                            }

                        }

                    }

                }

                RowLayout {
                    objectName: "battery-" + root.record.id
                    visible: root.record.connected && root.hasBattery
                    Layout.minimumWidth: 84
                    Layout.preferredWidth: 84
                    Layout.maximumWidth: 84
                    spacing: 8

                    Item {
                        Layout.minimumWidth: 26
                        Layout.maximumWidth: 26
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 14

                        Rectangle {
                            width: 23
                            height: 14
                            radius: 2
                            color: "transparent"
                            border.width: 1
                            border.color: HoloniightPalette.textSecondary

                            Rectangle {
                                x: 3
                                y: 3
                                width: 17 * Math.max(0, Math.min(100, root.record.battery || 0)) / 100
                                height: 8
                                radius: 1
                                color: HoloniightPalette.textSecondary
                            }

                        }

                        Rectangle {
                            x: 24
                            y: 4
                            width: 2
                            height: 6
                            radius: 1
                            color: HoloniightPalette.textSecondary
                        }

                    }

                    HnLabel {
                        Layout.fillWidth: true
                        role: HnTypographyRole.Caption
                        color: HoloniightPalette.textSecondary
                        horizontalAlignment: Text.AlignRight
                        rawText: root.hasBattery ? root.record.battery + "%" : ""
                    }

                }

                Controls.Button {
                    objectName: "action-" + root.record.id
                    visible: !root.record.connected && !root.record.paired
                    Layout.preferredWidth: 100
                    text: root.actionText
                    Accessible.name: text + " " + root.record.name
                    enabled: !root.record.busy && !root.adapterBusy
                    onActiveFocusChanged: {
                        if (activeFocus) {
                            root.focusEntered();
                        }
                    }
                    onClicked: root.actionRequested(root.action)
                }

                Controls.ToolButton {
                    objectName: "expand-" + root.record.id
                    visible: root.record.connected || root.record.paired
                    text: root.expanded ? "⌄" : "›"
                    Accessible.name: qsTr("Actions for %1").arg(root.record.name)
                    enabled: !root.record.busy && !root.adapterBusy
                    onActiveFocusChanged: {
                        if (activeFocus) {
                            root.focusEntered();
                        }
                    }
                    onClicked: root.expansionRequested()
                }

            }

        }

        Controls.Button {
            objectName: "expandedAction-" + root.record.id
            Layout.alignment: Qt.AlignRight
            Layout.rightMargin: 12
            Layout.bottomMargin: 12
            visible: root.expanded && (root.record.connected || root.record.paired)
            text: root.actionText
            Accessible.name: text + " " + root.record.name
            enabled: !root.record.busy && !root.adapterBusy
            onActiveFocusChanged: {
                if (activeFocus) {
                    root.focusEntered();
                }
            }
            onClicked: root.actionRequested(root.action)
        }

    }
}

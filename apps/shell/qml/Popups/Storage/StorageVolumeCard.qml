pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import HolonightShell
import Holonight.Core
import Holonight.Controls

// REQ-F-007/012/013/014/018/019: one row per volume beneath its drive header.
// REQ-NF-002: the body-click-to-open handler (bodyArea) and the action buttons (actionsRow)
// are geometrically disjoint siblings — a button press can never also reach the body TapHandler.
Rectangle {
    id: root

    required property string targetId
    required property string driveId
    required property string name
    required property string stateText
    required property bool mounted
    required property bool canMount
    required property bool canUnmount
    required property real usedBytes
    required property real totalBytes
    required property real freeBytes
    required property string operationText
    required property string errorText

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + 24
    radius: 10
    color: HoloniightPalette.surfaceRaised
    border.color: root.errorText.length > 0 ? HoloniightPalette.error : HoloniightPalette.borderSubtle
    border.width: 1

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Item {
            id: bodyArea
            Layout.fillWidth: true
            implicitHeight: bodyColumn.implicitHeight

            TapHandler {
                gesturePolicy: TapHandler.ReleaseWithinBounds
                enabled: root.operationText.length === 0
                onTapped: StorageService.openVolume(root.targetId)
            }

            ColumnLayout {
                id: bodyColumn
                width: parent.width
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        implicitWidth: 8
                        implicitHeight: 8
                        radius: 4
                        color: root.mounted ? HoloniightPalette.primary : "transparent"
                        border.color: root.mounted ? HoloniightPalette.primary : HoloniightPalette.borderStrong
                        border.width: 1
                    }

                    HnLabel {
                        Layout.fillWidth: true
                        rawText: root.name
                        elide: Text.ElideRight
                        role: HnTypographyRole.Body
                    }
                }

                StorageCapacityBar {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    visible: root.mounted
                    usedBytes: root.usedBytes
                    totalBytes: root.totalBytes
                    freeBytes: root.freeBytes
                }

                HnLabel {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    visible: root.mounted
                    rawText: root.stateText
                    role: HnTypographyRole.Caption
                    color: HoloniightPalette.textMuted
                    elide: Text.ElideMiddle
                }
            }
        }

        RowLayout {
            id: actionsRow
            Layout.fillWidth: true
            Layout.leftMargin: 16
            spacing: 8

            HnLabel {
                Layout.fillWidth: true
                visible: root.errorText.length > 0
                rawText: root.errorText
                wrapMode: Text.Wrap
                role: HnTypographyRole.Caption
                color: HoloniightPalette.error
            }

            Controls.Button {
                text: qsTr("Try Again")
                visible: root.errorText.length > 0
                onClicked: StorageService.retry(root.targetId)
            }

            HnLabel {
                Layout.fillWidth: true
                visible: root.errorText.length === 0 && root.operationText.length > 0
                rawText: root.operationText
                role: HnTypographyRole.Caption
                color: HoloniightPalette.textMuted
            }

            Item { Layout.fillWidth: true; visible: root.errorText.length === 0 && root.operationText.length === 0 }

            Controls.Button {
                text: qsTr("Mount")
                highlighted: true
                visible: root.errorText.length === 0 && root.operationText.length === 0 && root.canMount
                onClicked: StorageService.mount(root.targetId)
            }
            Controls.Button {
                text: qsTr("Unmount")
                visible: root.errorText.length === 0 && root.operationText.length === 0 && root.canUnmount
                onClicked: StorageService.unmount(root.targetId)
            }
        }
    }
}

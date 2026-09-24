pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import HolonightShell
import Holonight.Core
import Holonight.Controls

ColumnLayout {
    id: root
    spacing: 8
    HnLabel { rawText: qsTr("Storage"); role: HnTypographyRole.Title }
    HnLabel {
        Layout.fillWidth: true
        visible: StorageService.errorMessage.length > 0
        rawText: StorageService.errorMessage
        wrapMode: Text.Wrap
        color: HoloniightPalette.error
    }
    HnLabel {
        visible: StorageService.count === 0
        rawText: qsTr("No removable storage")
    }
    ListView {
        id: devices
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: 12
        model: StorageService
        Controls.ScrollBar.vertical: Controls.ScrollBar {}
        section.property: "driveId"
        section.delegate: HnLabel {
            required property string section
            rawText: StorageService.driveLabel(section)
            role: HnTypographyRole.Caption
        }
        delegate: ColumnLayout {
            id: row
            required property string targetId
            required property string driveId
            required property string name
            required property string stateText
            required property bool canMount
            required property bool canUnmount
            required property bool canEject
            required property bool canPowerOff
            width: devices.width
            HnLabel { rawText: row.name; Layout.fillWidth: true; elide: Text.ElideRight }
            HnLabel { rawText: row.stateText; Layout.fillWidth: true; wrapMode: Text.Wrap; role: HnTypographyRole.Caption }
            Flow {
                Layout.fillWidth: true
                spacing: 4
                Controls.Button { text: qsTr("Mount"); visible: row.canMount; onClicked: StorageService.mount(row.targetId) }
                Controls.Button { text: qsTr("Unmount"); visible: row.canUnmount; onClicked: StorageService.unmount(row.targetId) }
                Controls.Button { text: qsTr("Eject"); visible: row.canEject; onClicked: StorageService.eject(row.driveId) }
                Controls.Button { text: qsTr("Power off…"); visible: row.canPowerOff; onClicked: StorageService.requestPowerOff(row.driveId) }
            }
        }
    }
    HnLabel {
        visible: StorageService.confirmationText.length > 0
        Layout.fillWidth: true
        rawText: StorageService.confirmationText
        wrapMode: Text.Wrap
    }
    RowLayout {
        visible: StorageService.confirmationText.length > 0
        Controls.Button { text: qsTr("Power off"); onClicked: StorageService.confirmPowerOff() }
        Controls.Button { text: qsTr("Cancel"); onClicked: StorageService.cancelPowerOff() }
    }
}

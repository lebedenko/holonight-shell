pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Controls
import Holonight.Components
import HolonightShell

// REQ-F-007: drive-grouped popup, each drive a collapsible header (StorageDriveSection) over
// its StorageVolumeCard rows. REQ-F-020: transient empty-state, shown instead of closing the
// popup when the last device disappears while it is open.
ColumnLayout {
    id: root
    spacing: 8

    // {driveId: true} for collapsed drives; absent/false = expanded (default).
    property var collapsedDriveIds: ({})

    function toggleDriveCollapsed(driveId) {
        const next = Object.assign({}, root.collapsedDriveIds);
        if (next[driveId]) {
            delete next[driveId];
        } else {
            next[driveId] = true;
        }
        root.collapsedDriveIds = next;
    }

    HnPanelHeader {
        Layout.fillWidth: true
        dividerVisible: false
        title: qsTr("Removable Storage")
        description: StorageService.deviceCount === 0 ? qsTr("No devices connected")
                     : (StorageService.deviceCount === 1 ? qsTr("1 device connected")
                        : qsTr("%1 devices connected").arg(StorageService.deviceCount))

        leadingContent: Component {
            ExternalIcon {
                iconName: "drive-removable-media-symbolic"
                iconSize: 28
                tintColor: HoloniightPalette.textPrimary
            }
        }

        trailingContent: Component {
            HnIconButton {
                objectName: "storageSettingsButton"
                icon.source: "preferences-system-symbolic"
                icon.color: HoloniightPalette.textSecondary
                Accessible.name: qsTr("Storage settings")
                // Inert for now — holonight-settings has no Storage page yet; wiring this up
                // is out of scope for this cycle (product decision).
            }
        }
    }

    HnEmptyState {
        objectName: "storageEmptyState"
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: StorageService.deviceCount === 0
        titleText: qsTr("No removable storage")
        descriptionText: qsTr("External drives and USB devices will appear here")

        graphicContent: Component {
            ExternalIcon {
                iconName: "drive-removable-media-symbolic"
                iconSize: 48
                tintColor: HoloniightPalette.textMuted
            }
        }
    }

    ListView {
        id: devices
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: StorageService.deviceCount > 0
        clip: true
        spacing: 10
        model: StorageService
        Controls.ScrollBar.vertical: Controls.ScrollBar {}
        section.property: "driveId"
        section.delegate: StorageDriveSection {
            width: devices.width
            collapsed: !!root.collapsedDriveIds[section]
            onToggleCollapsed: root.toggleDriveCollapsed(section)
        }
        delegate: StorageVolumeCard {
            id: volumeCard
            width: devices.width
            visible: !root.collapsedDriveIds[driveId]
            height: visible ? implicitHeight : 0
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Controls.Button {
            text: qsTr("Open in Files")
            onClicked: StorageService.openInFiles()
        }
        Controls.Button {
            text: qsTr("Show All Devices")
            onClicked: StorageService.showAllDevices()
        }
    }
}

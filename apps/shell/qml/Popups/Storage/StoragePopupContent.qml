pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Controls
import Holonight.Components
import HolonightShell
import "../Status/PopupMetrics.js" as PopupMetrics

// REQ-F-007: drive-grouped popup, each drive a collapsible header (StorageDriveSection) over
// its StorageVolumeCard rows. REQ-F-020: transient empty-state, shown instead of closing the
// popup when the last device disappears while it is open.
PopupContentLayout {
    id: root
    naturalWidth: footerItem ? footerItem.implicitWidth : 0

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

    header: Component {
        PopupHeader {
            title: qsTr("Removable Storage")
            subtitle: StorageService.deviceCount === 1 ? qsTr("1 device connected") : qsTr("%1 devices connected").arg(StorageService.deviceCount)
            iconSource: "drive-removable-media-symbolic"
        }
    }

    HnEmptyState {
        objectName: "storageEmptyState"
        Layout.fillWidth: true
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
        visible: StorageService.deviceCount > 0
        clip: true
        spacing: 0
        interactive: false
        implicitHeight: contentHeight
        model: StorageService
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
            height: visible ? implicitHeight + 8 : 0
        }
    }

    footer: Component { RowLayout {
        id: actions
        Layout.fillWidth: true
        spacing: PopupMetrics.rowGap

        Controls.Button {
            text: qsTr("Open in Files")
            onClicked: StorageService.openInFiles()
        }
        Controls.Button {
            text: qsTr("Show All Devices")
            onClicked: StorageService.showAllDevices()
        }
        Item { Layout.fillWidth: true }
    }
    }
}

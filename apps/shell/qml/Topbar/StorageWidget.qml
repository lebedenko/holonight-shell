import QtQuick
import HolonightShell
import Holonight.Core
import Holonight.Controls
import Holonight.Components

// REQ-F-003/004/005: hidden (deviceCount == 0) / subdued (devices present, none mounted) /
// active+badge (at least one mounted) states.
BarSection {
    id: root
    required property string barMonitorName

    readonly property int deviceCount: StorageService.deviceCount
    readonly property bool active: root.deviceCount > 0 && StorageService.hasMounted

    implicitWidth: root.deviceCount > 0 ? 44 : 0
    visible: root.deviceCount > 0
    enabled: root.visible

    ExternalIcon {
        id: storageIcon
        anchors.centerIn: parent
        iconSize: 22
        iconName: "drive-removable-media-symbolic"
        tintColor: root.active ? HoloniightPalette.primary : HoloniightPalette.textMuted

        Behavior on tintColor {
            ColorAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
    }

    Rectangle {
        id: badge
        visible: root.active
        width: Math.max(15, badgeLabel.implicitWidth + 8)
        height: 15
        radius: height / 2
        color: HoloniightPalette.primary
        anchors {
            right: storageIcon.right
            top: storageIcon.top
            rightMargin: -6
            topMargin: -4
        }

        HnLabel {
            id: badgeLabel
            anchors.centerIn: parent
            rawText: root.deviceCount.toString()
            role: HnTypographyRole.Caption
            color: HoloniightPalette.onPrimary
        }
    }

    BarTooltipArea {
        barMonitorName: root.barMonitorName
        title: qsTr("Storage")
        description: qsTr("Manage removable storage")
    }
    StatusPopupTriggerArea {
        popupId: "storage"
        barMonitorName: root.barMonitorName
    }
}

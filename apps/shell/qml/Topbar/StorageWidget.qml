import QtQuick
import HolonightShell
import Holonight.Core
import Holonight.Controls

BarSection {
    id: root
    required property string barMonitorName
    implicitWidth: StorageService.count > 0 ? 44 : 0
    visible: StorageService.count > 0
    enabled: visible

    HnIcon {
        anchors.centerIn: parent
        width: 22
        height: 22
        source: "image://icon/drive-removable-media-symbolic"
        normalColor: HoloniightPalette.textPrimary
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

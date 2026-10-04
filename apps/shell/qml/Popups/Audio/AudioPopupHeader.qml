import QtQuick
import Holonight.Core
import Holonight.Controls
import HolonightShell

PopupHeader {
    id: root
    property Item nextTabItem: null
    readonly property Item settingsButtonItem: trailingItem
    title: qsTr("Audio")
    iconSource: "audio-volume-high-symbolic"
    trailingContent: Component {
        HnIconButton {
            objectName: "headerSettingsGear"
            icon.source: "qrc:/HolonightShell/common/network-settings.svg"
            icon.color: HoloniightPalette.textSecondary
            Accessible.name: qsTr("Open Audio settings")
            activeFocusOnTab: true
            KeyNavigation.tab: root.nextTabItem
            KeyNavigation.priority: KeyNavigation.BeforeItem
            onClicked: {
                SettingsNavigationService.openPage("audio")
                StatusPopupSurface.hide()
            }
        }
    }
}

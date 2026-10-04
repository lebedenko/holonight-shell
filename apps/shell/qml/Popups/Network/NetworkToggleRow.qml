import QtQuick
import QtQuick.Controls as Controls
import Holonight.Core
import HolonightShell

PopupHeader {
    title: qsTr("Network")
    iconSource: "network-wireless-symbolic"
    trailingContent: Component {
        Controls.Switch {
            objectName: "wifiSwitch"
            enabled: NetworkService.available && NetworkService.wifiHardwareEnabled
            checked: NetworkService.wifiEnabled
            Accessible.role: Accessible.CheckBox
            Accessible.checked: checked
            Accessible.name: qsTr("Wi-Fi")
            onClicked: NetworkService.setWifiEnabled(!NetworkService.wifiEnabled)
        }
    }
}

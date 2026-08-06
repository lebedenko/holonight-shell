import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls

import HolonightShell

HnSettingsRow {
    id: root
    objectName: "chargeLimitSettingsRow"
    visible: BatteryService.chargeLimit >= 0
    Layout.fillWidth: true
    titleText: qsTr("Charge limit")
    control: Component {
        Text {
            text: BatteryService.chargeLimit + "%"
            color: HoloniightPalette.textMuted
            font.family: AppearanceService.fixedFont
            font.pixelSize: 13
        }
    }
}

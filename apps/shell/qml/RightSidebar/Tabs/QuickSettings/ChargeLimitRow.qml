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
        HnLabel {
            rawText: BatteryService.chargeLimit + "%"
            role: HnTypographyRole.Code
            color: HoloniightPalette.textMuted
        }
    }
}

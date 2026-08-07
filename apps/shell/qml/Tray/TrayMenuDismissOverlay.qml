import QtQuick
import Holonight.Core
import HolonightShell

import "../Utility" as Utility

Rectangle {
    id: root
    color: AppearanceService.debugOverlays
        ? Qt.rgba(HoloniightPalette.accentCyan.r, HoloniightPalette.accentCyan.g,
                  HoloniightPalette.accentCyan.b, 0.10)
        : "transparent"

    Utility.AppearanceReloadBridge {}

    MouseArea {
        anchors.fill: parent
        onClicked: TrayMenuSurface.close()
    }
}

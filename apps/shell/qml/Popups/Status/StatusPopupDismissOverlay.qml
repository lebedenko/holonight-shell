import QtQuick
import Holonight.Core
import HolonightShell

import "../../Utility" as Utility

// Fullscreen transparent capture layer rendered beneath the popup (same layer, created
// first). Any click that does not land on the popup lands here and dismisses it.
Rectangle {
    id: root
    color: AppearanceService.debugOverlays
        ? Qt.rgba(HoloniightPalette.accentCyan.r, HoloniightPalette.accentCyan.g,
                  HoloniightPalette.accentCyan.b, 0.10)
        : "transparent"

    Utility.ThemeReloadBridge {}

    MouseArea {
        anchors.fill: parent
        onClicked: StatusPopupSurface.hide()
    }
}

import QtQuick
import Holonight.Core

import HolonightShell

Item {
    id: root

    width: 0
    height: 0
    visible: false

    Component.onCompleted: HoloniightPalette.reload()

    Connections {
        target: ThemeService

        function onPaletteReloadRequested() {
            HoloniightPalette.reload()
        }
    }
}

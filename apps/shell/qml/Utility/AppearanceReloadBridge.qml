import QtQuick
import Holonight.Core

import HolonightShell

Item {
    width: 0
    height: 0
    visible: false

    Component.onCompleted: HoloniightPalette.reload()

    Connections {
        target: AppearanceService

        function onRevisionChanged() {
            HoloniightPalette.reload()
        }
    }
}

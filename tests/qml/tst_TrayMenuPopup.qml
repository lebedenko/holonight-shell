import QtQuick
import QtTest
import Holonight.Core
import Holonight.Controls

import HolonightShell

TestCase {
    id: root

    name: "TrayMenuPopup"

    Component {
        id: trayMenuComponent

        TrayMenuPopup {
            width: 244
            height: 120
        }
    }

    function test_tray_menu_uses_hn_surface_frame_with_menu_role() {
        const popup = createTemporaryObject(trayMenuComponent, null)
        verify(popup !== null, "TrayMenuPopup should instantiate")

        let frame = null
        for (let i = 0; i < popup.children.length; ++i) {
            if (popup.children[i].surfaceRole !== undefined) {
                frame = popup.children[i]
                break
            }
        }

        verify(frame !== null, "TrayMenuPopup should contain an HnSurfaceFrame panel")
        compare(frame.surfaceRole, HnSurfaceRole.Menu, "Surface role must be HnSurfaceRole.Menu")
        compare(frame.fillColor, HoloniightPalette.surfaceRaised, "Fill color must be surfaceRaised")
        compare(frame.borderColor, HoloniightPalette.borderPassive, "Border color must be borderPassive")
    }
}

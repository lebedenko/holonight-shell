import QtQuick
import QtTest
import Holonight.Core
import Holonight.Controls

import HolonightShell

TestCase {
    id: root

    name: "TooltipPopup"

    Component {
        id: tooltipComponent

        TooltipPopup {
            width: 240
            height: 60
        }
    }

    Component {
        id: networkIconComponent

        NetworkIcon {
            width: 32
            height: 32
        }
    }

    function findByObjectName(item, objectName) {
        if (item.objectName === objectName) {
            return item
        }
        for (let i = 0; i < item.children.length; ++i) {
            const match = findByObjectName(item.children[i], objectName)
            if (match !== null) {
                return match
            }
        }
        return null
    }

    function test_tooltip_uses_hn_surface_frame_with_tooltip_role() {
        const tooltip = createTemporaryObject(tooltipComponent, null)
        verify(tooltip !== null, "TooltipPopup should instantiate")

        let frame = null
        for (let i = 0; i < tooltip.children.length; ++i) {
            if (tooltip.children[i].surfaceRole !== undefined) {
                frame = tooltip.children[i]
                break
            }
        }

        verify(frame !== null, "TooltipPopup should contain an HnSurfaceFrame panel")
        compare(frame.surfaceRole, HnSurfaceRole.Tooltip, "Surface role must be HnSurfaceRole.Tooltip")
        compare(frame.fillColor, HoloniightPalette.surfaceRaised, "Fill color must be surfaceRaised")
        compare(frame.borderColor, tooltip.tooltipBorderColor, "Border color must use the subtle tooltip edge")
    }

    function test_battery_tooltip_preserves_state_and_aspect_ratio() {
        TooltipSurface.show("", 0, 0, "Battery", "37% charged", "battery", 37, true, 100)
        const tooltip = createTemporaryObject(tooltipComponent, null)
        verify(tooltip !== null, "TooltipPopup should instantiate")

        const icon = findByObjectName(tooltip, "tooltipShellIcon")
        verify(icon !== null, "TooltipPopup should contain its shell icon")
        compare(icon.width, 28)
        compare(icon.height, 14)
        compare(icon.batteryPercent, 37)
        compare(icon.charging, true)

        TooltipSurface.hide()
    }

    function test_network_signal_strength_controls_visible_bars() {
        const icon = createTemporaryObject(networkIconComponent, null, {
            "name": "wifi_online",
            "signalStrength": 66
        })
        verify(icon !== null, "NetworkIcon should instantiate")
        compare(icon.signalBars, 3)

        icon.signalStrength = 1
        compare(icon.signalBars, 1)
        icon.signalStrength = 0
        compare(icon.signalBars, 0)
        icon.signalStrength = 100
        compare(icon.signalBars, 4)
        icon.name = "wifi_offline"
        compare(icon.signalBars, 0)
    }
}

import QtQuick
import QtTest
import Holonight.Core

TestCase {
    id: root

    name: "HnIcon"

    Component {
        id: iconComponent
        HnIcon {
            source: ""
        }
    }

    function test_defaults() {
        const icon = createTemporaryObject(iconComponent, null)
        verify(icon)
        compare(icon.size, 24)
        compare(icon.iconState, HnIcon.Normal)
        compare(icon.tinted, true)
        compare(icon.implicitWidth, 24)
        compare(icon.implicitHeight, 24)
    }

    function test_state_normal_uses_normalColor() {
        const icon = createTemporaryObject(iconComponent, null, { "normalColor": "#ff0000" })
        verify(icon)
        compare(icon.iconState, HnIcon.Normal)
        compare(icon.resolvedColor, Qt.color("#ff0000"))
    }

    function test_state_muted_uses_mutedColor() {
        const icon = createTemporaryObject(iconComponent, null, {
            "iconState": HnIcon.Muted,
            "mutedColor": "#00ff00"
        })
        verify(icon)
        compare(icon.resolvedColor, Qt.color("#00ff00"))
    }

    function test_state_disabled_uses_disabledColor() {
        const icon = createTemporaryObject(iconComponent, null, {
            "iconState": HnIcon.Disabled,
            "disabledColor": "#0000ff"
        })
        verify(icon)
        compare(icon.resolvedColor, Qt.color("#0000ff"))
    }

    function test_state_active_uses_activeColor() {
        const icon = createTemporaryObject(iconComponent, null, {
            "iconState": HnIcon.Active,
            "activeColor": "#ffff00"
        })
        verify(icon)
        compare(icon.resolvedColor, Qt.color("#ffff00"))
    }

    function test_cross_state_isolation() {
        const icon = createTemporaryObject(iconComponent, null, {
            "iconState": HnIcon.Normal,
            "normalColor": "#ff0000",
            "mutedColor": "#00ff00"
        })
        verify(icon)
        compare(icon.resolvedColor, Qt.color("#ff0000"))
    }

    function test_color_override_updates_resolvedColor() {
        const icon = createTemporaryObject(iconComponent, null, { "normalColor": "#ff0000" })
        verify(icon)
        compare(icon.resolvedColor, Qt.color("#ff0000"))
        icon.normalColor = "#00ffff"
        compare(icon.resolvedColor, Qt.color("#00ffff"))
    }

    function test_state_change_updates_resolvedColor() {
        const icon = createTemporaryObject(iconComponent, null, {
            "normalColor": "#ff0000",
            "activeColor": "#0000ff"
        })
        verify(icon)
        compare(icon.resolvedColor, Qt.color("#ff0000"))
        icon.iconState = HnIcon.Active
        compare(icon.resolvedColor, Qt.color("#0000ff"))
    }

    function test_out_of_range_iconState_falls_through_to_normal() {
        const icon = createTemporaryObject(iconComponent, null, { "normalColor": "#aabbcc" })
        verify(icon)
        icon.iconState = 99
        compare(icon.resolvedColor, Qt.color("#aabbcc"))
    }

    function test_size_sets_implicit_dimensions() {
        const icon = createTemporaryObject(iconComponent, null, { "size": 48 })
        verify(icon)
        compare(icon.implicitWidth, 48)
        compare(icon.implicitHeight, 48)
    }

    function test_size_change_updates_implicit_dimensions() {
        const icon = createTemporaryObject(iconComponent, null)
        verify(icon)
        compare(icon.implicitWidth, 24)
        icon.size = 32
        compare(icon.implicitWidth, 32)
        compare(icon.implicitHeight, 32)
    }

    function test_tinted_toggle_no_crash() {
        const icon = createTemporaryObject(iconComponent, null)
        verify(icon)
        compare(icon.tinted, true)
        icon.tinted = false
        compare(icon.tinted, false)
        icon.tinted = true
        compare(icon.tinted, true)
    }

    function test_empty_source_no_crash() {
        const icon = createTemporaryObject(iconComponent, null, { "source": "" })
        verify(icon)
    }

    function test_invalid_source_no_crash() {
        const icon = createTemporaryObject(iconComponent, null, { "source": "image://invalid/nonexistent" })
        verify(icon)
    }

    function test_shell_svg_source_uses_hnicons_provider_when_tinted() {
        const icon = createTemporaryObject(iconComponent, null, {
            "source": "qrc:/HolonightShell/bar-icons/special-ws.svg",
            "normalColor": "#ff0000"
        })
        verify(icon)
        compare(String(icon._renderSource).indexOf("image://hnicons/"), 0)
    }

    function test_weather_compass_layers_use_hnicons_provider_when_tinted() {
        const sources = [
            "qrc:/HolonightShell/weather-ui/wind-compass.svg",
            "qrc:/HolonightShell/weather-ui/wind-compass-labels.svg",
            "qrc:/HolonightShell/weather-ui/wind-compass-accent.svg"
        ]

        for (const source of sources) {
            const icon = createTemporaryObject(iconComponent, null, { "source": source })
            verify(icon)
            compare(String(icon._renderSource).indexOf("image://hnicons/"), 0)
            icon.destroy()
        }
    }

    function test_image_icon_source_remains_on_icon_provider() {
        const icon = createTemporaryObject(iconComponent, null, {
            "source": "image://icon/audio-volume-high"
        })
        verify(icon)
        compare(String(icon._renderSource), "image://icon/audio-volume-high")
    }
}

import QtQuick
import QtTest
import Holonight.Core

import HolonightShell

TestCase {
    id: root

    name: "NetworkPopupIcon"

    Component {
        id: iconComponent

        NetworkPopupIcon {}
    }

    function test_supported_glyphs_select_their_vector_shape() {
        const glyphs = [
            { name: "refresh", objectName: "refreshGlyph" },
            { name: "settings", objectName: "settingsGlyph" },
            { name: "info", objectName: "infoGlyph" },
            { name: "lock", objectName: "lockGlyph" },
            { name: "chevron", objectName: "chevronGlyph" }
        ]

        for (const glyph of glyphs) {
            const icon = createTemporaryObject(iconComponent, null, { name: glyph.name })
            verify(icon)
            compare(icon.recognized, true)
            compare(findChild(icon, glyph.objectName).visible, true)
            icon.destroy()
        }
    }

    function test_color_is_palette_driven() {
        const icon = createTemporaryObject(iconComponent, null, {
            name: "refresh",
            iconColor: HoloniightPalette.accentBlue
        })
        verify(icon)
        compare(icon.resolvedColor, HoloniightPalette.accentBlue)

        icon.iconColor = HoloniightPalette.accentCyan
        compare(icon.resolvedColor, HoloniightPalette.accentCyan)
    }

    function test_unknown_glyph_is_not_recognized() {
        const icon = createTemporaryObject(iconComponent, null, { name: "missing" })
        verify(icon)
        compare(icon.recognized, false)
    }
}

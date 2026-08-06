import QtQuick
import QtTest
import Holonight.Core

import HolonightShell

TestCase {
    id: root

    name: "OsdSelectionRenderer"

    Component {
        id: selectionComponent

        OsdSelectionRenderer {}
    }

    function makeSelection(shortLabel, fullLabel) {
        const item = createTemporaryObject(selectionComponent, null, {
            shortLabel: shortLabel,
            fullLabel: fullLabel
        })
        verify(item)
        // Component.onCompleted seeds the display properties from the required ones.
        compare(item.ready, true)
        return item
    }

    function test_keyboard_glyph_is_used_for_the_selection_channel() {
        const item = makeSelection("EN", "English (US)")
        compare(findChild(item, "selectionIcon").name, "keyboard")
    }

    // REQ-F-015: the short label carries the glance value, so it is rendered large; the full label is
    // secondary. Both are declared in points so the comparison is direct.
    function test_short_label_is_the_dominant_text() {
        const item = makeSelection("EN", "English (US)")

        verify(item.shortLabelPointSize >= 32)
        verify(item.fullLabelPointSize < item.shortLabelPointSize)

        const shortText = findChild(item, "shortLabelText")
        const fullText = findChild(item, "fullLabelText")
        compare(shortText.font.pointSize, item.shortLabelPointSize)
        compare(fullText.font.pointSize, item.fullLabelPointSize)
        compare(shortText.font.bold, true)
        compare(shortText.color, HoloniightPalette.textPrimary)
        compare(fullText.color, HoloniightPalette.textMuted)
    }

    function test_labels_render_from_the_required_properties() {
        const item = makeSelection("EN", "English (US)")
        compare(findChild(item, "shortLabelText").text, "EN")
        compare(findChild(item, "fullLabelText").text, "English (US)")
        compare(findChild(item, "shortLabelText").textFormat, Text.PlainText)
        compare(findChild(item, "fullLabelText").textFormat, Text.PlainText)
    }

    // REQ-F-017: an update while the OSD is already visible swaps the text in place — the label fades
    // out, changes at the bottom of the fade, and fades back in. The surface itself never moves, so the
    // old text is still on screen at the instant the property changes.
    function test_label_change_swaps_at_the_bottom_of_a_fade() {
        const item = makeSelection("EN", "English (US)")
        const shortText = findChild(item, "shortLabelText")

        item.shortLabel = "DE"
        compare(shortText.text, "EN")

        tryVerify(function () {
            return shortText.opacity < 1
        })
        tryCompare(shortText, "text", "DE")
        tryCompare(shortText, "opacity", 1)
        compare(item.displayShortLabel, "DE")
    }

    // The two labels fade independently: a full-label-only change (same layout code, different
    // description) must not disturb the large label.
    function test_full_label_fades_without_touching_the_short_label() {
        const item = makeSelection("EN", "English (US)")
        const shortText = findChild(item, "shortLabelText")
        const fullText = findChild(item, "fullLabelText")

        item.fullLabel = "English (UK)"
        compare(fullText.text, "English (US)")
        compare(shortText.opacity, 1)

        tryCompare(fullText, "text", "English (UK)")
        tryCompare(fullText, "opacity", 1)
        compare(shortText.opacity, 1)
        compare(shortText.text, "EN")
    }

    // Re-asserting the same label is a no-op: the change handlers compare against what is displayed, so
    // a repeated event does not produce a gratuitous flicker.
    function test_repeated_label_does_not_start_a_fade() {
        const item = makeSelection("EN", "English (US)")
        const shortText = findChild(item, "shortLabelText")

        item.shortLabel = "EN"
        compare(shortText.opacity, 1)

        wait(item.fadeDuration * 3)
        compare(shortText.opacity, 1)
        compare(shortText.text, "EN")
    }

    // The fade reads root.shortLabel when it bottoms out rather than a value captured at the start, so
    // a second change arriving mid-fade still lands on the newest label.
    function test_change_during_a_fade_lands_on_the_newest_label() {
        const item = makeSelection("EN", "English (US)")
        const shortText = findChild(item, "shortLabelText")

        item.shortLabel = "DE"
        tryVerify(function () {
            return shortText.opacity < 1
        })

        item.shortLabel = "FR"
        tryCompare(shortText, "text", "FR")
        tryCompare(shortText, "opacity", 1)
        compare(item.displayShortLabel, "FR")
    }

    function test_label_changes_keep_the_renderer_width_stable() {
        const item = makeSelection("EN", "English (US)")
        const initialWidth = item.implicitWidth

        item.shortLabel = "A deliberately oversized layout abbreviation"
        item.fullLabel = "A substantially longer keyboard layout name than the initial value"

        tryCompare(findChild(item, "shortLabelText"), "text", item.shortLabel)
        tryCompare(findChild(item, "fullLabelText"), "text", item.fullLabel)
        compare(item.implicitWidth, initialWidth)
    }
}

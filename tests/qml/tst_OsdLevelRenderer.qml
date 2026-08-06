import QtQuick
import QtTest
import Holonight.Core

import HolonightShell

TestCase {
    id: root

    name: "OsdLevelRenderer"

    Component {
        id: levelComponent

        OsdLevelRenderer {}
    }

    SignalSpy {
        id: valueSpy

        signalName: "valueChanged"
    }

    // Every case builds a fresh instance with the value already in place: `Behavior on value` does not
    // run for properties supplied at construction, so tier assertions stay free of animation timing.
    function makeLevel(channel, value, muted) {
        const item = createTemporaryObject(levelComponent, null, {
            channel: channel,
            value: value,
            muted: muted
        })
        verify(item)
        return item
    }

    // REQ-F-020: exactly four audio icon names, with the tier boundaries at 34 and 67.
    function test_audio_icon_tiers_data() {
        return [
            { tag: "silent", value: 0, expected: "audio-volume-low" },
            { tag: "just-below-medium", value: 33, expected: "audio-volume-low" },
            { tag: "medium-boundary", value: 34, expected: "audio-volume-medium" },
            { tag: "just-below-high", value: 66, expected: "audio-volume-medium" },
            { tag: "high-boundary", value: 67, expected: "audio-volume-high" },
            { tag: "full", value: 100, expected: "audio-volume-high" }
        ]
    }

    function test_audio_icon_tiers(data) {
        const item = makeLevel("audio-volume", data.value, false)
        compare(item.iconName, data.expected)
        compare(findChild(item, "levelIcon").name, data.expected)
    }

    // REQ-F-020: mute is a state, not a tier — it wins over whatever the value would have selected.
    function test_muted_audio_overrides_every_tier() {
        for (const value of [0, 50, 100]) {
            const item = makeLevel("audio-volume", value, true)
            compare(item.iconName, "audio-volume-muted")
            compare(findChild(item, "levelIcon").name, "audio-volume-muted")
        }
    }

    // REQ-F-021: brightness is checked before both the mute flag and the tiers, so it always resolves
    // to the single brightness glyph. `muted: true` is not reachable from BrightnessChannelSource, but
    // pinning it here keeps the precedence explicit.
    function test_brightness_always_uses_the_brightness_icon() {
        for (const muted of [false, true]) {
            const item = makeLevel("screen-brightness", 90, muted)
            compare(item.iconName, "brightness")
            compare(findChild(item, "levelIcon").name, "brightness")
        }
    }

    function test_channel_label_distinguishes_the_two_level_channels() {
        compare(makeLevel("audio-volume", 40, false).channelLabel, "Volume")
        compare(makeLevel("screen-brightness", 40, false).channelLabel, "Brightness")
        const channelLabel = findChild(makeLevel("screen-brightness", 40, false), "channelLabel")
        compare(channelLabel.text, "Brightness")
        compare(channelLabel.textFormat, Text.PlainText)
    }

    // REQ-F-022: the readout is a percentage, replaced wholesale by "Muted" rather than annotated.
    function test_value_readout_switches_to_muted() {
        const audible = makeLevel("audio-volume", 42, false)
        compare(audible.valueText, "42%")
        compare(findChild(audible, "valueLabel").text, "42%")
        compare(findChild(audible, "valueLabel").textFormat, Text.PlainText)

        const muted = makeLevel("audio-volume", 42, true)
        compare(muted.valueText, "Muted")
        compare(findChild(muted, "valueLabel").text, "Muted")
    }

    // REQ-F-023: muting dims the bar but must not collapse it — the fill still reports the real volume,
    // so unmuting does not look like a jump from zero.
    function test_mute_dims_the_fill_without_moving_it() {
        const audible = makeLevel("audio-volume", 40, false)
        const muted = makeLevel("audio-volume", 40, true)

        const audibleFill = findChild(audible, "barFill")
        const mutedFill = findChild(muted, "barFill")
        tryVerify(function () {
            return audibleFill.width > 0 && mutedFill.width > 0
        })
        compare(mutedFill.width, audibleFill.width)

        compare(audible.resolvedFillColor, HoloniightPalette.accentCyan)
        compare(muted.resolvedFillColor, HoloniightPalette.textDisabled)
        compare(mutedFill.color, HoloniightPalette.textDisabled)
    }

    // Out-of-range percentages are clamped at the fill, not at the source, so a misbehaving channel
    // cannot draw the bar past its track or at a negative width.
    function test_fill_width_is_clamped_to_the_track() {
        const full = makeLevel("audio-volume", 100, false)
        const overshoot = makeLevel("audio-volume", 150, false)
        const negative = makeLevel("audio-volume", -10, false)

        const fullTrack = findChild(full, "barTrack")
        tryVerify(function () {
            return fullTrack.width > 0
        })

        compare(findChild(overshoot, "barFill").width, findChild(full, "barFill").width)
        compare(findChild(negative, "barFill").width, 0)
    }

    // REQ-NF-003: a same-channel update slides the bar. A snap would emit valueChanged once; the
    // Behavior drives the property through intermediate frames on its way to the target.
    function test_value_changes_animate_rather_than_snapping() {
        const item = makeLevel("audio-volume", 0, false)

        valueSpy.target = item
        valueSpy.clear()

        item.value = 100
        tryCompare(item, "value", 100)
        verify(valueSpy.count > 1)
    }

    // REQ-F-017/NF-003: an update arriving mid-slide retargets the running animation instead of
    // restarting it from the old value.
    function test_mid_flight_update_retargets_the_running_animation() {
        const item = makeLevel("audio-volume", 0, false)

        item.value = 100
        tryVerify(function () {
            return item.value > 0 && item.value < 100
        })

        item.value = 40
        // Still mid-flight, never snapped back to the starting point.
        verify(item.value > 0)
        tryCompare(item, "value", 40)
    }
}

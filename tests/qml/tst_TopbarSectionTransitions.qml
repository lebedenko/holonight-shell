import QtQuick
import QtTest
import Holonight.Core
import HolonightShell

TestCase {
    name: "TopbarSectionTransitions"

    Component {
        id: weatherSectionComponent

        WeatherSection {
            barMonitorName: "TEST-1"
        }
    }

    Component {
        id: audioWidgetComponent

        AudioWidget {
            barMonitorName: "TEST-1"
        }
    }

    Component {
        id: batteryWidgetComponent

        BatteryWidget {
            barMonitorName: "TEST-1"
        }
    }

    Component {
        id: keyboardWidgetComponent

        KeyboardLayoutWidget {
            barMonitorName: "TEST-1"
        }
    }

    Component {
        id: networkWidgetComponent

        NetworkWidget {
            barMonitorName: "TEST-1"
        }
    }

    function init() {
        TopbarTestSeed.reset()
    }

    function verifyAnimatedExit(widget, makeUnavailable) {
        tryVerify(function() { return widget.implicitWidth > 0 && widget.visible && widget.enabled })

        makeUnavailable()

        compare(widget.enabled, false)
        verify(widget.visible)
        verify(widget.implicitWidth > 0)
        tryVerify(function() { return widget.implicitWidth === 0 && !widget.visible })
    }

    function test_weather_retains_loaded_widget_during_exit() {
        compare(WeatherService.hasData, true)
        const weather = createTemporaryObject(weatherSectionComponent, null)
        verify(weather)
        tryVerify(function() { return weather.implicitWidth > 0 && weather.visible })

        TopbarTestSeed.setWeatherHasData(false)

        compare(weather.enabled, false)
        verify(weather.visible)
        verify(weather.implicitWidth > 0)
        tryVerify(function() { return weather.implicitWidth === 0 && !weather.visible })
    }

    function test_audio_exit_disables_interaction_before_width_reaches_zero() {
        const audio = createTemporaryObject(audioWidgetComponent, null)
        verify(audio)
        verifyAnimatedExit(audio, function() { TopbarTestSeed.setAudioAvailable(false) })
    }

    function test_battery_exit_disables_interaction_before_width_reaches_zero() {
        const battery = createTemporaryObject(batteryWidgetComponent, null)
        verify(battery)
        verifyAnimatedExit(battery, function() { TopbarTestSeed.setBatteryPresent(false) })
    }

    function test_keyboard_exit_disables_interaction_before_width_reaches_zero() {
        const keyboard = createTemporaryObject(keyboardWidgetComponent, null)
        verify(keyboard)
        verifyAnimatedExit(keyboard, function() { TopbarTestSeed.setKeyboardLayoutCode("") })
    }

    function test_audio_reappearance_reverses_from_its_exit_width() {
        const audio = createTemporaryObject(audioWidgetComponent, null)
        verify(audio)
        tryVerify(function() { return audio.implicitWidth > 0 && audio.enabled })
        const expandedWidth = audio.implicitWidth

        TopbarTestSeed.setAudioAvailable(false)
        tryVerify(function() { return audio.implicitWidth > 0 && audio.implicitWidth < expandedWidth })
        const exitWidth = audio.implicitWidth

        TopbarTestSeed.setAudioAvailable(true)
        tryVerify(function() { return audio.implicitWidth > exitWidth })
        tryCompare(audio, "implicitWidth", expandedWidth)
    }

    function test_network_stays_present_when_unavailable() {
        const network = createTemporaryObject(networkWidgetComponent, null)
        verify(network)
        const width = network.implicitWidth

        TopbarTestSeed.setNetworkAvailable(false)

        compare(network.visible, true)
        compare(network.enabled, true)
        compare(network.implicitWidth, width)
        compare(network.primaryIconName, "wifi_offline")
    }

    function test_network_hover_outline_remains_subordinate_to_tooltip() {
        const network = createTemporaryObject(networkWidgetComponent, null)
        verify(network)
        compare(network.hoverOutlineOpacity, 0.088)
        compare(network.hoverShadowOpacity, 0.23)
    }

    function test_weather_temperature_coloring_thresholds() {
        const weather = createTemporaryObject(weatherSectionComponent, null)
        verify(weather)
        tryVerify(function() { return weather.implicitWidth > 0 && weather.visible })

        // 1. Cold temperature (<= 5°C): should return accentCyan
        TopbarTestSeed.setWeatherTemperature(-2)
        compare(weather.temperatureColor(-2), HoloniightPalette.accentCyan)
        compare(weather.temperatureColor(5), HoloniightPalette.accentCyan)

        // 2. Hot temperature (>= 30°C): should return warning
        TopbarTestSeed.setWeatherTemperature(32)
        compare(weather.temperatureColor(32), HoloniightPalette.warning)
        compare(weather.temperatureColor(30), HoloniightPalette.warning)

        // 3. Normal temperature (between 5°C and 30°C): should return textPrimary
        TopbarTestSeed.setWeatherTemperature(20)
        compare(weather.temperatureColor(20), HoloniightPalette.textPrimary)
    }
}

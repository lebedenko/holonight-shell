import QtQuick
import QtTest
import HolonightShell

TestCase {
    id: root
    name: "WeatherPreferences"
    width: 1200
    height: 1200
    visible: true
    when: windowShown

    Component { id: barComponent; WeatherSection { barMonitorName: "TEST-1" } }
    Component { id: popupComponent; WeatherPopupContent { width: 1000; height: 1200 } }

    function init() {
        WeatherService.resetHourlyData()
        WeatherService.setEmptyForecasts(false)
        WeatherService.setAvailableFields(["temperature", "humidity", "visibility", "pressure", "uvi", "windSpeed", "windGust", "windDirection", "aqi", "timeUpdated", "feelsLike", "condition", "conditionId", "sunrise", "sunset"])
        TopbarTestSeed.setWeatherHasData(true)
        WeatherService.setCurrentTemperature(18)
        WeatherService.setLocationLabel("Lviv, Ukraine")
    }

    function cleanup() {
        WeatherService.showInBar = true
        WeatherService.compactMode = false
        WeatherService.showFeelsLike = true
        WeatherService.showLocation = true
        WeatherService.temperatureUnit = "°C"
        WeatherService.windUnit = "km/h"
        WeatherService.pressureUnit = "hPa"
        WeatherService.attribution = "OpenWeatherMap"
    }

    function test_bar_visibility_compact_size_and_tooltip() {
        const bar = createTemporaryObject(barComponent, root)
        verify(bar)
        tryVerify(() => bar.implicitWidth > 0)
        wait(250)
        const fullWidth = bar.implicitWidth
        const condition = findChild(bar, "weatherBarCondition")
        verify(condition.visible)
        const tooltip = findChild(bar, "weatherTooltip")
        verify(tooltip.description.includes("Lviv"))
        verify(tooltip.description.includes("Feels like"))
        WeatherService.compactMode = true
        tryCompare(condition, "visible", false)
        tryVerify(() => bar.implicitWidth < fullWidth)
        WeatherService.showFeelsLike = false
        WeatherService.showLocation = false
        verify(!tooltip.description.includes("Feels like"))
        verify(!tooltip.description.includes("Lviv"))
        WeatherService.showInBar = false
        compare(bar.enabled, false)
        tryCompare(bar, "implicitWidth", 0)
        compare(bar.visible, false)
    }

    function test_popup_switches_units_graph_and_attribution_are_reactive() {
        const popup = createTemporaryObject(popupComponent, root)
        verify(popup)
        const location = findChild(popup, "weatherLocationLabel")
        const feels = findChild(popup, "weatherFeelsLike")
        verify(location.visible)
        verify(feels.visible)
        WeatherService.showLocation = false
        WeatherService.showFeelsLike = false
        compare(location.visible, false)
        compare(feels.visible, false)
        const temp = findChild(popup, "weatherTemperature")
        const maximum = findChild(popup, "weatherDailyMax1")
        const graph = findChild(popup, "weatherTemperatureGraph")
        const originalPoint = graph.points[0].temperature
        WeatherService.temperatureUnit = "°F"
        compare(temp.text, "64")
        verify(maximum.text.endsWith("°F"))
        compare(graph.points[0].temperature, originalPoint * 1.8 + 32)
        WeatherService.pressureUnit = "bar"
        compare(findChild(popup, "weatherPressure").value, "1.026 bar")
        WeatherService.attribution = "Open-Meteo (CC BY 4.0)"
        verify(findChild(popup, "weatherAttribution").text.includes("Open-Meteo"))
    }

    function test_unavailable_values_hide_and_restore_at_both_widths() {
        for (const width of [1000, 400]) {
            const popup = createTemporaryObject(popupComponent, root, {width: width})
            verify(popup)
            WeatherService.setAvailableFields(["windGust"])
            WeatherService.setEmptyForecasts(true)
            compare(findChild(popup, "weatherFeelsLike").visible, false)
            compare(findChild(popup, "weatherPressure").visible, false)
            compare(findChild(popup, "weatherWindSpeed").visible, false)
            compare(findChild(popup, "weatherWindDirection").visible, false)
            compare(findChild(popup, "weatherWindGust").visible, true)
            compare(findChild(popup, "weatherSunrise").visible, false)
            compare(findChild(popup, "weatherTemperatureGraph").hasSamples, false)
            const hiddenHeight = popup.implicitHeight
            WeatherService.setAvailableFields(["temperature", "feelsLike", "pressure", "sunrise"])
            WeatherService.setEmptyForecasts(false)
            tryCompare(findChild(popup, "weatherFeelsLike"), "visible", true)
            tryCompare(findChild(popup, "weatherPressure"), "visible", true)
            tryVerify(() => popup.implicitHeight > hiddenHeight)
            popup.destroy()
        }
    }

    function test_graphs_preserve_gaps_and_zero_samples() {
        const popup = createTemporaryObject(popupComponent, root)
        const graph = findChild(popup, "weatherTemperatureGraph")
        WeatherService.setHourlyData([
            {timestamp: 3600, temperature: 0, precipitation: 0, availableFields: ["temperature", "precipitation"]},
            {timestamp: 7200, temperature: 0, precipitation: 0, availableFields: []},
            {timestamp: 10800, temperature: 2, precipitation: 0, availableFields: ["temperature"]}
        ])
        const precipitation = findChild(popup, "weatherPrecipitationGraph")
        compare(precipitation.hasSamples, true)
        compare(precipitation.completeSamples, false)
        compare(graph.points.length, 3)
        compare(graph.points[1].timestamp, 7200)
        compare(graph.points[1].valid, false)
        compare(graph.points[0].valid, true)
        compare(graph.hasSamples, true)
        WeatherService.setHourlyData([{timestamp: 3600, temperature: 0, availableFields: ["temperature"]}])
        compare(graph.hasSamples, false)
    }
}

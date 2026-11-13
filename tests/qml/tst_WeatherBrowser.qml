import QtQuick
import QtTest
import HolonightShell
import Holonight.Core

TestCase {
    id: root
    name: "WeatherBrowser"
    width: 1100
    height: 1200
    visible: true
    when: windowShown
    Component {
        id: popupComponent
        Rectangle {
            width: 1000
            height: 1100
            color: HoloniightPalette.surfaceRaised
            WeatherPopupContent { anchors.fill: parent; anchors.margins: 16 }
        }
    }
    Component { id: heroComponent; WeatherCurrentSection { width: 480; height: implicitHeight } }

    function cleanup() {
        TopbarTestSeed.setWeatherHasData(true)
        WeatherService.setCondition(800)
    }

    function test_startup_and_navigation() {
        const popup = createTemporaryObject(popupComponent, root)
        verify(popup)
        const hero = findChild(popup, "weatherCurrentSection")
        verify(hero)
        const button = findChild(popup, "weatherTestButton")
        if (!ShellRuntime.troubleshootEnabled) {
            compare(button, null)
            compare(findChild(popup, "weatherNextCondition"), null)
            return
        }
        verify(button)
        compare(button.checked, false)
        tryVerify(() => findChild(hero, "weatherPreviewCaption") === null)
        waitForRendering(popup)
        const liveIcon = findChild(hero, "weatherHeroIcon")
        const originalWidth = liveIcon.width
        const originalHeight = liveIcon.height
        const originalX = liveIcon.x
        const originalY = liveIcon.y
        const originalHeroHeight = hero.height
        button.checked = true
        tryVerify(() => findChild(popup, "weatherNextCondition") !== null)
        waitForRendering(hero)
        compare(liveIcon.width, originalWidth)
        compare(liveIcon.height, originalHeight)
        compare(liveIcon.x, originalX)
        compare(liveIcon.y, originalY)
        compare(hero.height, originalHeroHeight)
        const icon = findChild(hero, "weatherHeroIcon")
        const caption = findChild(hero, "weatherPreviewCaption")
        compare(icon.provider, "openweather")
        compare(icon.conditionCode, 200)
        verify(caption.text.includes("OpenWeather"))
        verify(caption.text.includes("thunderstorm with light rain"))
        const previous = findChild(popup, "weatherPreviousCondition")
        const next = findChild(popup, "weatherNextCondition")
        mouseClick(previous)
        compare(icon.provider, "openmeteo")
        compare(icon.conditionCode, 99)
        mouseClick(next)
        compare(icon.conditionCode, 200)
        const clearDay = WeatherIconBridge.previewCatalogue.findIndex(entry => entry.provider === "openweather" && entry.code === 800 && entry.isDay)
        verify(clearDay > 0)
        hero.previewIndex = clearDay
        verify(icon.source.toString().endsWith("sunny.png"))
        next.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(icon.conditionCode, 800)
        tryCompare(icon, "isDay", false)
        verify(icon.source.toString().endsWith("clear-night.png"))
        hero.previewIndex = 0
        WeatherService.setCondition(762)
        compare(icon.conditionCode, 200)
        button.checked = false
        tryCompare(icon, "conditionCode", 762)
        tryCompare(hero, "testing", false)
        tryVerify(() => findChild(hero, "weatherPreviewCaption") === null)
        button.checked = true
        compare(icon.conditionCode, 200)
    }

    Component { id: daylightComponent; WeatherDaylight { visible: false } }
    function test_daylight_crosses_solar_boundaries_and_falls_back() {
        const fields = Array.from(WeatherService.current.availableFields)
        WeatherService.setAvailableFields(["sunrise", "sunset", "isDay"])
        WeatherService.setSunTimes(100, 200)
        const daylight = createTemporaryObject(daylightComponent, root)
        verify(daylight)
        compare(daylight.available, true)
        daylight.nowSeconds = 99
        compare(daylight.isDay, false)
        daylight.nowSeconds = 100
        compare(daylight.isDay, true)
        daylight.nowSeconds = 200
        compare(daylight.isDay, false)
        WeatherService.setSunTimes(0, 0)
        compare(daylight.isDay, true)
        WeatherService.setAvailableFields([])
        compare(daylight.available, false)
        WeatherService.setSunTimes(0, 4102444800)
        WeatherService.setAvailableFields(fields)
    }

    function savePopup(popup, path, dpr) {
        let saved = false
        verify(popup.grabToImage(result => {
            verify(result.saveToFile(path))
            saved = true
        }, Qt.size(popup.width * dpr, popup.height * dpr)))
        tryVerify(() => saved)
    }

    function test_capture_popup() {
        const popup = createTemporaryObject(popupComponent, root)
        verify(popup)
        waitForRendering(popup)
        const normalImage = controlsEvidence.captureItem(popup)
        const captureDpr = normalImage.width / popup.width
        console.info("Weather capture DPR:", captureDpr)
        const suffix = Math.round(captureDpr)
        savePopup(popup, controlsEvidence.capturePath("weather-normal-dpr" + suffix + ".png"), captureDpr)
        if (!ShellRuntime.troubleshootEnabled) return
        const button = findChild(popup, "weatherTestButton")
        button.checked = true
        tryVerify(() => findChild(popup, "weatherPreviewCaption") !== null)
        waitForRendering(popup)
        savePopup(popup, controlsEvidence.capturePath("weather-test-dpr" + suffix + ".png"), captureDpr)
    }

    function test_unavailable_and_constrained_layout() {
        TopbarTestSeed.setWeatherHasData(false)
        const popup = createTemporaryObject(popupComponent, root, {width: 300})
        const hero = findChild(popup, "weatherCurrentSection")
        verify(hero)
        const icon = findChild(hero, "weatherHeroIcon")
        compare(icon.conditionCode, -1)
        if (!ShellRuntime.troubleshootEnabled) return
        const button = findChild(popup, "weatherTestButton")
        button.checked = true
        tryVerify(() => findChild(hero, "weatherPreviewCaption") !== null)
        waitForRendering(hero)
        const caption = findChild(hero, "weatherPreviewCaption")
        verify(caption.height > 0)
        verify(icon.width >= 0)
        const next = findChild(popup, "weatherNextCondition")
        verify(next.mapToItem(popup, 0, next.height).y <= hero.mapToItem(popup, 0, 0).y)
        verify(caption.y + caption.height <= hero.height)
        button.checked = false
        compare(icon.conditionCode, -1)
    }
}

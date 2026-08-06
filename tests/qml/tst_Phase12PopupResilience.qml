import QtQuick
import QtTest
import HolonightShell

TestCase {
    name: "Phase12PopupResilience"

    Component {
        id: brightnessComponent

        BrightnessSlider {
            width: 320
        }
    }

    Component {
        id: weatherComponent

        WeatherPopupContent {
            width: 760
            height: 300
        }
    }

    Component {
        id: roomyWeatherComponent

        WeatherPopupContent {
            width: 760
            height: 1000
        }
    }

    function test_brightness_continuous_drag_is_throttled_and_commit_is_immediate() {
        BrightnessService.resetWrites()
        const slider = createTemporaryObject(brightnessComponent, null)
        verify(slider)
        const control = findChild(slider, "brightnessControl")
        verify(control)

        control.valueChanging(25)
        wait(25)
        control.valueChanging(60)
        tryVerify(function() { return BrightnessService.writeCount === 1 }, 200)
        compare(BrightnessService.lastWritten, 60)

        control.valueCommitted(75)
        compare(BrightnessService.writeCount, 2)
        compare(BrightnessService.lastWritten, 75)

        wait(120)
        compare(BrightnessService.writeCount, 2)
    }

    function test_weather_content_scrolls_within_a_bounded_viewport() {
        const weather = createTemporaryObject(weatherComponent, null)
        verify(weather)

        const viewport = findChild(weather, "weatherViewport")
        verify(viewport)
        tryVerify(function() { return viewport.contentHeight > viewport.height })
        verify(viewport.clip)
        verify(viewport.interactive)

        viewport.contentY = viewport.contentHeight - viewport.height
        verify(viewport.contentY > 0)
    }

    function test_weather_content_does_not_scroll_when_preferred_frame_fits() {
        const weather = createTemporaryObject(roomyWeatherComponent, null)
        verify(weather)

        const viewport = findChild(weather, "weatherViewport")
        verify(viewport)
        tryVerify(function() { return viewport.contentHeight <= viewport.height })
        verify(!viewport.interactive)
    }

    function test_weather_location_subtitle_tracks_service_label() {
        WeatherService.setLocationLabel("Lviv, Ukraine")
        const weather = createTemporaryObject(roomyWeatherComponent, null)
        verify(weather)
        const location = findChild(weather, "weatherLocationLabel")
        verify(location)
        compare(location.text, "Lviv, Ukraine")
        verify(location.visible)

        WeatherService.setLocationLabel("")
        compare(location.text, "")
        verify(!location.visible)
        WeatherService.setLocationLabel("Lviv, Ukraine")
    }
}

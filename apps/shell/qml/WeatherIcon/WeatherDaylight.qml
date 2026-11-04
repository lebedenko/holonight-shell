import QtQuick
import HolonightShell

Item {
    id: root
    property double nowSeconds: Date.now() / 1000
    readonly property bool solarAvailable: WeatherService.current.availableFields.includes("sunrise")
        && WeatherService.current.availableFields.includes("sunset")
        && WeatherService.current.sunrise > 0 && WeatherService.current.sunset > WeatherService.current.sunrise
    readonly property bool available: solarAvailable || WeatherService.current.availableFields.includes("isDay")
    readonly property bool isDay: solarAvailable
        ? nowSeconds >= WeatherService.current.sunrise && nowSeconds < WeatherService.current.sunset
        : WeatherService.current.isDay
    Timer {
        interval: 1000
        repeat: true
        running: root.visible
        triggeredOnStart: true
        onTriggered: root.nowSeconds = Date.now() / 1000
    }
}

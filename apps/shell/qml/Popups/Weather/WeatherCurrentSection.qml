pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell
import Holonight.Core
import Holonight.Controls

import "../../WeatherIcon"

Item {
    id: root

    readonly property color dividerColor: HoloniightPalette.borderPassive
    readonly property bool isDay: WeatherService.hasData
        && (Date.now() / 1000) >= WeatherService.current.sunrise
        && (Date.now() / 1000) < WeatherService.current.sunset

    function temperatureColor(temperature) {
        if (temperature <= 5) {
            return HoloniightPalette.accentCyan
        }
        if (temperature >= 30) {
            return HoloniightPalette.warning
        }
        return HoloniightPalette.textPrimary
    }

    Row {
        id: layoutRow
        anchors.fill: parent
        spacing: 16

        Item {
            width: 240
            height: parent.height

            WeatherIconCompositor {
                id: heroIcon
                conditionCode: WeatherService.hasData ? WeatherService.current.conditionId : 0
                conditionDescription: WeatherService.hasData ? WeatherService.current.condition : ""
                windSpeedKmh: WeatherService.hasData ? WeatherService.current.windSpeed : 0
                isDay: root.isDay
                iconSize: 220
                moonPhase: (WeatherService.hasData && WeatherService.daily.length > 0) ? WeatherService.daily[0].moonPhase : undefined
                anchors.centerIn: parent
            }
        }

        Item {
            width: 280
            height: parent.height

            Column {
                spacing: 6

                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -22

                Row {
                    spacing: 6

                    Text {
                        id: tempText
                        text: WeatherService.hasData ? Math.round(WeatherService.current.temperature) : ""
                        color: WeatherService.hasData ? root.temperatureColor(WeatherService.current.temperature) : HoloniightPalette.textPrimary
                        font.pointSize: 84
                        font.family: AppearanceService.displayFont
                        font.weight: Font.Thin
                    }

                    Text {
                        id: tempUnitText
                        text: WeatherService.hasData ? "°C" : ""
                        color: tempText.color
                        font.pointSize: 42
                        font.family: AppearanceService.displayFont
                        font.weight: Font.Thin

                        anchors.top: tempText.top
                        anchors.topMargin: 8
                    }
                }

                Text {
                    text: WeatherService.hasData ? WeatherService.current.condition.toUpperCase() : ""
                    color: HoloniightPalette.accentViolet
                    font.pointSize: 12
                    font.family: AppearanceService.uiFont
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }

                Item {
                    width: parent.width
                    height: 12

                    HnSeparator {
                        orientation: Qt.Horizontal
                        width: parent.width
                        fadeMode: HnSeparator.FadeEnd
                        opacity: 0.5
                    }
                }

                Text {
                    text: WeatherService.hasData
                        ? "Feels like " + Math.round(WeatherService.current.feelsLike) + "°C"
                        : ""
                    color: HoloniightPalette.textPrimary
                    font.pointSize: 9
                    font.family: AppearanceService.uiFont
                    font.weight: Font.Light
                }
            }
        }
    }
}

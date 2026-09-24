pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell
import Holonight.Core
import Holonight.Controls

import "../../WeatherIcon"

Item {
    id: root
    implicitHeight: Math.max(256, currentContent.implicitHeight + 44)

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
            id: iconColumn
            width: Math.min(240, Math.max(0, (root.width - layoutRow.spacing) * 0.46))
            height: parent.height

            WeatherIconCompositor {
                id: heroIcon
                conditionCode: WeatherService.hasData ? WeatherService.current.conditionId : 0
                conditionDescription: WeatherService.hasData ? WeatherService.current.condition : ""
                windSpeedKmh: WeatherService.hasData ? WeatherService.current.windSpeed : 0
                isDay: root.isDay
                iconSize: Math.min(220, parent.width, parent.height)
                moonPhase: (WeatherService.hasData && WeatherService.daily.length > 0) ? WeatherService.daily[0].moonPhase : undefined
                anchors.centerIn: parent
            }
        }

        Item {
            width: Math.max(0, root.width - iconColumn.width - layoutRow.spacing)
            height: parent.height

            Column {
                id: currentContent
                width: parent.width
                spacing: 6

                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -22

                Row {
                    id: temperatureRow
                    width: parent.width
                    spacing: 6

                    Text {
                        id: tempText
                        width: Math.max(0, temperatureRow.width - tempUnitText.implicitWidth - temperatureRow.spacing)
                        text: WeatherService.hasData ? Math.round(WeatherService.current.temperature) : ""
                        color: WeatherService.hasData ? root.temperatureColor(WeatherService.current.temperature) : HoloniightPalette.textPrimary
                        font.pointSize: AppearanceService.displayFontSize * 3.5
                        font.family: AppearanceService.displayFont
                        font.weight: Font.Thin
                        elide: Text.ElideRight
                    }

                    Text {
                        id: tempUnitText
                        text: WeatherService.hasData ? "°C" : ""
                        color: tempText.color
                        font.pointSize: AppearanceService.displayFontSize * 1.75
                        font.family: AppearanceService.displayFont
                        font.weight: Font.Thin

                        anchors.top: tempText.top
                        anchors.topMargin: 8
                    }
                }

                HnLabel {
                    width: parent.width
                    rawText: WeatherService.hasData ? WeatherService.current.condition : ""
                    role: HnTypographyRole.MicroHeader
                    color: HoloniightPalette.accentViolet
                    font.family: AppearanceService.uiFont
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

                HnLabel {
                    width: parent.width
                    rawText: WeatherService.hasData
                        ? "Feels like " + Math.round(WeatherService.current.feelsLike) + "°C"
                        : ""
                    role: HnTypographyRole.Caption
                    color: HoloniightPalette.textPrimary
                    elide: Text.ElideRight
                }
            }
        }
    }
}

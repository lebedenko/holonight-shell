pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell
import Holonight.Core
import "../../Topbar"

import "../../WeatherIcon"

Column {
    id: root
    spacing: 6

    function getDailyEntryForTimestamp(timestamp) {
        var hourlyDate = new Date(timestamp * 1000)
        var hYear = hourlyDate.getFullYear()
        var hMonth = hourlyDate.getMonth()
        var hDay = hourlyDate.getDate()

        var daily = WeatherService.daily
        for (var i = 0; i < daily.length; ++i) {
            var d = daily[i]
            if (!d) {
                continue
            }
            var dailyDate = new Date(d.date * 1000)
            if (dailyDate.getFullYear() === hYear &&
                dailyDate.getMonth() === hMonth &&
                dailyDate.getDate() === hDay) {
                return d
            }
        }
        return null
    }

    WeatherIconRoles {
        id: iconRoles
    }

    ListView {
        id: strip
        width: parent.width
        height: 144
        orientation: ListView.Horizontal
        clip: true
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        spacing: 8
        model: WeatherService.hourly

        delegate: Rectangle {
            id: card
            required property var modelData  // HourlyEntry Q_GADGET
            required property int index
            readonly property bool isCurrent: card.index === 0
            width: 108
            height: strip.height
            radius: 4
            color: {
                if (card.isCurrent) {
                    return Qt.rgba(HoloniightPalette.accentBlue.r, HoloniightPalette.accentBlue.g,
                                   HoloniightPalette.accentBlue.b, 0.08)
                }
                if (hoverHandler.hovered) {
                    return Qt.rgba(HoloniightPalette.textPrimary.r, HoloniightPalette.textPrimary.g,
                                   HoloniightPalette.textPrimary.b, 0.03)
                }
                return "transparent"
            }
            border.width: 1
            border.color: {
                if (card.isCurrent) {
                    return Qt.rgba(HoloniightPalette.accentBlue.r, HoloniightPalette.accentBlue.g,
                                   HoloniightPalette.accentBlue.b, 0.48)
                }
                if (hoverHandler.hovered) {
                    return Qt.rgba(HoloniightPalette.accentBlue.r, HoloniightPalette.accentBlue.g,
                                   HoloniightPalette.accentBlue.b, 0.20)
                }
                return Qt.rgba(HoloniightPalette.borderPassive.r, HoloniightPalette.borderPassive.g,
                               HoloniightPalette.borderPassive.b, 0.08)
            }

            HoverHandler {
                id: hoverHandler
            }

            readonly property bool isDay: {
                if (card.isCurrent) {
                    return WeatherService.hasData
                        && (Date.now() / 1000) >= WeatherService.current.sunrise
                        && (Date.now() / 1000) < WeatherService.current.sunset
                }
                var daily = WeatherService.daily
                var current = WeatherService.current
                var entry = root.getDailyEntryForTimestamp(card.modelData.timestamp)
                if (entry && entry.sunrise && entry.sunset) {
                    return card.modelData.timestamp >= entry.sunrise && card.modelData.timestamp < entry.sunset
                }
                return card.modelData.timestamp >= WeatherService.current.sunrise
                    && card.modelData.timestamp < WeatherService.current.sunset
            }

            Column {
                anchors.fill: parent
                anchors.topMargin: 8
                anchors.bottomMargin: 8
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 4

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: card.isCurrent
                        ? qsTr("Now")
                        : Qt.formatTime(new Date(card.modelData.timestamp * 1000), "HH:mm")
                    color: HoloniightPalette.textPrimary
                    font.pixelSize: 14
                    font.family: AppearanceService.uiFont
                }

                WeatherIconCompositor {
                    anchors.horizontalCenter: parent.horizontalCenter
                    conditionCode: card.modelData.conditionId
                    conditionDescription: card.modelData.condition
                    windSpeedKmh: card.isCurrent && WeatherService.hasData ? WeatherService.current.windSpeed : 0
                    isDay: card.isDay
                    iconSize: 64
                    date: new Date(card.modelData.timestamp * 1000)
                    moonPhase: {
                        var entry = root.getDailyEntryForTimestamp(card.modelData.timestamp)
                        return entry ? entry.moonPhase : ((WeatherService.daily.length > 0) ? WeatherService.daily[0].moonPhase : undefined)
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Math.round(card.modelData.temperature) + "°C"
                    color: HoloniightPalette.textPrimary
                    font.pixelSize: 16
                    font.family: AppearanceService.uiFont
                    font.weight: Font.DemiBold
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 2

                    // qmllint disable import unresolved-type
                    HnIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        size: 12
                        source: "qrc:/HolonightShell/weather-ui/precipitation.svg"
                        normalColor: HoloniightPalette.accentBlue
                    }
                    // qmllint enable import unresolved-type

                    Text {
                        text: Math.round(card.modelData.pop * 100) + "%"
                        color: HoloniightPalette.accentBlue
                        font.pixelSize: 10
                        font.family: AppearanceService.uiFont
                    }
                }
            }
        }
    }
}

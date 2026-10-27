pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell
import Holonight.Core
import Holonight.Controls
import "../../Topbar"

import "../../WeatherIcon"

Column {
    id: root
    readonly property var entries: WeatherService.hourly.filter(entry => entry.availableFields.some(field => ["temperature", "pop", "conditionId", "condition"].includes(field)))
    visible: entries.length > 0
    spacing: 6
    readonly property int cardWidth: 108
    readonly property real naturalWidth: 6 * cardWidth + 5 * strip.spacing

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
        objectName: "weatherHourlyList"
        width: parent.width
        height: Math.max(144, 64 + AppearanceService.uiFontSize * 7)
        orientation: ListView.Horizontal
        clip: true
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        spacing: 8
        model: root.entries

        delegate: Rectangle {
            id: card
            required property var modelData  // HourlyEntry Q_GADGET
            required property int index
            readonly property bool isCurrent: WeatherService.hourly.length > 0 && card.modelData.timestamp === WeatherService.hourly[0].timestamp
            width: root.cardWidth
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
                if (card.isCurrent && WeatherService.current.availableFields.includes("sunrise") && WeatherService.current.availableFields.includes("sunset")) {
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
                return String(card.modelData.iconCode || "").endsWith("n") ? false : true
            }

            Column {
                anchors.fill: parent
                anchors.topMargin: 8
                anchors.bottomMargin: 8
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 4

                HnLabel {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: card.width - 16
                    rawText: card.isCurrent
                        ? qsTr("Now")
                        : Qt.formatTime(new Date(card.modelData.timestamp * 1000), "HH:mm")
                    role: HnTypographyRole.Caption
                    color: HoloniightPalette.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
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
                        return entry && entry.availableFields.includes("moonPhase") ? entry.moonPhase : undefined
                    }
                }

                HnLabel {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: card.width - 16
                    visible: card.modelData.availableFields.includes("temperature")
                    rawText: WeatherService.formatTemperature(card.modelData.temperature, WeatherService.temperatureUnit)
                    role: HnTypographyRole.Subheading
                    color: HoloniightPalette.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }

                Row {
                    visible: card.modelData.availableFields.includes("pop")
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

                    HnLabel {
                        rawText: Math.round(card.modelData.pop * 100) + "%"
                        role: HnTypographyRole.Caption
                        color: HoloniightPalette.accentBlue
                    }
                }
            }
        }
    }
}

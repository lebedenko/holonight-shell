pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell
import Holonight.Core
import Holonight.Controls

import "../../WeatherIcon"

Column {
    id: root
    spacing: 8

    readonly property bool isCurrentDay: WeatherService.hasData
        && (Date.now() / 1000) >= WeatherService.current.sunrise
        && (Date.now() / 1000) < WeatherService.current.sunset

    readonly property double remainingDayPop: {
        if (!WeatherService.hasData || WeatherService.hourly.length === 0) {
            return 0
        }
        var todayDate = new Date()
        var tYear = todayDate.getFullYear()
        var tMonth = todayDate.getMonth()
        var tDay = todayDate.getDate()

        var maxPop = -1
        var hourly = WeatherService.hourly
        for (var i = 0; i < hourly.length; ++i) {
            var h = hourly[i]
            if (!h) continue
            var hDate = new Date(h.timestamp * 1000)
            if (hDate.getFullYear() === tYear &&
                hDate.getMonth() === tMonth &&
                hDate.getDate() === tDay) {
                if (h.pop > maxPop) {
                    maxPop = h.pop
                }
            }
        }
        if (maxPop === -1) {
            return hourly[0].pop
        }
        return maxPop
    }

    Row {
        width: parent.width
        height: Math.max(160, AppearanceService.uiFontSize * 11)
        spacing: 0

        Repeater {
            model: Math.max(0, Math.min(WeatherService.daily ? WeatherService.daily.length : 0, 5))

            delegate: Item {
                id: dayCard
                required property int index
                readonly property var entry: WeatherService.daily[dayCard.index]
                width: root.width / 5
                height: parent.height

                HnSeparator {
                    orientation: Qt.Vertical
                    height: parent.height
                    fadeMode: HnSeparator.FadeBoth
                    opacity: 0.5
                    visible: dayCard.index > 0
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 6

                    HnLabel {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: dayCard.width
                        rawText: dayCard.index === 0
                            ? (root.isCurrentDay ? "Today" : "Tonight")
                            : (dayCard.index === 1
                                ? "Tomorrow"
                                : Qt.formatDate(new Date(dayCard.entry.date * 1000), "ddd"))
                        role: HnTypographyRole.Caption
                        color: HoloniightPalette.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }

                    WeatherIconCompositor {
                        anchors.horizontalCenter: parent.horizontalCenter
                        conditionCode: dayCard.index === 0
                            ? (WeatherService.hasData ? WeatherService.current.conditionId : 0)
                            : dayCard.entry.conditionId
                        conditionDescription: dayCard.index === 0
                            ? (WeatherService.hasData ? WeatherService.current.condition : "")
                            : dayCard.entry.condition
                        windSpeedKmh: dayCard.index === 0 && WeatherService.hasData ? WeatherService.current.windSpeed : 0
                        isDay: dayCard.index === 0 ? root.isCurrentDay : true
                        iconSize: 64
                        date: dayCard.index === 0 ? new Date() : new Date(dayCard.entry.date * 1000)
                        moonPhase: (WeatherService.hasData && WeatherService.daily.length > 0)
                            ? WeatherService.daily[0].moonPhase
                            : undefined
                    }

                    Item {
                        width: dayCard.width
                        height: Math.max(36, currentTemperature.implicitHeight, dailyTemperatures.implicitHeight)
                        anchors.horizontalCenter: parent.horizontalCenter

                        HnLabel {
                            id: currentTemperature
                            anchors.centerIn: parent
                            width: parent.width
                            visible: dayCard.index === 0
                            rawText: (dayCard.index === 0 && WeatherService.hasData)
                                ? Math.round(WeatherService.current.temperature) + "°C"
                                : ""
                            role: HnTypographyRole.Subheading
                            color: HoloniightPalette.textPrimary
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }

                        Column {
                            id: dailyTemperatures
                            anchors.fill: parent
                            visible: dayCard.index > 0
                            spacing: 2

                            HnLabel {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: dayCard.width
                                rawText: dayCard.index > 0 ? Math.round(dayCard.entry.tempMax) + "°C" : ""
                                role: HnTypographyRole.Body
                                color: HoloniightPalette.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }

                            HnLabel {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: dayCard.width
                                rawText: dayCard.index > 0 ? Math.round(dayCard.entry.tempMin) + "°C" : ""
                                role: HnTypographyRole.Caption
                                color: HoloniightPalette.textMuted
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                                opacity: 0.8
                            }
                        }
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

                        HnLabel {
                            rawText: dayCard.index === 0
                                ? Math.round(root.remainingDayPop * 100) + "%"
                                : Math.round(dayCard.entry.pop * 100) + "%"
                            role: HnTypographyRole.Caption
                            color: HoloniightPalette.accentBlue
                        }
                    }
                }
            }
        }
    }
}

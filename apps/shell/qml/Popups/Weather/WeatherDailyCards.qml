pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell
import Holonight.Core
import Holonight.Controls

import "../../WeatherIcon"

Column {
    id: root
    readonly property var entries: WeatherService.daily.filter(entry => entry.availableFields.some(field => ["tempMin", "tempMax", "pop", "conditionId", "condition"].includes(field)))
    visible: entries.length > 0
    spacing: 8

    readonly property bool isCurrentDay: !WeatherService.current.availableFields.includes("sunrise") || !WeatherService.current.availableFields.includes("sunset")
        ? !String(WeatherService.current.iconCode || "").endsWith("n") : WeatherService.hasData
        && (Date.now() / 1000) >= WeatherService.current.sunrise
        && (Date.now() / 1000) < WeatherService.current.sunset

    readonly property var remainingDayPop: {
        if (!WeatherService.hasData || WeatherService.hourly.length === 0) {
            return undefined
        }
        var todayDate = new Date()
        var tYear = todayDate.getFullYear()
        var tMonth = todayDate.getMonth()
        var tDay = todayDate.getDate()

        var maxPop = -1
        var hourly = WeatherService.hourly
        for (var i = 0; i < hourly.length; ++i) {
            var h = hourly[i]
            if (!h || !h.availableFields.includes("pop")) continue
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
            return undefined
        }
        return maxPop
    }

    Row {
        width: parent.width
        height: Math.max(160, AppearanceService.uiFontSize * 11)
        spacing: 0

        Repeater {
            model: Math.min(root.entries.length, 5)

            delegate: Item {
                id: dayCard
                required property int index
                readonly property var entry: root.entries[dayCard.index]
                readonly property int sourceIndex: WeatherService.daily.findIndex(value => value.date === dayCard.entry.date)
                width: root.width / Math.min(root.entries.length, 5)
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
                        rawText: dayCard.sourceIndex === 0
                            ? (root.isCurrentDay ? "Today" : "Tonight")
                            : (dayCard.sourceIndex === 1
                                ? "Tomorrow"
                                : Qt.formatDate(new Date(dayCard.entry.date * 1000), "ddd"))
                        role: HnTypographyRole.Caption
                        color: HoloniightPalette.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }

                    WeatherIconCompositor {
                        anchors.horizontalCenter: parent.horizontalCenter
                        conditionCode: dayCard.sourceIndex === 0
                            ? (WeatherService.hasData ? WeatherService.current.conditionId : 0)
                            : dayCard.entry.conditionId
                        conditionDescription: dayCard.sourceIndex === 0
                            ? (WeatherService.hasData ? WeatherService.current.condition : "")
                            : dayCard.entry.condition
                        windSpeedKmh: dayCard.sourceIndex === 0 && WeatherService.hasData ? WeatherService.current.windSpeed : 0
                        isDay: dayCard.sourceIndex === 0 ? root.isCurrentDay : true
                        iconSize: 64
                        date: dayCard.sourceIndex === 0 ? new Date() : new Date(dayCard.entry.date * 1000)
                        moonPhase: (WeatherService.hasData && WeatherService.daily.length > 0 && WeatherService.daily[0].availableFields.includes("moonPhase"))
                            ? WeatherService.daily[0].moonPhase
                            : undefined
                    }

                    Item {
                        width: dayCard.width
                        visible: (dayCard.sourceIndex === 0 && WeatherService.current.availableFields.includes("temperature")) || dayCard.entry.availableFields.includes("tempMin") || dayCard.entry.availableFields.includes("tempMax")
                        height: Math.max(36, currentTemperature.implicitHeight, dailyTemperatures.implicitHeight)
                        anchors.horizontalCenter: parent.horizontalCenter

                        HnLabel {
                            id: currentTemperature
                            anchors.centerIn: parent
                            width: parent.width
                            visible: dayCard.sourceIndex === 0 && WeatherService.current.availableFields.includes("temperature")
                            rawText: (dayCard.sourceIndex === 0 && WeatherService.hasData)
                                ? WeatherService.formatTemperature(WeatherService.current.temperature, WeatherService.temperatureUnit)
                                : ""
                            role: HnTypographyRole.Subheading
                            color: HoloniightPalette.textPrimary
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }

                        Column {
                            id: dailyTemperatures
                            anchors.fill: parent
                            visible: dayCard.sourceIndex > 0 || !WeatherService.current.availableFields.includes("temperature")
                            spacing: 2

                            HnLabel {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: dayCard.width
                                objectName: "weatherDailyMax" + dayCard.index
                                visible: dayCard.entry.availableFields.includes("tempMax")
                                rawText: WeatherService.formatTemperature(dayCard.entry.tempMax, WeatherService.temperatureUnit)
                                role: HnTypographyRole.Body
                                color: HoloniightPalette.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }

                            HnLabel {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: dayCard.width
                                visible: dayCard.entry.availableFields.includes("tempMin")
                                rawText: WeatherService.formatTemperature(dayCard.entry.tempMin, WeatherService.temperatureUnit)
                                role: HnTypographyRole.Caption
                                color: HoloniightPalette.textMuted
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                                opacity: 0.8
                            }
                        }
                    }

                    Row {
                        visible: dayCard.entry.availableFields.includes("pop") || (dayCard.sourceIndex === 0 && root.remainingDayPop !== undefined)
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
                            rawText: dayCard.sourceIndex === 0 && root.remainingDayPop !== undefined
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

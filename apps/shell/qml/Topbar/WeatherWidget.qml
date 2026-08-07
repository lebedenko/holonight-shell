import QtQuick
import HolonightShell
import Holonight.Core

import "../Controls"

BarSection {
    id: root

    required property string barMonitorName
    readonly property int iconSize: iconRoles.topbar
    readonly property bool ready: WeatherService.hasData

    readonly property int slantCut: 12
    readonly property int contentLeftMargin: 24 + root.slantCut
    readonly property int contentRightMargin: 24 + root.slantCut
    readonly property int inheritedSectionPadding: 8

    readonly property bool isDay: {
        if (!root.ready) {
            return true
        }
        const nowSecs = Date.now() / 1000
        return nowSecs >= WeatherService.current.sunrise && nowSecs < WeatherService.current.sunset
    }

    implicitWidth: root.ready
        ? (root.contentLeftMargin + contentRow.implicitWidth + root.contentRightMargin)
        : 0

    Behavior on implicitWidth {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    WeatherIconRoles {
        id: iconRoles
    }

    BarFrame {

        anchors {
            fill: parent
            leftMargin: -root.inheritedSectionPadding
            rightMargin: -root.inheritedSectionPadding
        }
        leftBottomOffset: root.slantCut
        rightTopOffset: root.slantCut
        visible: root.ready
    }

    Row {
        id: contentRow

        anchors {
            left: parent.left
            leftMargin: root.contentLeftMargin - root.inheritedSectionPadding
            verticalCenter: parent.verticalCenter
        }
        spacing: 6

        // qmllint disable import unresolved-type
        HnIcon {
            anchors.verticalCenter: parent.verticalCenter
            size: root.iconSize
            tinted: true
            normalColor: root.ready ? root.temperatureColor(WeatherService.current.temperature) : HoloniightPalette.textPrimary
            source: root.ready ? WeatherService.iconPath(WeatherService.current.conditionId, root.isDay) : ""
            opacity: hoverHandler.hovered ? 1.0 : 0.92

            Behavior on opacity {
                NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
            }
        }
        // qmllint enable import unresolved-type

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(tempText.implicitWidth, condText.width)

            Text {
                id: tempText
                text: root.ready ? Math.round(WeatherService.current.temperature) + "°C" : ""
                color: root.ready ? root.temperatureColor(WeatherService.current.temperature) : HoloniightPalette.textPrimary
                font.family: AppearanceService.displayFont
                font.pixelSize: AppearanceService.displayFontSize
                font.weight: Font.Normal
            }

            Text {
                id: condText
                text: root.ready ? root.shortCondition(WeatherService.current.condition).toUpperCase() : ""
                color: HoloniightPalette.accentViolet
                opacity: 0.6
                font.pixelSize: 10
                font.letterSpacing: 0.5
                elide: Text.ElideRight
                maximumLineCount: 1
                width: Math.min(Math.max(condText.implicitWidth, tempText.implicitWidth), 90)
            }
        }
    }

    HoverHandler { id: hoverHandler }

    BarTooltipArea {
        barMonitorName: root.barMonitorName
        title: root.ready ? root.capitalize(WeatherService.current.condition) : ""
        description: root.ready
            ? ("Feels like " + Math.round(WeatherService.current.feelsLike) + "°\nClick for forecast"
               + (WeatherService.stale ? "\nStale — last update over 1h ago" : ""))
            : ""
        iconName: ""
    }

    StatusPopupTriggerArea {
        popupId: "weather"
        barMonitorName: root.barMonitorName
    }

    function temperatureColor(temperature) {
        if (temperature <= 5) {
            return HoloniightPalette.accentCyan
        }
        if (temperature >= 30) {
            return HoloniightPalette.warning
        }
        return HoloniightPalette.textPrimary
    }

    function capitalize(text: string): string {
        return text.length > 0 ? text.charAt(0).toUpperCase() + text.slice(1) : text
    }

    function shortCondition(text: string): string {
        if (!text) return ""
        const lower = text.toLowerCase().trim()

        if (lower === "clear sky") return "Clear Sky"
        if (lower === "few clouds") return "Few Clouds"
        if (lower === "scattered clouds") return "Partly Cloudy"
        if (lower === "broken clouds") return "Broken Clouds"
        if (lower === "overcast clouds") return "Overcast"

        if (lower.includes("thunderstorm")) return "Thunderstorm"

        if (lower.includes("drizzle rain") || lower.includes("shower rain and drizzle")) return "Drizzle Rain"
        if (lower.includes("drizzle")) return "Drizzle"

        if (lower.includes("heavy intensity rain") || lower.includes("very heavy rain") || lower.includes("extreme rain")) return "Heavy Rain"
        if (lower.includes("freezing rain")) return "Freezing Rain"
        if (lower.includes("shower rain") || lower.includes("showers")) return "Showers"
        if (lower.includes("light rain")) return "Light Rain"
        if (lower.includes("moderate rain") || lower.includes("rain")) return "Rain"

        if (lower.includes("heavy snow")) return "Heavy Snow"
        if (lower.includes("rain and snow") || lower.includes("rain & snow")) return "Rain & Snow"
        if (lower.includes("shower snow")) return "Snow Showers"
        if (lower.includes("sleet")) return "Sleet"
        if (lower.includes("light snow")) return "Light Snow"
        if (lower.includes("snow")) return "Snow"

        if (lower.includes("sand") || lower.includes("dust")) return "Dust & Sand"
        if (lower.includes("volcanic ash")) return "Volcanic Ash"
        if (lower.includes("squalls")) return "Squalls"
        if (lower.includes("tornado")) return "Tornado"
        if (lower.includes("hail")) return "Hail"
        if (lower.includes("hurricane")) return "Hurricane"
        if (lower === "mist") return "Mist"
        if (lower === "smoke") return "Smoke"
        if (lower === "haze") return "Haze"
        if (lower === "fog") return "Fog"

        return text
    }
}

pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell
import Holonight.Core

Column {
    id: root
    spacing: 8

    component DetailCell: Item {
        id: cell
        property string label: ""
        property string value: ""
        property string iconName: ""
        width: root.width
        height: 22

        Canvas {
            id: iconCanvas
            x: 0
            y: 1
            width: 20
            height: 20
            antialiasing: true

            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.scale(width / 24, height / 24)
                const gradient = ctx.createLinearGradient(4, 4, 20, 20)
                gradient.addColorStop(0, HoloniightPalette.accentCyan)
                gradient.addColorStop(1, HoloniightPalette.accentViolet)
                ctx.strokeStyle = gradient
                ctx.fillStyle = gradient
                ctx.lineWidth = 1.7

                if (cell.iconName === "humidity") {
                    ctx.beginPath()
                    ctx.moveTo(12, 4)
                    ctx.bezierCurveTo(8, 9, 6, 12, 6, 15)
                    ctx.bezierCurveTo(6, 19, 8.8, 21, 12, 21)
                    ctx.bezierCurveTo(15.2, 21, 18, 19, 18, 15)
                    ctx.bezierCurveTo(18, 12, 16, 9, 12, 4)
                    ctx.stroke()
                    return
                }

                if (cell.iconName === "visibility") {
                    ctx.beginPath()
                    ctx.moveTo(3.5, 12)
                    ctx.bezierCurveTo(7, 7, 17, 7, 20.5, 12)
                    ctx.bezierCurveTo(17, 17, 7, 17, 3.5, 12)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.arc(12, 12, 3, 0, Math.PI * 2)
                    ctx.stroke()
                    return
                }

                if (cell.iconName === "pressure") {
                    ctx.beginPath()
                    ctx.arc(12, 12, 7.5, Math.PI * 0.72, Math.PI * 2.28)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(12, 12)
                    ctx.lineTo(16.5, 9)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(8, 18)
                    ctx.lineTo(16, 18)
                    ctx.stroke()
                    return
                }

                ctx.beginPath()
                ctx.arc(12, 12, 4.2, 0, Math.PI * 2)
                ctx.stroke()
                for (let i = 0; i < 8; ++i) {
                    const angle = i * Math.PI / 4
                    ctx.beginPath()
                    ctx.moveTo(12 + Math.cos(angle) * 7, 12 + Math.sin(angle) * 7)
                    ctx.lineTo(12 + Math.cos(angle) * 9.5, 12 + Math.sin(angle) * 9.5)
                    ctx.stroke()
                }
            }
        }

        Text {
            x: 30
            y: 3
            width: 90
            text: cell.label
            color: HoloniightPalette.textPrimary
            font.pixelSize: 13
            font.family: AppearanceService.uiFont
        }

        Text {
            anchors.right: parent.right
            y: 3
            width: 90
            text: cell.value
            color: HoloniightPalette.textMuted
            font.pixelSize: 13
            font.family: AppearanceService.uiFont
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
        }

        onIconNameChanged: iconCanvas.requestPaint()
        Component.onCompleted: iconCanvas.requestPaint()
    }

    DetailCell {
        iconName: "humidity"
        label: "Humidity"
        value: WeatherService.hasData ? WeatherService.current.humidity + "%" : "—"
    }
    DetailCell {
        iconName: "visibility"
        label: "Visibility"
        value: WeatherService.hasData ? WeatherService.current.visibility.toFixed(0) + " km" : "—"
    }
    DetailCell {
        iconName: "pressure"
        label: "Pressure"
        value: WeatherService.hasData ? WeatherService.current.pressure + " hPa" : "—"
    }
    DetailCell {
        iconName: "uv"
        label: "UV Index"
        value: {
            if (!WeatherService.hasData) return "—"
            const uvi = WeatherService.current.uvi
            if (uvi < 3) return uvi.toFixed(0) + " (Low)"
            if (uvi < 6) return uvi.toFixed(0) + " (Mod)"
            if (uvi < 8) return uvi.toFixed(0) + " (High)"
            return uvi.toFixed(0) + " (Very High)"
        }
    }

    WeatherWindWidget {
        width: parent.width
        height: 126
        hasData: WeatherService.hasData
        speedKmh: WeatherService.hasData ? WeatherService.current.windSpeed : 0
        gustKmh: WeatherService.hasData ? WeatherService.current.windGust : 0
        directionDeg: WeatherService.hasData ? WeatherService.current.windDirection : 0
    }
}

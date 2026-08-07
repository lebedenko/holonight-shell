import QtQuick
import HolonightShell
import Holonight.Core

Canvas {
    id: root
    antialiasing: true

    readonly property int displaySeconds: 24 * 60 * 60

    function canvasFont(pixelSize, family) {
        const escapedFamily = String(family).replace(/\\/g, "\\\\").replace(/'/g, "\\'")
        return pixelSize + "px '" + escapedFamily + "'"
    }

    readonly property var points: {
        const list = WeatherService.hourly
        const result = []
        if (list.length === 0) {
            return result
        }

        const startTime = list[0].timestamp
        const endTime = startTime + root.displaySeconds
        for (let idx = 0; idx < list.length; idx++) {
            if (list[idx].timestamp >= endTime) {
                break
            }
            result.push({
                temperature: list[idx].temperature,
                timestamp: list[idx].timestamp,
            })
        }
        return result
    }

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()

        const pts = root.points
        if (pts.length < 2) {
            return
        }

        const wid = root.width
        const hgt = root.height
        const leftPad = 34
        const rightPad = 8
        const topPad = 12
        const bottomPad = 24
        const plotW = wid - leftPad - rightPad
        const plotH = hgt - topPad - bottomPad

        const temps = pts.map(point => point.temperature)
        const minT = Math.floor(Math.min.apply(null, temps))
        const maxT = Math.ceil(Math.max.apply(null, temps))
        const range = Math.max(1, maxT - minT)
        const midT = Math.round((minT + maxT) / 2)
        const startTime = pts[0].timestamp

        function xPos(timestamp) { return leftPad + ((timestamp - startTime) / root.displaySeconds) * plotW }
        function yPos(val) { return topPad + ((maxT - val) / range) * plotH }
        function drawTemperaturePath() {
            ctx.beginPath()
            ctx.moveTo(xPos(pts[0].timestamp), yPos(pts[0].temperature))
            for (let idx = 1; idx < pts.length; idx++) {
                ctx.lineTo(xPos(pts[idx].timestamp), yPos(pts[idx].temperature))
            }
        }

        ctx.strokeStyle = Qt.rgba(HoloniightPalette.textMuted.r,
                                  HoloniightPalette.textMuted.g,
                                  HoloniightPalette.textMuted.b, 0.12)
        ctx.lineWidth = 1
        for (const lvl of [minT, midT, maxT]) {
            const gridY = yPos(lvl)
            ctx.beginPath()
            ctx.moveTo(leftPad, gridY)
            ctx.lineTo(wid - rightPad, gridY)
            ctx.stroke()
        }
        for (let tick = 0; tick <= 6; tick++) {
            const gridX = leftPad + (tick / 6) * plotW
            ctx.beginPath()
            ctx.moveTo(gridX, topPad)
            ctx.lineTo(gridX, hgt - bottomPad)
            ctx.stroke()
        }

        ctx.shadowBlur = 12
        ctx.shadowColor = Qt.rgba(HoloniightPalette.accentCyan.r,
                                  HoloniightPalette.accentCyan.g,
                                  HoloniightPalette.accentCyan.b, 0.72)
        ctx.strokeStyle = Qt.rgba(HoloniightPalette.accentCyan.r,
                                  HoloniightPalette.accentCyan.g,
                                  HoloniightPalette.accentCyan.b, 0.36)
        ctx.lineWidth = 3.6
        ctx.lineJoin = "round"
        ctx.lineCap = "round"
        drawTemperaturePath()
        ctx.stroke()

        ctx.shadowBlur = 0
        ctx.strokeStyle = HoloniightPalette.accentCyan
        ctx.lineWidth = 1.8
        ctx.lineJoin = "round"
        ctx.lineCap = "round"
        drawTemperaturePath()
        ctx.stroke()

        ctx.fillStyle = HoloniightPalette.accentCyan
        for (let idx = 0; idx < pts.length; idx += Math.max(1, Math.floor(pts.length / 12))) {
            ctx.beginPath()
            ctx.arc(xPos(pts[idx].timestamp), yPos(pts[idx].temperature), 2, 0, Math.PI * 2)
            ctx.fill()
        }

        // Y-axis labels at min / mid / max.
        ctx.fillStyle = HoloniightPalette.textMuted
        ctx.font = root.canvasFont(9, AppearanceService.monospaceFont)
        ctx.textAlign = "left"
        ctx.fillText(maxT + "°", 0, yPos(maxT) + 4)
        ctx.fillText(midT + "°", 0, yPos(midT) + 4)
        ctx.fillText(minT + "°", 0, yPos(minT) + 4)

        ctx.textAlign = "center"
        for (let tick = 0; tick <= 6; tick++) {
            const timestamp = startTime + (tick / 6) * root.displaySeconds
            ctx.fillText(Qt.formatTime(new Date(timestamp * 1000), "HH:mm"),
                         xPos(timestamp), hgt - 5)
        }
    }

    Connections {
        target: WeatherService
        function onForecastChanged() { root.requestPaint() }
    }

    Connections {
        target: AppearanceService
        function onFixedFontChanged() { root.requestPaint() }
    }

    Component.onCompleted: root.requestPaint()
}

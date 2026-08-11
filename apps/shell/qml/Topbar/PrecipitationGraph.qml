import QtQuick
import HolonightShell
import Holonight.Core

// Violet bar graph of hourly precipitation with grid, axis labels, and time labels.
// Repaints on forecastChanged. (REQ-F-017)
Canvas {
    id: root
    antialiasing: true

    readonly property int displaySeconds: 24 * 60 * 60

    function canvasFont(fontSize, family, fallbackFamily) {
        const escapedFamily = String(family).replace(/\\/g, "\\\\").replace(/'/g, "\\'")
        return fontSize + "px '" + escapedFamily + "', " + fallbackFamily
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
                precipitation: list[idx].precipitation,
                timestamp: list[idx].timestamp,
            })
        }
        return result
    }

    // CLAUDE.md Canvas gotcha: qualify every property access with root inside onPaint.
    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()

        const pts = root.points
        if (pts.length === 0) {
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

        const vals = pts.map(point => point.precipitation)
        const maxP = Math.max.apply(null, vals)
        const hasPrecipitation = maxP > 0
        const axisMax = Math.max(1, Math.ceil(maxP))
        const slot = plotW / 24
        const barW = Math.max(2, slot - 2)
        const startTime = pts[0].timestamp

        function xPos(timestamp) { return leftPad + ((timestamp - startTime) / root.displaySeconds) * plotW }
        function yPos(val) { return topPad + ((axisMax - val) / axisMax) * plotH }

        ctx.strokeStyle = Qt.rgba(HoloniightPalette.textMuted.r,
                                  HoloniightPalette.textMuted.g,
                                  HoloniightPalette.textMuted.b, 0.12)
        ctx.lineWidth = 1
        for (const lvl of [0, axisMax / 2, axisMax]) {
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

        if (!hasPrecipitation) {
            ctx.fillStyle = Qt.rgba(HoloniightPalette.textMuted.r,
                                    HoloniightPalette.textMuted.g,
                                    HoloniightPalette.textMuted.b, 0.48)
            ctx.font = root.canvasFont(12, AppearanceService.uiFont, "sans-serif")
            ctx.textAlign = "center"
            ctx.fillText("No precipitation expected", leftPad + plotW / 2, topPad + plotH / 2 + 4)
        }

        const barGradient = ctx.createLinearGradient(0, topPad, 0, hgt - bottomPad)
        barGradient.addColorStop(0, HoloniightPalette.accentViolet)
        barGradient.addColorStop(1, Qt.rgba(HoloniightPalette.accentViolet.r,
                                            HoloniightPalette.accentViolet.g,
                                            HoloniightPalette.accentViolet.b, 0.34))

        ctx.fillStyle = barGradient
        if (hasPrecipitation) {
            for (let idx = 0; idx < vals.length; idx++) {
                if (vals[idx] <= 0) {
                    continue
                }
                const barH = Math.max(2, (vals[idx] / axisMax) * plotH)
                const barX = xPos(pts[idx].timestamp) + (slot - barW) / 2
                const barY = hgt - bottomPad - barH
                ctx.fillRect(barX, barY, barW, barH)
            }
        }

        ctx.fillStyle = HoloniightPalette.textMuted
        ctx.font = root.canvasFont(9, AppearanceService.monospaceFont, "monospace")
        ctx.textAlign = "left"
        ctx.fillText(axisMax.toFixed(0), 0, yPos(axisMax) + 4)
        ctx.fillText((axisMax / 2).toFixed(axisMax > 5 ? 0 : 1), 0, yPos(axisMax / 2) + 4)
        ctx.fillText("0", 0, yPos(0) + 4)

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
        function onUiFontChanged() { root.requestPaint() }
        function onMonospaceFontChanged() { root.requestPaint() }
    }

    Component.onCompleted: root.requestPaint()
}

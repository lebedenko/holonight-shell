import QtQuick
import Holonight.Core

Canvas {
    id: root

    property int signalStrength: 100
    property color activeColor: HoloniightPalette.accentCyan
    property color mutedColor: HoloniightPalette.borderPassive

    width: 18
    height: 16

    onSignalStrengthChanged: requestPaint()
    onActiveColorChanged: requestPaint()
    onMutedColorChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()

        const activeBars = Math.max(0, Math.min(4, Math.ceil(root.signalStrength / 25)))
        const barW = 3
        const gap = 2
        const barHeights = [4, 7, 10, 14]
        const canvasH = root.height

        for (let idx = 0; idx < 4; idx++) {
            const barX = idx * (barW + gap)
            const barH = barHeights[idx]
            const barY = canvasH - barH
            const isActive = idx < activeBars

            ctx.fillStyle = isActive ? root.activeColor : root.mutedColor
            ctx.globalAlpha = isActive ? 1.0 : 0.22
            ctx.fillRect(barX, barY, barW, barH)
        }
        ctx.globalAlpha = 1.0
    }
}

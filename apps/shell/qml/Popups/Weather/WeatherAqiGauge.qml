import QtQuick
import HolonightShell
import Holonight.Core

Item {
    id: root

    property int aqi: 0
    property bool showValue: true

    readonly property color trackColor: Qt.rgba(HoloniightPalette.borderPassive.r,
                                                HoloniightPalette.borderPassive.g,
                                                HoloniightPalette.borderPassive.b, 0.32)
    readonly property color accentColor: colorForAqi(root.aqi)

    function clamp(value, minValue, maxValue) {
        return Math.max(minValue, Math.min(maxValue, value))
    }

    function colorForAqi(value) {
        if (value <= 0) {
            return HoloniightPalette.textSecondary
        }
        if (value === 1) {
            return HoloniightPalette.accentCyan
        }
        if (value === 2) {
            return HoloniightPalette.success
        }
        if (value === 3) {
            return HoloniightPalette.accentYellow
        }
        if (value === 4) {
            return HoloniightPalette.warning
        }
        return HoloniightPalette.error
    }

    function labelForAqi(value) {
        const labels = ["Not available", "Excellent", "Good", "Moderate", "Poor", "Hazardous"]
        return value >= 1 && value <= 5 ? labels[value] : labels[0]
    }

    Canvas {
        id: gaugeCanvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()

            const side = Math.min(root.width, root.height)
            if (side <= 0) {
                return
            }

            const centerX = root.width / 2
            const centerY = root.height / 2
            const lineWidth = Math.max(2, side * 0.11)
            const radius = side / 2 - lineWidth / 2 - 1
            const startAngle = Math.PI * 0.72
            const sweep = Math.PI * 1.56
            const value = root.clamp(root.aqi, 0, 5)

            ctx.lineCap = "round"
            ctx.lineWidth = lineWidth

            ctx.beginPath()
            ctx.arc(centerX, centerY, radius, startAngle, startAngle + sweep)
            ctx.strokeStyle = root.trackColor
            ctx.stroke()

            if (value > 0) {
                ctx.beginPath()
                ctx.arc(centerX, centerY, radius, startAngle, startAngle + sweep * (value / 5))
                ctx.strokeStyle = root.accentColor
                ctx.stroke()
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.showValue
        text: root.aqi >= 1 && root.aqi <= 5 ? root.aqi : "—"
        color: root.accentColor
        font.family: AppearanceService.monospaceFont
        font.pixelSize: Math.max(9, Math.round(Math.min(root.width, root.height) * 0.3))
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    onAqiChanged: gaugeCanvas.requestPaint()
    onWidthChanged: gaugeCanvas.requestPaint()
    onHeightChanged: gaugeCanvas.requestPaint()
    onTrackColorChanged: gaugeCanvas.requestPaint()
    onAccentColorChanged: gaugeCanvas.requestPaint()
    Component.onCompleted: gaugeCanvas.requestPaint()
}

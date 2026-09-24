pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import HolonightShell
import Holonight.Core

Item {
    id: root

    property int speedKmh: 0
    property int gustKmh: 0
    property int directionDeg: 0
    property bool hasData: false

    readonly property int normalizedDirection: ((root.directionDeg % 360) + 360) % 360
    readonly property string directionCode: directionName(root.normalizedDirection, true)
    readonly property string directionLabel: directionName(root.normalizedDirection, false)
    implicitHeight: Math.max(126, textColumn.implicitHeight + 8)

    function directionName(degrees, compact) {
        const codes = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
        const labels = ["North", "North-East", "East", "South-East", "South", "South-West", "West", "North-West"]
        const index = Math.floor((degrees + 22.5) / 45) % 8
        return compact ? codes[index] : labels[index]
    }

    RowLayout {
        anchors.fill: parent
        spacing: 12

        Item {
            Layout.preferredWidth: 112
            Layout.fillHeight: true

            // qmllint disable import unresolved-type
            HnIcon {
                anchors.centerIn: parent
                size: 112
                source: "qrc:/HolonightShell/weather-ui/wind-compass.svg"
                normalColor: HoloniightPalette.borderPassive
            }

            HnIcon {
                anchors.centerIn: parent
                size: 112
                source: "qrc:/HolonightShell/weather-ui/wind-compass-labels.svg"
                normalColor: HoloniightPalette.textPrimary
            }

            HnIcon {
                anchors.centerIn: parent
                size: 112
                source: "qrc:/HolonightShell/weather-ui/wind-compass-accent.svg"
                normalColor: HoloniightPalette.accentCyan
            }
            // qmllint enable import unresolved-type

            Canvas {
                id: arrowCanvas
                anchors.centerIn: parent
                width: 112
                height: 112
                antialiasing: true
                rotation: root.normalizedDirection

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.translate(width / 2, height / 2)

                    const gradient = ctx.createLinearGradient(0, -40, 0, 8)
                    gradient.addColorStop(0, HoloniightPalette.accentCyan)
                    gradient.addColorStop(1, HoloniightPalette.accentBlue)

                    ctx.shadowBlur = 10
                    ctx.shadowColor = HoloniightPalette.accentBlue
                    ctx.strokeStyle = gradient
                    ctx.fillStyle = gradient
                    ctx.lineWidth = 2.2
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"

                    ctx.beginPath()
                    ctx.moveTo(0, 0)
                    ctx.lineTo(0, -38)
                    ctx.stroke()

                    ctx.beginPath()
                    ctx.moveTo(0, -44)
                    ctx.lineTo(-8, -25)
                    ctx.lineTo(0, -31)
                    ctx.lineTo(8, -25)
                    ctx.closePath()
                    ctx.fill()

                    ctx.beginPath()
                    ctx.arc(0, 0, 6, 0, Math.PI * 2)
                    ctx.stroke()
                }
            }
        }

        Column {
            id: textColumn
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            Row {
                spacing: 4
                Text {
                    id: speedText
                    text: root.hasData ? root.speedKmh : "—"
                    color: HoloniightPalette.textPrimary
                    font.family: AppearanceService.displayFont
                    font.pointSize: AppearanceService.displayFontSize * 1.3125
                    font.weight: Font.Thin
                }
                HnLabel {
                    anchors.baseline: speedText.baseline
                    rawText: root.hasData ? "km/h" : ""
                    role: HnTypographyRole.Caption
                    color: HoloniightPalette.textMuted
                }
            }

            HnLabel {
                rawText: root.hasData ? root.directionCode : "—"
                role: HnTypographyRole.Subheading
                color: HoloniightPalette.accentViolet
                font.weight: Font.Medium
            }

            HnLabel {
                width: parent.width
                rawText: root.hasData ? "From " + root.directionLabel : ""
                role: HnTypographyRole.Caption
                color: HoloniightPalette.textMuted
                elide: Text.ElideRight
            }

            HnLabel {
                width: parent.width
                rawText: root.hasData && root.gustKmh > 0 ? "gusts " + root.gustKmh + " km/h" : ""
                role: HnTypographyRole.Caption
                color: HoloniightPalette.textSecondary
                elide: Text.ElideRight
            }
        }
    }

    onDirectionDegChanged: arrowCanvas.requestPaint()
    Component.onCompleted: arrowCanvas.requestPaint()
}

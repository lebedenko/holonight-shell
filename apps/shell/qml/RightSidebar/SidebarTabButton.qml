import QtQuick
import Holonight.Core
import Holonight.Controls

import HolonightShell

HnNavigationDelegate {
    id: root

    objectName: "sidebarNavigationDelegate"
    property int tabIndex: 0
    property string iconName: ""
    property string label: ""
    property bool isActive: false
    property int badgeCount: 0
    readonly property color iconColor: root.isActive
                                        ? HoloniightPalette.selectionIndicator
                                        : HoloniightPalette.textSecondary

    height: 48
    title: root.label
    badgeText: root.badgeCount > 0 ? root.badgeCount.toString() : ""
    checkable: true
    checked: root.isActive
    leadingContent: Component {
        Canvas {
            id: iconCanvas

            width: 24
            height: 24

            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.scale(24 / 48, 24 / 48) // Scale the 48x48 icon definitions to fit 24x24
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.strokeStyle = root.iconColor
                ctx.lineWidth = 3
                root.drawIcon(ctx, root.iconName)
            }

            Connections {
                target: root
                function onIsActiveChanged() { iconCanvas.requestPaint() }
                function onIconNameChanged() { iconCanvas.requestPaint() }
            }
        }
    }

    function drawIcon(ctx, name) {
        if (name === "overview") {
            // Grid of 4 squares
            ctx.beginPath()
            ctx.rect(6, 6, 14, 14)
            ctx.rect(28, 6, 14, 14)
            ctx.rect(6, 28, 14, 14)
            ctx.rect(28, 28, 14, 14)
            ctx.stroke()
        } else if (name === "calendar") {
            // Calendar outline with header line
            ctx.beginPath()
            ctx.moveTo(10, 8)
            ctx.lineTo(38, 8)
            ctx.arcTo(42, 8, 42, 12, 4)
            ctx.lineTo(42, 38)
            ctx.arcTo(42, 42, 38, 42, 4)
            ctx.lineTo(10, 42)
            ctx.arcTo(6, 42, 6, 38, 4)
            ctx.lineTo(6, 12)
            ctx.arcTo(6, 8, 10, 8, 4)
            ctx.closePath()
            ctx.moveTo(6, 18)
            ctx.lineTo(42, 18)
            ctx.moveTo(14, 4)
            ctx.lineTo(14, 12)
            ctx.moveTo(34, 4)
            ctx.lineTo(34, 12)
            ctx.stroke()
            // Day dots
            ctx.fillStyle = ctx.strokeStyle
            ctx.beginPath()
            ctx.arc(14, 28, 2, 0, Math.PI * 2)
            ctx.arc(24, 28, 2, 0, Math.PI * 2)
            ctx.arc(34, 28, 2, 0, Math.PI * 2)
            ctx.fill()
        } else if (name === "notifications") {
            // Bell shape
            ctx.beginPath()
            ctx.moveTo(24, 4)
            ctx.bezierCurveTo(24, 4, 14, 10, 14, 22)
            ctx.lineTo(14, 32)
            ctx.lineTo(10, 36)
            ctx.lineTo(38, 36)
            ctx.lineTo(34, 32)
            ctx.lineTo(34, 22)
            ctx.bezierCurveTo(34, 10, 24, 4, 24, 4)
            ctx.stroke()
            // Bell bottom clapper
            ctx.beginPath()
            ctx.moveTo(20, 36)
            ctx.bezierCurveTo(20, 41, 28, 41, 28, 36)
            ctx.stroke()
        } else if (name === "system") {
            // Monitor screen
            ctx.beginPath()
            ctx.rect(6, 8, 36, 24)
            ctx.moveTo(24, 32)
            ctx.lineTo(24, 40)
            ctx.moveTo(14, 40)
            ctx.lineTo(34, 40)
            ctx.stroke()
        } else if (name === "applications") {
            // Grid of 4 small circles forming 2x2 grid
            ctx.beginPath()
            ctx.arc(14, 14, 5, 0, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(34, 14, 5, 0, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(14, 34, 5, 0, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(34, 34, 5, 0, Math.PI * 2)
            ctx.stroke()
        } else if (name === "settings") {
            // Gear/cog simplified: circle with outer tick marks
            ctx.beginPath()
            ctx.arc(24, 24, 8, 0, Math.PI * 2)
            ctx.stroke()
            const ticks = 8
            for (let i = 0; i < ticks; i++) {
                const angle = (i / ticks) * Math.PI * 2
                const inner = 12
                const outer = 18
                ctx.beginPath()
                ctx.moveTo(24 + inner * Math.cos(angle), 24 + inner * Math.sin(angle))
                ctx.lineTo(24 + outer * Math.cos(angle), 24 + outer * Math.sin(angle))
                ctx.stroke()
            }
        }
    }
}

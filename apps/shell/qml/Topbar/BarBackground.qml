import QtQuick
import Holonight.Core

Rectangle {
    id: root

    readonly property real glowOpacity: 0.18

    color: "transparent"

    Rectangle {
        anchors.fill: parent
        opacity: root.glowOpacity
        gradient: Gradient {
            orientation: Gradient.Horizontal

            GradientStop {
                position: 0.0
                color: HoloniightPalette.accentCyan
            }

            GradientStop {
                position: 1.0
                color: HoloniightPalette.accentViolet
            }
        }
    }
}

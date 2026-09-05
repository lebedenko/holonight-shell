import QtQuick
import QtQuick.Shapes
import Holonight.Core

Item {
    id: root
    property string kind: "shield"
    property int size: 24
    property color color: HoloniightPalette.accentCyan
    implicitWidth: size
    implicitHeight: size
    Accessible.ignored: true

    Shape {
        width: 24
        height: 24
        anchors.centerIn: parent
        scale: root.size / 24
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: "transparent"
            strokeColor: root.color
            strokeWidth: 1.4
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg {
                path: root.kind === "shield" ? "M12 2 L21 6 L21 12 Q21 19 12 23 Q3 19 3 12 L3 6 Z M12 10 L12 16 M10.5 9 A1.5 1.5 0 1 0 13.5 9 A1.5 1.5 0 1 0 10.5 9"
                    : root.kind === "user" ? "M8 8 A4 4 0 1 0 16 8 A4 4 0 1 0 8 8 M4 22 L4 20 A8 8 0 0 1 20 20 L20 22"
                    : root.kind === "chevron" ? "M5 9 L12 16 L19 9"
                    : root.kind === "hidden" ? "M2 12 Q12 0 22 12 Q12 24 2 12 M9 12 A3 3 0 1 0 15 12 A3 3 0 1 0 9 12 M3 3 L21 21"
                    : "M2 12 Q12 0 22 12 Q12 24 2 12 M9 12 A3 3 0 1 0 15 12 A3 3 0 1 0 9 12"
            }
        }
    }
}

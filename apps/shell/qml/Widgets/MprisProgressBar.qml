import QtQuick
import Holonight.Core

// Thin, non-interactive playback progress for the MPRIS desktop widget.
Item {
    id: root

    required property real positionUs
    required property real lengthUs
    required property bool canSeek

    visible: root.lengthUs > 0
    implicitHeight: 3

    readonly property real fraction: root.lengthUs > 0
        ? Math.min(1.0, Math.max(0.0, root.positionUs / root.lengthUs))
        : 0.0

    Rectangle {
        id: track
        objectName: "mprisProgressTrack"
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: 2
        radius: height / 2
        color: HoloniightPalette.textDisabled
        opacity: 0.28
    }

    Rectangle {
        id: fill
        objectName: "mprisProgressFill"
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width * root.fraction
        height: 3
        radius: height / 2
        opacity: root.canSeek ? 1.0 : 0.6
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { objectName: "mprisProgressCyanStop"; position: 0.0; color: HoloniightPalette.accentCyan }
            GradientStop { objectName: "mprisProgressBlueStop"; position: 0.5; color: HoloniightPalette.accentBlue }
            GradientStop { objectName: "mprisProgressVioletStop"; position: 1.0; color: HoloniightPalette.accentViolet }
        }
    }
}

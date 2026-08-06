import QtQuick
import Holonight.Core
import HolonightShell

import "../Controls"

// Fixed-layout "now playing" pill: icon, artist—title (fade-truncated, fixed width), then a
// fixed-width row of 3 control buttons (REQ-F-017, REQ-F-023). Zero width / not rendered when no
// active player exists (REQ-F-016).
BarSection {
    id: root

    required property string barMonitorName
    readonly property bool ready: MprisService.hasActivePlayer

    readonly property int slantCut: 12
    readonly property int contentLeftMargin: 20 + root.slantCut
    readonly property int contentRightMargin: 20 + root.slantCut
    readonly property int inheritedSectionPadding: 8
    readonly property int textWidth: 140

    // Playing/Paused visual distinction using only palette tokens (REQ-F-024).
    readonly property color emphasisColor: MprisService.activePlaybackStatus === "Playing"
        ? HoloniightPalette.accentCyan
        : HoloniightPalette.textMuted

    implicitWidth: root.ready
        ? (root.contentLeftMargin + contentRow.implicitWidth + root.contentRightMargin)
        : 0

    Behavior on implicitWidth {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    BarFrame {
        anchors {
            fill: parent
            leftMargin: -root.inheritedSectionPadding
            rightMargin: -root.inheritedSectionPadding
        }
        leftBottomOffset: root.slantCut
        rightTopOffset: root.slantCut
        visible: root.ready
    }

    Row {
        id: contentRow

        anchors {
            left: parent.left
            leftMargin: root.contentLeftMargin - root.inheritedSectionPadding
            verticalCenter: parent.verticalCenter
        }
        spacing: 10

        Image {
            id: appIcon
            objectName: "mprisAppIcon"
            anchors.verticalCenter: parent.verticalCenter
            width: 22
            height: 22
            sourceSize: Qt.size(width, height)
            source: root.ready && MprisService.activeDesktopEntry.length > 0
                ? "image://icon/" + MprisService.activeDesktopEntry
                : ""
            fillMode: Image.PreserveAspectFit
            asynchronous: true
        }

        // The icon and this text area have no click handlers — the only interactive elements in
        // the pill are the three control buttons (REQ-F-021).
        Item {
            id: textArea
            anchors.verticalCenter: parent.verticalCenter
            width: root.textWidth
            height: trackText.implicitHeight

            readonly property string displayText: MprisService.activeArtist.length > 0
                ? (MprisService.activeArtist + " — " + MprisService.activeTitle)
                : MprisService.activeTitle

            Text {
                id: trackText
                width: parent.width
                text: textArea.displayText
                elide: Text.ElideRight
                color: root.emphasisColor
                font.family: AppearanceService.uiFont
                font.pixelSize: AppearanceService.uiFontSize
            }

            Rectangle {
                id: textFade

                readonly property color fadeColor: HoloniightPalette.surface

                visible: trackText.truncated
                anchors {
                    top: parent.top
                    right: parent.right
                    bottom: parent.bottom
                }
                width: Math.min(28, parent.width)

                gradient: Gradient {
                    orientation: Gradient.Horizontal

                    GradientStop {
                        position: 0.0
                        color: Qt.rgba(textFade.fadeColor.r, textFade.fadeColor.g, textFade.fadeColor.b, 0.0)
                    }
                    GradientStop {
                        position: 1.0
                        color: textFade.fadeColor
                    }
                }
            }
        }

        // Fixed total width regardless of enabled state — each MprisControlButton keeps its size
        // constant and only dims via opacity, so this row never causes layout shift (REQ-F-023).
        Row {
            id: controlsRow
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            MprisControlButton {
                objectName: "mprisPreviousButton"
                glyph: "previous"
                buttonEnabled: MprisService.canControl && MprisService.canGoPrevious
                onClicked: MprisService.previous()
            }

            MprisControlButton {
                objectName: "mprisPlayPauseButton"
                glyph: "playPause"
                playing: MprisService.activePlaybackStatus === "Playing"
                buttonEnabled: MprisService.canControl
                    && (MprisService.activePlaybackStatus === "Playing" ? MprisService.canPause : MprisService.canPlay)
                onClicked: MprisService.playPause()
            }

            MprisControlButton {
                objectName: "mprisNextButton"
                glyph: "next"
                buttonEnabled: MprisService.canControl && MprisService.canGoNext
                onClicked: MprisService.next()
            }
        }
    }
}

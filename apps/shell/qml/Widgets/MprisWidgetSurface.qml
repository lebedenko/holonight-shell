import QtQuick
import QtQuick.Effects
import HolonightShell
import Holonight.Core

// Pointer-transparent MPRIS desktop surface. C++ continuously pushes the plain properties below;
// only barMonitorName is seeded through setInitialProperties.
Item {
    id: root

    focusPolicy: Qt.NoFocus

    required property string barMonitorName

    property string title: ""
    property string artist: ""
    property string album: ""
    property string identity: ""
    property string desktopEntry: ""
    property string artworkPath: ""
    property string playbackStatus: ""
    property real positionUs: 0
    property real lengthUs: 0
    property bool canSeek: false
    property bool contentVisible: true
    property bool pausedTimedOut: false

    readonly property string displayTitle: root.title.length > 0 ? root.title : root.identity
    property real presentationOpacity: 1.0

    state: !root.contentVisible ? "occupancyHidden"
        : (root.playbackStatus === "Playing" ? "playing"
        : (root.playbackStatus === "Paused" ? (root.pausedTimedOut ? "pauseTimedOut" : "paused")
        : "stopped"))
    opacity: 0.0

    states: [
        State {
            name: "playing"
            PropertyChanges { root.opacity: 1.0; root.presentationOpacity: 1.0 }
        },
        State {
            name: "paused"
            PropertyChanges { root.opacity: 1.0; root.presentationOpacity: 0.55 }
        },
        State {
            name: "pauseTimedOut"
            PropertyChanges { root.opacity: 0.0; root.presentationOpacity: 0.55 }
        },
        State {
            name: "stopped"
            PropertyChanges { root.opacity: 0.0; root.presentationOpacity: 1.0 }
        },
        State {
            name: "occupancyHidden"
            PropertyChanges { root.opacity: 0.0; root.presentationOpacity: 1.0 }
        }
    ]

    transitions: [
        Transition {
            from: "paused"; to: "pauseTimedOut"; reversible: true
            OpacityAnimator { duration: 1000; easing.type: Easing.OutCubic }
        },
        Transition {
            from: "pauseTimedOut"; to: "playing"
            OpacityAnimator { duration: 300; easing.type: Easing.OutCubic }
        }
    ]

    visible: root.opacity > 0 && root.contentVisible

    Behavior on presentationOpacity {
        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
    }

    function formatTime(us) {
        const totalSeconds = Math.max(0, Math.floor(us / 1000000));
        const hours = Math.floor(totalSeconds / 3600);
        const minutes = Math.floor((totalSeconds % 3600) / 60);
        const seconds = totalSeconds % 60;
        const mm = String(minutes).padStart(2, '0');
        const ss = String(seconds).padStart(2, '0');
        return hours > 0 ? (hours + ":" + mm + ":" + ss) : (mm + ":" + ss);
    }

    Image {
        id: washSource
        objectName: "mprisWashSource"
        x: 16
        y: 0
        width: 336
        height: 432
        visible: false
        source: artwork.effectiveSource
        sourceSize: Qt.size(96, 96)
        fillMode: Image.Stretch
        asynchronous: true
        cache: true
        layer.enabled: true
    }

    MultiEffect {
        id: ambientWash
        objectName: "mprisAmbientWash"
        anchors.fill: washSource
        visible: root.visible && washSource.status === Image.Ready
        source: washSource
        blurEnabled: true
        blur: 1.0
        blurMax: 64
        brightness: -0.32
        contrast: -0.12
        saturation: 0.18
        opacity: root.playbackStatus === "Playing" ? 0.14 : 0.07
        autoPaddingEnabled: false
    }

    MprisArtwork {
        id: artwork
        objectName: "mprisArtwork"
        x: 56
        y: 32
        width: 256
        height: 256
        artworkPath: root.artworkPath
        desktopEntry: root.desktopEntry
    }

    Item {
        id: metadataArea
        objectName: "mprisMetadataArea"
        x: 56
        y: artwork.y + artwork.height + 16
        width: 256
        height: metadataBlock.height
        opacity: root.presentationOpacity

        MultiEffect {
            objectName: "mprisMetadataShadow"
            anchors.fill: metadataBlock
            source: metadataBlock
            shadowEnabled: true
            shadowColor: HoloniightPalette.primary
            shadowBlur: 0.45
            shadowOpacity: 0.24
            shadowScale: 1.01
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 1
            autoPaddingEnabled: true
        }

        Item {
            id: metadataBlock
            objectName: "mprisMetadataBlock"
            width: parent.width
            height: timeRow.y + timeRow.height

            Item {
                id: titleRow
                objectName: "mprisTitleRow"
                width: parent.width
                height: Math.max(nowPlayingGlyph.height, titleLabel.implicitHeight)

                Image {
                    id: nowPlayingGlyph
                    objectName: "mprisNowPlayingGlyph"
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    sourceSize: Qt.size(width, height)
                    source: HnIconProvider.sourceUrl(
                                "qrc:/HolonightShell/media/now-playing-glyph.svg", width,
                                HoloniightPalette.textSecondary,
                                HoloniightPalette.accentCyan,
                                HoloniightPalette.accentBlue,
                                HoloniightPalette.accentViolet,
                                HoloniightPalette.background,
                                HoloniightPalette.revision)
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }

                Text {
                    id: titleLabel
                    objectName: "mprisTitleLabel"
                    anchors.left: nowPlayingGlyph.right
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.displayTitle
                    color: HoloniightPalette.textPrimary
                    font.family: AppearanceService.displayFont
                    font.pixelSize: Math.round(AppearanceService.displayFontSize * 1.25)
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
            }

            Text {
                id: artistLabel
                objectName: "mprisArtistLabel"
                y: titleRow.y + titleRow.height + 4
                width: parent.width
                visible: root.artist.length > 0
                text: root.artist
                color: HoloniightPalette.textSecondary
                font.family: AppearanceService.displayFont
                font.pixelSize: Math.round(AppearanceService.displayFontSize * 0.8)
                textFormat: Text.PlainText
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            MprisProgressBar {
                id: progressBar
                objectName: "mprisProgressBar"
                y: artistLabel.y + (artistLabel.visible ? artistLabel.height + 4 : 0)
                width: parent.width
                height: visible ? implicitHeight : 0
                positionUs: root.positionUs
                lengthUs: root.lengthUs
                canSeek: root.canSeek
            }

            Item {
                id: timeRow
                objectName: "mprisTimeRow"
                y: progressBar.y + progressBar.height + (progressBar.visible ? 8 : 0)
                width: parent.width
                height: Math.max(elapsedLabel.implicitHeight, durationLabel.implicitHeight)

                StableDigitsText {
                    id: elapsedLabel
                    objectName: "mprisElapsedLabel"
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    text: root.formatTime(root.positionUs)
                    color: HoloniightPalette.textDisabled
                    opacity: 0.72
                    fontFamily: AppearanceService.displayFont
                    pixelSize: Math.round(AppearanceService.displayFontSize * 0.7)
                }

                StableDigitsText {
                    id: durationLabel
                    objectName: "mprisDurationLabel"
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    visible: root.lengthUs > 0
                    text: root.formatTime(root.lengthUs)
                    color: HoloniightPalette.textDisabled
                    opacity: 0.56
                    fontFamily: AppearanceService.displayFont
                    pixelSize: Math.round(AppearanceService.displayFontSize * 0.7)
                }
            }
        }
    }
}

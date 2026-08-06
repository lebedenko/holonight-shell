import QtQuick
import QtTest
import HolonightShell
import Holonight.Core

TestCase {
    id: testCase
    name: "MprisDesktopWidget"
    when: windowShown

    Component {
        id: widgetComponent
        Window {
            width: 368
            height: 456
            visible: true
            color: "transparent"

            MprisWidgetSurface {
                objectName: "testMprisWidget"
                anchors.fill: parent
                barMonitorName: "test"
                playbackStatus: "Playing"
            }
        }
    }

    Component {
        id: artworkWindowComponent
        Window {
            width: 368
            height: 320
            visible: true
            color: "transparent"

            MprisArtwork {
                objectName: "standaloneMprisArtwork"
                x: 56
                y: 32
                width: 256
                height: 256
                artworkPath: ""
                desktopEntry: ""
            }
        }
    }

    function createWidget() {
        const window = createTemporaryObject(widgetComponent, null);
        verify(window !== null);
        const widget = findChild(window, "testMprisWidget");
        tryCompare(widget, "visible", true);
        tryCompare(widget, "width", 368);
        tryCompare(widget, "height", 456);
        return widget;
    }

    function test_twoHourTransportUsesRealPrecision() {
        const widget = createWidget();
        widget.positionUs = 7200123456;
        widget.lengthUs = 7380000000;
        compare(widget.formatTime(widget.positionUs), "2:00:00");
        compare(widget.formatTime(widget.lengthUs), "2:03:00");
        const progress = findChild(widget, "mprisProgressBar");
        verify(progress.fraction > 0.97);
        verify(progress.fraction < 0.98);
    }

    function test_surfaceAndContentShareAlignmentColumn() {
        const widget = createWidget();
        widget.title = "source-Preserved TITLE";
        widget.artist = "lower case artist";
        widget.album = "Not rendered";
        widget.lengthUs = 180000000;

        const artwork = findChild(widget, "mprisArtwork");
        const metadataArea = findChild(widget, "mprisMetadataArea");
        const titleRow = findChild(widget, "mprisTitleRow");
        const artist = findChild(widget, "mprisArtistLabel");
        const progress = findChild(widget, "mprisProgressBar");
        const timeRow = findChild(widget, "mprisTimeRow");
        const glyph = findChild(widget, "mprisNowPlayingGlyph");
        const title = findChild(widget, "mprisTitleLabel");

        compare(widget.width, 368);
        compare(widget.height, 456);
        compare(artwork.x, 56);
        compare(artwork.y, 32);
        compare(artwork.width, 256);
        compare(metadataArea.x, artwork.x);
        compare(metadataArea.width, artwork.width);
        compare(titleRow.width, 256);
        compare(artist.width, 256);
        compare(progress.width, 256);
        compare(timeRow.width, 256);
        compare(glyph.width, 16);
        compare(glyph.height, 16);
        compare(title.text, "source-Preserved TITLE");
        compare(title.elide, Text.ElideRight);
        compare(title.maximumLineCount, 1);
        compare(artist.elide, Text.ElideRight);
        compare(artist.maximumLineCount, 1);
        widget.title = "A deliberately very long title that cannot fit beside the spectrum glyph in one line";
        widget.artist = "A deliberately very long artist name that cannot fit inside the metadata column in one line";
        tryVerify(() => title.truncated);
        tryVerify(() => artist.truncated);
        compare(findChild(widget, "mprisAlbumLabel"), null);
        compare(findChild(widget, "mprisIdentityBadge"), null);
        compare(findChild(widget, "mprisPauseGlyph"), null);
    }

    function test_missingArtistAndDurationCollapseIndependently() {
        const widget = createWidget();
        widget.artist = "Artist";
        widget.lengthUs = 180000000;
        wait(0);

        const artist = findChild(widget, "mprisArtistLabel");
        const progress = findChild(widget, "mprisProgressBar");
        const elapsed = findChild(widget, "mprisElapsedLabel");
        const duration = findChild(widget, "mprisDurationLabel");
        const metadata = findChild(widget, "mprisMetadataBlock");
        const heightWithArtist = metadata.height;
        verify(artist.visible);
        verify(progress.visible);
        verify(duration.visible);

        widget.artist = "";
        verify(!artist.visible);
        tryVerify(() => metadata.height < heightWithArtist);

        widget.lengthUs = 0;
        wait(0);
        verify(!progress.visible);
        verify(!duration.visible);
        verify(elapsed.visible);
        compare(elapsed.text, "00:00");
    }

    function test_progressAndStableTimestamps() {
        const widget = createWidget();
        widget.lengthUs = 100000000;
        widget.positionUs = 99000000;
        widget.canSeek = true;

        const progress = findChild(widget, "mprisProgressBar");
        const track = findChild(widget, "mprisProgressTrack");
        const fill = findChild(widget, "mprisProgressFill");
        const timeRow = findChild(widget, "mprisTimeRow");
        const elapsed = findChild(widget, "mprisElapsedLabel");
        const duration = findChild(widget, "mprisDurationLabel");
        const elapsedWidth = elapsed.width;

        compare(progress.height, 3);
        compare(track.height, 2);
        verify(Math.abs((track.y + track.height / 2) - progress.height / 2) <= 0.5);
        compare(track.color, HoloniightPalette.textDisabled);
        compare(track.opacity, 0.28);
        compare(fill.height, 3);
        compare(progress.fraction, 0.99);
        compare(fill.opacity, 1.0);
        compare(timeRow.y - (progress.y + progress.height), 8);
        compare(elapsed.color, HoloniightPalette.textDisabled);
        compare(duration.color, HoloniightPalette.textDisabled);
        verify(elapsed.opacity > duration.opacity);
        compare(elapsed.x, 0);
        compare(duration.x + duration.width, 256);

        widget.positionUs = 11000000;
        compare(elapsed.width, elapsedWidth);
        widget.positionUs = -10;
        compare(progress.fraction, 0.0);
        widget.positionUs = 150000000;
        compare(progress.fraction, 1.0);
        widget.canSeek = false;
        compare(fill.opacity, 0.6);
        compare(findChild(widget, "mprisProgressCyanStop").color, HoloniightPalette.accentCyan);
        compare(findChild(widget, "mprisProgressBlueStop").color, HoloniightPalette.accentBlue);
        compare(findChild(widget, "mprisProgressVioletStop").color, HoloniightPalette.accentViolet);
    }

    function test_artworkUsesSharedShapeAndFullFrameCrop() {
        const widget = createWidget();
        const artwork = findChild(widget, "mprisArtwork");
        const content = findChild(widget, "mprisArtworkContent");
        const mask = findChild(widget, "mprisArtworkMask");
        const effect = findChild(widget, "mprisArtworkEffect");
        const frame = findChild(widget, "mprisArtworkFrame");

        compare(artwork.implicitWidth, 256);
        compare(artwork.implicitHeight, 256);
        compare(artwork.placeholderVisible, true);
        compare(content.width, artwork.width);
        compare(content.height, artwork.height);
        compare(content.fillMode, Image.PreserveAspectCrop);
        compare(effect.maskEnabled, true);
        compare(effect.maskSource, mask);
        compare(mask.width, frame.width);
        compare(mask.height, frame.height);
        compare(mask.fillColor, "#ffffff");
        compare(frame.fillColor, "#00000000");

        artwork.artworkPath = "/definitely/missing/artwork.png";
        tryCompare(artwork, "imageError", true);
        compare(artwork.placeholderVisible, true);
        tryCompare(content, "status", Image.Ready);
        compare(effect.visible, true);
        artwork.artworkPath = "";
        compare(artwork.imageError, false);
    }

    function test_ambientWashTracksPlaybackAndVisibility() {
        const widget = createWidget();
        const washSource = findChild(widget, "mprisWashSource");
        const wash = findChild(widget, "mprisAmbientWash");
        compare(washSource.sourceSize.width, 96);
        compare(washSource.sourceSize.height, 96);
        compare(washSource.asynchronous, true);
        compare(washSource.cache, true);
        compare(wash.blurEnabled, true);
        compare(wash.blur, 1.0);
        compare(wash.brightness, -0.32);
        compare(wash.contrast, -0.12);
        compare(wash.saturation, 0.18);
        compare(wash.opacity, 0.14);

        widget.playbackStatus = "Paused";
        tryCompare(widget, "state", "paused");
        compare(wash.opacity, 0.07);
        widget.pausedTimedOut = true;
        tryCompare(widget, "opacity", 0.0, 1200);
        compare(widget.visible, false);
        compare(wash.visible, false);
    }

    function test_presentationStates() {
        const widget = createWidget();
        compare(widget.state, "playing");
        compare(widget.opacity, 1.0);

        widget.playbackStatus = "Paused";
        tryCompare(widget, "state", "paused");
        tryCompare(widget, "presentationOpacity", 0.55, 500);
        compare(findChild(widget, "mprisArtwork").opacity, 1.0);

        widget.contentVisible = false;
        compare(widget.state, "occupancyHidden");
        compare(widget.visible, false);
        widget.contentVisible = true;
        widget.playbackStatus = "Stopped";
        compare(widget.state, "stopped");
        compare(widget.visible, false);
    }

    function test_themedAssetsLoad() {
        const artworkWindow = createTemporaryObject(artworkWindowComponent, null);
        verify(artworkWindow !== null);
        const artwork = findChild(artworkWindow, "standaloneMprisArtwork");
        const content = findChild(artworkWindow, "mprisArtworkContent");
        const glyphAsset = "qrc:/HolonightShell/media/now-playing-glyph.svg";
        verify(HnIconProvider.supportsSemanticColors(glyphAsset));
        tryCompare(content, "status", Image.Ready);

        const widget = createWidget();
        const glyph = findChild(widget, "mprisNowPlayingGlyph");
        verify(String(glyph.source).startsWith("image://hnicons/"));
        tryCompare(glyph, "status", Image.Ready);
        compare(glyph.sourceSize.width, 16);
        compare(glyph.sourceSize.height, 16);

        const localArtworkUrl = String(Qt.resolvedUrl("../../assets/media/artwork-fallback.svg"));
        artwork.artworkPath = localArtworkUrl.substring("file://".length);
        tryCompare(content, "status", Image.Ready);
        compare(artwork.placeholderVisible, false);
    }
}

import QtQuick
import QtTest
import HolonightShell

// T-020: QML smoke test for MprisService singleton registration, MprisWidget property binding,
// and icon resolution (REQ-F-009, REQ-F-010, REQ-F-016, REQ-F-017, REQ-F-018, REQ-NF-006).

TestCase {
    id: root

    name: "MprisQmlTests"

    Component {
        id: widgetComponent
        MprisWidget {
            barMonitorName: "TEST-1"
        }
    }

    // MprisService is one process-wide singleton and QtQuickTest runs test functions in
    // alphabetical (not declaration) order, so every player seeded by any test in this file must
    // be torn down before the next test runs — otherwise leftover players (and same-millisecond
    // timestamp ties in the selection algorithm's deterministic tie-break) leak between tests.
    function init() {
        MprisTestSeed.removePlayer("org.mpris.MediaPlayer2.qmltest1")
        MprisTestSeed.removePlayer("org.mpris.MediaPlayer2.qmltest2")
        MprisTestSeed.removePlayer("org.mpris.MediaPlayer2.qmltest3")
        MprisTestSeed.removePlayer("org.mpris.MediaPlayer2.qmltest4")
    }

    function test_widget_zero_width_when_no_active_player() {
        compare(MprisService.hasActivePlayer, false)
        const widget = createTemporaryObject(widgetComponent, null)
        verify(widget)
        compare(widget.implicitWidth, 0)
    }

    function test_singleton_accessible_without_errors() {
        verify(MprisService.hasActivePlayer !== undefined)
        verify(MprisService.activeTitle !== undefined)
        verify(MprisService.activeDesktopEntry !== undefined)
    }

    function test_seeding_a_playing_player_sets_has_active_player() {
        MprisTestSeed.seedPlayer("org.mpris.MediaPlayer2.qmltest1", {
            "PlaybackStatus": "Playing",
            "Metadata": {
                "xesam:title": "QML Test Song",
                "xesam:artist": ["QML Test Artist"],
                "mpris:trackid": "track-qmltest1"
            },
            "Identity": "QmlTestPlayer",
            "DesktopEntry": "vlc",
            "CanControl": true,
            "CanPlay": true,
            "CanPause": true,
            "CanGoNext": true,
            "CanGoPrevious": true
        })

        compare(MprisService.hasActivePlayer, true)
        compare(MprisService.activeTitle, "QML Test Song")
        compare(MprisService.activeArtist, "QML Test Artist")
    }

    function test_widget_icon_url_matches_image_icon_pattern() {
        MprisTestSeed.seedPlayer("org.mpris.MediaPlayer2.qmltest2", {
            "PlaybackStatus": "Playing",
            "Metadata": {
                "xesam:title": "Icon Test Song",
                "xesam:artist": [],
                "mpris:trackid": "track-qmltest2"
            },
            "Identity": "IconTestPlayer",
            "DesktopEntry": "spotify",
            "CanControl": true,
            "CanPlay": true,
            "CanPause": true,
            "CanGoNext": true,
            "CanGoPrevious": true
        })

        const widget = createTemporaryObject(widgetComponent, null)
        verify(widget)
        compare(widget.ready, true)

        const icon = findChild(widget, "mprisAppIcon")
        verify(icon)
        compare(icon.source.toString(), "image://icon/spotify")
    }

    function test_control_buttons_reflect_capability_gating() {
        MprisTestSeed.seedPlayer("org.mpris.MediaPlayer2.qmltest3", {
            "PlaybackStatus": "Playing",
            "Metadata": {
                "xesam:title": "Gating Test Song",
                "xesam:artist": [],
                "mpris:trackid": "track-qmltest3"
            },
            "Identity": "GatingTestPlayer",
            "DesktopEntry": "vlc",
            "CanControl": true,
            "CanPlay": true,
            "CanPause": true,
            "CanGoNext": false,
            "CanGoPrevious": true
        })

        const widget = createTemporaryObject(widgetComponent, null)
        verify(widget)

        const nextButton = findChild(widget, "mprisNextButton")
        verify(nextButton)
        compare(nextButton.buttonEnabled, false)

        const previousButton = findChild(widget, "mprisPreviousButton")
        verify(previousButton)
        compare(previousButton.buttonEnabled, true)

        const widthBeforeCapabilityChange = widget.implicitWidth
        MprisTestSeed.setCanGoNext("org.mpris.MediaPlayer2.qmltest3", true)
        compare(nextButton.buttonEnabled, true)
        compare(widget.implicitWidth, widthBeforeCapabilityChange)
    }

    function test_play_pause_glyph_reflects_playback_status() {
        MprisTestSeed.seedPlayer("org.mpris.MediaPlayer2.qmltest4", {
            "PlaybackStatus": "Playing",
            "Metadata": {
                "xesam:title": "Glyph Test Song",
                "xesam:artist": [],
                "mpris:trackid": "track-qmltest4"
            },
            "Identity": "GlyphTestPlayer",
            "DesktopEntry": "vlc",
            "CanControl": true,
            "CanPlay": true,
            "CanPause": true,
            "CanGoNext": true,
            "CanGoPrevious": true
        })

        const widget = createTemporaryObject(widgetComponent, null)
        verify(widget)
        const playPauseButton = findChild(widget, "mprisPlayPauseButton")
        verify(playPauseButton)
        compare(playPauseButton.playing, true)

        MprisTestSeed.setPlaybackStatus("org.mpris.MediaPlayer2.qmltest4", "Paused")
        compare(MprisService.activePlaybackStatus, "Paused")
        compare(playPauseButton.playing, false)
    }
}

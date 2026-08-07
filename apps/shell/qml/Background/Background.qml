import QtQuick
import QtQuick.Window
import Holonight.Core
import HolonightShell

import "../Utility" as Utility

// Full-screen wallpaper host for one monitor. Hosted by BackgroundManager on the wlr-layer-shell
// BACKGROUND layer. Renders the configured image with crop-to-fill, crossfading on live reload, and
// falls back to the surface solid color when there is no image or the image fails to load.
//
// Two Image elements ping-pong: one is the visible "front", the other loads the incoming wallpaper
// underneath/over it. On hand-off we just flip which element is the front — we never copy a source
// from one Image to the other, so the incoming wallpaper is decoded exactly once (a crossfade between
// two *different* images inherently needs both decoded at the same time; that is not redundant).
Item {
    id: root

    Utility.AppearanceReloadBridge {}

    // Set by BackgroundManager via setInitialProperties at creation and updated on live config reload
    // and on monitor hotplug re-indexing. "" => no wallpaper; the surface base shows through.
    required property string imagePath

    // False until Component.onCompleted has shown the first wallpaper. Guards onImagePathChanged so the
    // initial setInitialProperties assignment does not also kick off a crossfade — that double-trigger
    // (instant path + crossfade path) races and blanks the image. Crossfades only run after this.
    property bool ready: false

    // Which Image element is currently the visible front. The other is the back, used to load the next
    // wallpaper before it is faded in and promoted to front.
    property bool frontIsA: true
    readonly property Image frontImage: root.frontIsA ? imageA : imageB
    readonly property Image backImage: root.frontIsA ? imageB : imageA

    // Always-present solid base. Visible whenever no image is loaded (empty list, or a load failure).
    Rectangle {
        anchors.fill: parent
        color: HoloniightPalette.surface
    }

    Image {
        id: imageA
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(Screen.width, Screen.height)
        asynchronous: true
        cache: false
        // The back (incoming) image sits on top so it can crossfade over the opaque front.
        z: root.frontIsA ? 0 : 1
        onStatusChanged: root.handleBackReady(imageA)
    }

    Image {
        id: imageB
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(Screen.width, Screen.height)
        asynchronous: true
        cache: false
        opacity: 0
        z: root.frontIsA ? 1 : 0
        onStatusChanged: root.handleBackReady(imageB)
    }

    // Crossfade the back image in over the front; promote it to front when fully opaque.
    NumberAnimation {
        id: fadeIn
        target: root.backImage
        property: "opacity"
        from: 0
        to: 1
        duration: 250
        easing.type: Easing.InOutQuad
        onFinished: root.promoteBackToFront()
    }

    // Fade the front image out to reveal the surface base when the wallpaper is cleared.
    NumberAnimation {
        id: fadeOut
        target: root.frontImage
        property Image fadedImage: root.frontImage
        property: "opacity"
        from: 1
        to: 0
        duration: 250
        easing.type: Easing.InOutQuad
        onFinished: {
            fadeOut.fadedImage.source = ""
            fadeOut.fadedImage.opacity = 1
        }
    }

    // A back image finished loading: start its fade-in. Errors fall back to the base color.
    function handleBackReady(img: Image) {
        if (img !== root.backImage)
            return
        if (img.status === Image.Error) {
            console.warn("Background: failed to load image:", img.source)
            img.source = ""
            img.opacity = 0
            return
        }
        if (img.status === Image.Ready)
            fadeIn.start()
    }

    // The back image is fully faded in; make it the front and retire the old front.
    function promoteBackToFront() {
        let oldFront = root.frontImage
        root.frontIsA = !root.frontIsA
        // Old front is now the back element: hide and clear it so it holds no decoded pixmap.
        oldFront.opacity = 0
        oldFront.source = ""
    }

    onImagePathChanged: {
        // Ignore the initial assignment from setInitialProperties; Component.onCompleted shows the
        // first wallpaper instantly. Only live config reloads / hotplug (after ready) crossfade.
        if (!root.ready)
            return

        if (root.imagePath === "") {
            fadeIn.stop()
            root.backImage.source = ""
            root.backImage.opacity = 0
            if (String(root.frontImage.source).length > 0) {
                fadeOut.fadedImage = root.frontImage
                fadeOut.start()
            }
            return
        }

        // No-op if the wallpaper for this monitor did not actually change (common on hotplug re-index).
        if (String(root.frontImage.source) === String(root.imagePath))
            return

        fadeOut.stop()
        fadeIn.stop()
        root.backImage.opacity = 0
        root.backImage.source = root.imagePath
    }

    Component.onCompleted: {
        // First wallpaper appears immediately (no crossfade on initial show).
        if (root.imagePath !== "") {
            frontImage.source = root.imagePath
            frontImage.opacity = 1
        }
        root.ready = true
    }
}

import QtQuick
import QtTest
import HolonightShell

TestCase {
    id: testCase
    name: "TopbarComposition"
    when: windowShown
    visible: true
    width: 1920
    height: 160

    TopBar {
        id: bar
        width: testCase.width
        height: 64
        barMonitorName: "TEST-1"
    }

    StatusesSection {
        id: statuses
        y: 90
        width: implicitWidth
        barMonitorName: "TEST-1"
    }

    BarFrame {
        id: reference
        x: 500
        y: 90
        width: statuses.width
        height: statuses.height
        leftBottomOffset: statuses.slantCut
    }

    function init() {
        StatusPopupSurface.setActivePopupId("")
        TopbarTestSeed.reset()
        tryVerify(function() { return statuses.width > 200 })
        waitForRendering(bar, 100)
    }

    function sectionBounds() {
        const bounds = []
        // The row's immediate children are the independently sized bar sections.
        const row = bar.children[1]
        for (let section of row.children) {
            if (!section.visible || section.width <= 0)
                continue
            const point = section.mapToItem(bar, 0, 0)
            verify(point.x >= 0, "section starts outside bar: " + section)
            verify(point.x + section.width <= bar.width + 0.01, "section ends outside bar: " + section)
            verify(point.y >= 0 && point.y + section.height <= bar.height)
            bounds.push([point.x, point.y, section.width, section.height])
        }
        verify(bounds.length >= 4)
        return JSON.stringify(bounds)
    }

    function test_settled_sections_remain_inside_bar() {
        // Observe several rendered frames with a constant service state.
        const initial = sectionBounds()
        for (let frame = 0; frame < 4; ++frame) {
            bar.update()
            waitForRendering(bar, 100)
            compare(sectionBounds(), initial)
        }
    }

    function test_status_backgrounds_preserve_frame_at_rest() {
        waitForRendering(statuses, 100)
        waitForRendering(reference, 100)
        const actual = controlsEvidence.captureItem(statuses)
        const expected = controlsEvidence.captureItem(reference)
        compare(actual.width, expected.width)
        compare(actual.height, expected.height)
        const ratio = actual.width / statuses.width
        // Sample the empty left strip of each status control within its feedback
        // rectangle. An opaque resting rectangle would overwrite the gradient.
        const row = statuses.children[1].children[1]
        for (let widget of row.children) {
            if (!widget.visible || widget.width <= 0)
                continue
            const point = widget.mapToItem(statuses, 1, 20)
            const x = Math.floor(point.x * ratio)
            const y = Math.floor(point.y * ratio)
            compare(actual.pixel(x, y), expected.pixel(x, y), "resting fill: " + widget)
        }
        // Compare the complete right edge against the isolated drawing.
        for (let y = 4; y < actual.height - 4; ++y)
            compare(actual.pixel(actual.width - 2, y), expected.pixel(expected.width - 2, y))
    }

    function test_active_feedback_returns_to_transparent_resting_fill() {
        const row = statuses.children[1].children[1]
        function feedbackStrip() {
            const image = controlsEvidence.captureItem(statuses)
            const ratio = image.width / statuses.width
            const pixels = []
            // Cover the rasterized left border rather than assuming its stroke
            // lands on one exact pixel at every fractional DPR.
            for (let offset = -1; offset <= 5; ++offset) {
                for (let height = 24; height <= 40; ++height) {
                    const point = row.children[0].mapToItem(statuses, offset, height)
                    pixels.push(String(image.pixel(Math.floor(point.x * ratio), Math.floor(point.y * ratio))))
                }
            }
            return JSON.stringify(pixels)
        }
        const resting = feedbackStrip()
        StatusPopupSurface.setActivePopupId("network")
        tryVerify(function() { return feedbackStrip() !== resting })
        StatusPopupSurface.setActivePopupId("")
        tryVerify(function() { return feedbackStrip() === resting })
    }

}

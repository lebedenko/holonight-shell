import QtQuick
import QtTest
import HolonightShell

TestCase {
    id: root

    name: "OsdView"

    Component {
        id: viewComponent

        OsdView {
            monitorName: "test-monitor"
            configured: true
        }
    }

    function makeView(properties) {
        const item = createTemporaryObject(viewComponent, null, properties)
        verify(item)
        return item
    }

    function test_channel_change_restarts_from_the_entrance_pose() {
        const item = makeView({
            kind: "level",
            channel: "audio-volume",
            value: 25,
            muted: false
        })

        item.present()
        tryCompare(item, "opacity", 1)
        tryCompare(item, "scale", 1)

        item.channel = "screen-brightness"
        item.present()

        verify(item.opacity < 1)
        verify(item.scale < 1)
        tryCompare(item, "opacity", 1)
        tryCompare(item, "scale", 1)
    }

    function test_payload_can_be_set_before_switching_renderer_kind() {
        const item = makeView({
            kind: "level",
            channel: "audio-volume",
            value: 30,
            muted: false
        })

        item.shortLabel = "DE"
        item.fullLabel = "German"
        item.channel = "keyboard-layout"
        item.kind = "selection"

        compare(item.contentItem.shortLabel, "DE")
        compare(item.contentItem.fullLabel, "German")
        compare(item.contentItem.displayShortLabel, "DE")
        compare(item.contentItem.displayFullLabel, "German")
    }
}

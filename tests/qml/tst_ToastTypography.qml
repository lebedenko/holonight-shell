import QtQuick
import QtTest
import HolonightShell

TestCase {
    name: "ToastTypography"
    visible: true

    Component {
        id: stackComponent

        ToastStack {
            monitorName: "DP-1"
        }
    }

    function test_stack_hides_when_empty_and_reappears_for_the_next_toast() {
        const stack = createTemporaryObject(stackComponent, this)
        verify(stack)
        const repeater = findChild(stack, "toastRepeater")
        verify(repeater)
        repeater.model = []
        compare(stack.hasToasts, false)
        compare(stack.visible, false)

        const notification = { notifId: 57, summary: "Focus regression", body: "", actions: [], createdAtMs: Date.now() }
        repeater.model = [notification]
        compare(stack.hasToasts, true)
        compare(stack.visible, true)
        repeater.model = []
        compare(stack.hasToasts, false)
        compare(stack.visible, false)
        repeater.model = [notification]
        compare(stack.hasToasts, true)
        compare(stack.visible, true)
    }

    Component {
        id: toastComponent

        ToastItem {
            width: 430
            model: ({
                notifId: 55,
                summary: "Update available",
                body: "<b>Important</b><img src='untrusted.png'> details",
                actions: [],
                createdAtMs: Date.now()
            })
        }
    }

    function test_styled_body_keeps_supported_markup_and_drops_images() {
        const toast = createTemporaryObject(toastComponent, this)
        verify(toast)

        const body = findChild(toast, "toastBody")
        verify(body)
        compare(body.textFormat, Text.StyledText)
        verify(body.text.includes("<b>Important</b>"))
        verify(!body.text.includes("<img"))
        verify(body.implicitHeight > 0)
    }

    function test_relative_time_tracks_replaced_notification() {
        const toast = createTemporaryObject(toastComponent, this)
        verify(toast)
        const time = findChild(toast, "toastRelativeTime")
        verify(time)

        toast.model = {
            notifId: 56,
            summary: "Updated notification",
            body: "Updated body",
            actions: [],
            createdAtMs: Date.now() - 121000
        }
        compare(time.text, "2m ago")
    }
}

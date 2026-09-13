import QtQuick
import QtTest
import Holonight.Authentication
import Holonight.Authentication.Test

TestCase {
    id: testCase
    name: "AuthenticationRealModel"
    when: windowShown
    property var fixture
    property var dialog
    Component { id: fixtureComponent; AuthenticationUiFixture {} }
    Component { id: dialogComponent; AuthenticationDialog {} }

    function init() {
        fixture = fixtureComponent.createObject(testCase)
        dialog = dialogComponent.createObject(null, {promptModel: fixture.model})
        verify(dialog !== null)
    }
    function cleanup() {
        child("identitySelector").popup.close()
        dialog.visible = false
        dialog.destroy()
        fixture.destroy()
    }
    function child(name) { return findChild(dialog, name) }
    function beginPolkit(count) {
        verify(fixture.beginPolkit(count))
        tryCompare(fixture.model, "lifecycleState", count > 1 ? 1 : 3)
        tryCompare(dialog, "visible", true)
        tryCompare(dialog, "active", true)
    }
    function capture(name) {
        waitForRendering(dialog.contentItem, 100)
        const image = fixture.capture(dialog)
        const path = fixture.evidencePath(name)
        if (path.length > 0) image.save(path)
        return image
    }
    function inputVisible(expected) {
        for (const name of ["promptLabel", "responseField", "responseHelper"])
            compare(child(name).visible, expected, name)
    }
    function test_failureRetryNewPrompt() {
        beginPolkit(1)
        fixture.prompt(false)
        tryCompare(child("responseField"), "activeFocus", true)
        inputVisible(true)
        child("responseField").text = "disposable-synthetic-text"
        child("authenticationPrompt").revealed = true
        child("authenticationPrompt").submitResponse()
        compare(fixture.model.lifecycleState, 3)
        compare(child("responseField").text, "")
        compare(child("authenticationPrompt").revealed, false)
        fixture.fail()
        tryCompare(fixture.model, "lifecycleState", 4)
        capture("failure")
        inputVisible(false)
        verify(child("retryButton").visible)
        verify(child("cancelButton").enabled)
        verify(fixture.model.messages.rowCount() > 0)
        child("retryButton").clicked()
        tryCompare(fixture.model, "lifecycleState", 3)
        capture("retry-waiting")
        inputVisible(false)
        fixture.prompt(false)
        tryCompare(fixture.model, "lifecycleState", 2)
        capture("new-prompt")
        inputVisible(true)
        tryCompare(child("responseField"), "activeFocus", true)
        compare(child("responseField").text, "")
        compare(child("authenticationPrompt").revealed, false)
    }
    function labelWithText(item, text) {
        if (item.text !== undefined && item.text === text && item.textFormat !== undefined) return item
        for (const child of item.children) {
            const found = labelWithText(child, text)
            if (found) return found
        }
        return null
    }
    function brightPixels(image, item, inset) {
        const ratio = image.width / dialog.contentItem.width
        const point = item.mapToItem(dialog.contentItem, inset, inset)
        let count = 0
        for (let y = Math.ceil(point.y * ratio); y < Math.floor((point.y + item.height - 2 * inset) * ratio); ++y)
            for (let x = Math.ceil(point.x * ratio); x < Math.floor((point.x + item.width - 2 * inset) * ratio); ++x) {
                const pixel = image.pixel(x, y)
                if (pixel.r > 0.4 && pixel.g > 0.4 && pixel.b > 0.4) ++count
            }
        return count
    }
    function test_profileFallbacksAndStableKeyboardSelection() {
        beginPolkit(3)
        const selector = child("identitySelector")
        tryCompare(selector, "activeFocus", true)
        keyClick(Qt.Key_Space)
        tryCompare(selector.popup, "opened", true)
        const list = selector.popup.contentItem
        tryVerify(() => list.itemAtIndex(0) !== null)
        const row = list.itemAtIndex(0)
        const avatar = findChild(row, "identityOptionAvatar")
        const local = fixture.localAvatar()
        verify(fixture.updateProfile("account-0", "updated-user", "Updated <b>Name</b>", local))
        tryCompare(row, "text", "Updated <b>Name</b>")
        tryCompare(child("accountName"), "text", "Updated <b>Name</b>")
        tryCompare(findChild(avatar, "hnAvatarImage"), "status", Image.Ready)
        const image = capture("updated-local-avatar")
        if (fixture.shaderRendering(dialog)) {
            const point = avatar.mapToItem(dialog.contentItem, avatar.width / 2, avatar.height / 2)
            const ratio = image.width / dialog.contentItem.width
            const pixel = image.pixel(Math.floor(point.x * ratio), Math.floor(point.y * ratio))
            verify(pixel.r > 0.7 && pixel.g < 0.3 && pixel.b > 0.4, String(pixel))
        }
        verify(fixture.updateProfile("account-0", "updated-user", "", "https://invalid/avatar.png"))
        tryCompare(row, "text", "updated-user")
        compare(String(avatar.source), "")
        verify(fixture.updateProfile("account-0", "", "", ""))
        tryCompare(row, "text", "Account 0")
        keyClick(Qt.Key_Down)
        keyClick(Qt.Key_Down)
        keyClick(Qt.Key_Return)
        tryCompare(fixture.model, "selectedIdentity", "account-2")
        compare(fixture.model.lifecycleState, 3)
        fixture.prompt(false)
        fixture.model.respond("disposable-synthetic-text")
        fixture.fail()
        fixture.model.retry()
        tryCompare(fixture.model, "lifecycleState", 1)
        tryCompare(selector, "currentIndex", 2)
        tryCompare(selector, "activeFocus", true)
        keyClick(Qt.Key_Return)
        tryCompare(fixture.model, "lifecycleState", 3)
        fixture.model.cancel()
        beginPolkit(3)
        tryCompare(selector, "currentIndex", 0)
    }
    function test_realIdentityRowsAndReopening() {
        beginPolkit(12)
        waitForRendering(dialog.contentItem)
        dialog.minimumWidth = 400
        dialog.minimumHeight = 450
        dialog.maximumWidth = 400
        dialog.maximumHeight = 450
        dialog.width = 400
        dialog.height = 450
        const selector = child("identitySelector")
        tryCompare(selector, "currentIndex", 0)
        for (let opening = 0; opening < 2; ++opening) {
            selector.popup.open()
            tryCompare(selector.popup, "opened", true)
            const list = selector.popup.contentItem
            tryVerify(() => list.itemAtIndex(0) !== null)
            const row = list.itemAtIndex(0)
            compare(row.stableId, "account-0")
            compare(row.text, "<b>Full Name</b>")
            verify(row.visible)
            verify(row.width > 100 && row.height === 82)
            verify(row.contentItem.visible)
            verify(row.contentItem.width > 50 && row.contentItem.height > 0)
            const avatar = findChild(row, "identityOptionAvatar")
            verify(avatar.visible)
            compare(avatar.width, 56)
            verify(String(avatar.fallbackSource).endsWith("user-avatar.svg"))
            tryCompare(findChild(avatar, "hnAvatarFallback"), "status", Image.Ready)
            waitForRendering(dialog.contentItem)
            const image = capture("identities-" + opening)
            const label = labelWithText(row.contentItem, row.text)
            verify(label !== null)
            compare(label.textFormat, Text.PlainText)
            verify(label.visible && label.width > 50 && label.height > 0)
            verify(brightPixels(image, label, 0) > 10, "identity label must render glyphs")
            if (fixture.shaderRendering(dialog))
                verify(brightPixels(image, avatar, 12) > 10, "fallback avatar artwork must render")
            const top = list.mapToItem(dialog.contentItem, 0, 0)
            const bottom = list.mapToItem(dialog.contentItem, 0, list.height)
            verify(top.y >= 0 && bottom.y <= dialog.height)
            console.log("IDENTITY", JSON.stringify({opening: opening, label: row.text, top: top, bottom: bottom}))
            tryVerify(() => list.contentHeight > list.height)
            list.positionViewAtIndex(11, ListView.Contain)
            tryVerify(() => list.contentY > 0)
            selector.popup.close()
            tryCompare(selector.popup, "visible", false)
        }
    }
    function test_askpassBorderCapture_data() {
        return [{tag: "normal", constrained: false}, {tag: "scrolling", constrained: true}]
    }
    function test_askpassBorderCapture(data) {
        verify(fixture.beginAskpass())
        tryCompare(dialog, "visible", true)
        tryCompare(dialog, "active", true)
        waitForRendering(dialog.contentItem)
        if (data.constrained) { dialog.minimumWidth = 400; dialog.minimumHeight = 450; dialog.maximumWidth = 400; dialog.maximumHeight = 450; dialog.width = 400; dialog.height = 450 }
        waitForRendering(dialog.contentItem)
        const field = child("responseField")
        field.forceActiveFocus()
        dialog.revealFocusedControl()
        const scroll = child("authenticationBodyScroll")
        if (data.constrained) tryVerify(() => scroll.contentHeight > scroll.availableHeight)
        tryVerify(() => {
            const point = field.mapToItem(scroll, 0, 0)
            return point.y >= 0 && point.y + field.height <= scroll.height
        })
        for (const focused of [false, true]) {
            if (focused) field.forceActiveFocus()
            else child("cancelButton").forceActiveFocus()
            tryCompare(field, "activeFocus", focused)
            waitForRendering(dialog.contentItem)
            const image = capture("askpass-" + data.tag + "-" + focused)
            const point = field.mapToItem(dialog.contentItem, 0, 0)
            const background = field.background
            const bgPoint = background.mapToItem(dialog.contentItem, 0, 0)
            console.log("BORDER", JSON.stringify({focused: focused, field: point,
                background: bgPoint, width: background.width, height: background.height,
                fieldWidth: field.width, fieldHeight: field.height}))
            let ancestor = background.parent
            const clips = []
            while (ancestor) {
                if (ancestor.clip) {
                    const origin = ancestor.mapToItem(dialog.contentItem, 0, 0)
                    clips.push({origin: origin, width: ancestor.width, height: ancestor.height})
                    verify(bgPoint.x >= origin.x && bgPoint.x + background.width <= origin.x + ancestor.width)
                    verify(bgPoint.y >= origin.y - 0.01 && bgPoint.y + background.height <= origin.y + ancestor.height + 0.01)
                }
                ancestor = ancestor.parent
            }
            console.log("CLIPS", JSON.stringify(clips))
            const ratio = image.width / dialog.contentItem.width
            const left = bgPoint.x * ratio
            const right = (bgPoint.x + background.width) * ratio
            const top = bgPoint.y * ratio
            const bottom = (bgPoint.y + background.height) * ratio
            const fill = background.color
            const border = background.border.color
            function coverage(x, y) {
                const pixel = image.pixel(x, y)
                const delta = [border.r - fill.r, border.g - fill.g, border.b - fill.b]
                const actual = [pixel.r - fill.r, pixel.g - fill.g, pixel.b - fill.b]
                return Math.max(0, Math.min(1, (actual[0] * delta[0] + actual[1] * delta[1] + actual[2] * delta[2])
                    / (delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2])))
            }
            // Integrate the stroke across a straight, empty segment of each edge.
            function edge(center, along, vertical) {
                let total = 0
                for (let sample = -4; sample <= 4; ++sample)
                    for (let offset = -4; offset <= 4; ++offset)
                        total += vertical ? coverage(Math.floor(center) + offset, Math.floor(along) + sample)
                                          : coverage(Math.floor(along) + sample, Math.floor(center) + offset)
                return total / 9
            }
            const edges = [edge(left, (top + bottom) / 2, true), edge(right, (top + bottom) / 2, true),
                edge(top, (left + right) / 2, false), edge(bottom, (left + right) / 2, false)]
            console.log("EDGE_COVERAGE", JSON.stringify({ratio: ratio, edges: edges}))
            verify(Math.min(...edges) > 0.5, JSON.stringify(edges))
            verify(Math.max(...edges) - Math.min(...edges) < 0.35, JSON.stringify(edges))
        }
    }
}

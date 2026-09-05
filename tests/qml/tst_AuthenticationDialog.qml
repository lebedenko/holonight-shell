import QtQuick
import QtQuick.Controls.Basic
import QtTest
import Holonight
import Holonight.Core
import Holonight.Authentication

TestCase {
    id: testCase
    name: "AuthenticationDialog"
    when: windowShown

    property var dialog: null

    QtObject {
        id: model
        property int frontendKind: 1
        property var selectedAccount: ({})
        property string fieldAtSubmission: ""
        property string requestMessage: "Authenticate"
        property string requestReference: "org.example.action"
        property var requesterDetails: ({})
        property string currentPrompt: "Password"
        property int inputMode: 2
        property var identities: []
        property string selectedIdentity: ""
        property var messages: []
        property int lifecycleState: 2
        property string requestToken: "test"
        property int sessionGeneration: 1
        property string response: ""
        property string operation: ""
        signal clearSensitiveInput()
        signal promptChanged()
        onInputModeChanged: promptChanged()
        onCurrentPromptChanged: promptChanged()
        function respond(value) { fieldAtSubmission = findChild(testCase.dialog, "responseField").text; response = value; operation = "respond"; clearSensitiveInput() }
        function confirm(accepted) { operation = accepted ? "allow" : "reject" }
        function acknowledge() { operation = "acknowledge" }
        function cancel() { operation = "cancel"; clearSensitiveInput() }
        function selectIdentity(stableId) { selectedIdentity = stableId; operation = "identity:" + stableId; lifecycleState = 3 }
        function retry() { operation = "retry"; lifecycleState = identities.count > 1 ? 1 : 2 }
    }

    ListModel {
        id: identityModel
        ListElement { stableId: "root"; displayLabel: "Administrator"; username: "root"; fullName: ""; avatarUrl: "" }
        ListElement { stableId: "user"; displayLabel: "Current user"; username: "user"; fullName: "Current User"; avatarUrl: "" }
    }

    ListModel { id: manyIdentities }

    Component {
        id: dialogComponent
        AuthenticationDialog {}
    }

    function init() {
        model.frontendKind = 1
        model.selectedAccount = ({})
        model.requestReference = "org.example.action"
        model.identities = []
        model.selectedIdentity = ""
        model.requesterDetails = ({})
        model.requestMessage = "Authenticate"
        model.inputMode = 2
        model.lifecycleState = 2
        model.currentPrompt = "Password"
        model.response = ""
        model.operation = ""
        dialog = dialogComponent.createObject(null, {"promptModel": model})
        verify(dialog !== null)
        tryCompare(dialog, "visible", true)
    }

    function cleanup() {
        dialog.destroy()
        dialog = null
    }

    function test_secretAndVisibleInputModes() {
        const field = findChild(dialog, "responseField")
        verify(field !== null)
        compare(field.echoMode, TextInput.Password)
        verify((field.inputMethodHints & Qt.ImhSensitiveData) !== 0)
        compare(field.selectByMouse, false)
        tryCompare(field, "activeFocus", true)

        field.text = "marker-secret"
        model.clearSensitiveInput()
        compare(field.text, "")

        model.inputMode = 1
        compare(field.echoMode, TextInput.Normal)
        compare(field.selectByMouse, true)
    }

    function test_submitAndCancelClearSensitiveField() {
        const field = findChild(dialog, "responseField")
        field.text = "submitted-secret"
        findChild(dialog, "authenticateButton").clicked()
        compare(model.operation, "respond")
        compare(model.response, "submitted-secret")
        compare(model.fieldAtSubmission, "")
        compare(field.text, "")

        model.operation = ""
        field.text = "cancelled-secret"
        findChild(dialog, "cancelButton").clicked()
        compare(model.operation, "cancel")
        compare(field.text, "")
    }

    function test_confirmationAndNotificationControls() {
        model.inputMode = 3
        const allow = findChild(dialog, "allowButton")
        const reject = findChild(dialog, "rejectButton")
        const actionRow = findChild(dialog, "actionRow")
        compare(allow.visible, true)
        compare(reject.visible, true)
        allow.clicked()
        compare(model.operation, "allow")
        reject.clicked()
        compare(model.operation, "reject")

        model.inputMode = 4
        compare(actionRow.visible, false)
        compare(findChild(dialog, "responseField").visible, false)
    }

    function test_dialogCancellationControl() {
        const cancel = findChild(dialog, "cancelButton")
        verify(cancel.visible)
        cancel.clicked()
        compare(model.operation, "cancel")
    }

    function test_keyboardSecretTabBacktabAndEscape() {
        const field = findChild(dialog, "responseField")
        const cancel = findChild(dialog, "cancelButton")
        tryCompare(field, "activeFocus", true)
        const reveal = findChild(dialog, "revealButton")
        const authenticate = findChild(dialog, "authenticateButton")
        keyClick(Qt.Key_Tab)
        tryCompare(reveal, "activeFocus", true)
        keyClick(Qt.Key_Tab)
        tryCompare(cancel, "activeFocus", true)
        keyClick(Qt.Key_Tab)
        tryCompare(authenticate, "activeFocus", true)
        keyClick(Qt.Key_Tab)
        tryCompare(field, "activeFocus", true)
        keyClick(Qt.Key_Backtab)
        tryCompare(authenticate, "activeFocus", true)
        keyClick(Qt.Key_Backtab)
        keyClick(Qt.Key_Backtab)
        tryCompare(reveal, "activeFocus", true)
        keyClick(Qt.Key_Backtab)
        tryCompare(field, "activeFocus", true)
        keyClick(Qt.Key_M)
        keyClick(Qt.Key_Return)
        compare(model.response, "m")
        compare(field.text, "")
        keyClick(Qt.Key_Escape)
        compare(model.operation, "cancel")
    }

    function test_keyboardConfirmationAndRetry() {
        model.inputMode = 3
        const cancel = findChild(dialog, "cancelButton")
        const reject = findChild(dialog, "rejectButton")
        const allow = findChild(dialog, "allowButton")
        tryCompare(cancel, "activeFocus", true)
        keyClick(Qt.Key_Tab)
        tryCompare(reject, "activeFocus", true)
        keyClick(Qt.Key_Tab)
        tryCompare(allow, "activeFocus", true)
        keyClick(Qt.Key_Return)
        compare(model.operation, "allow")
        keyClick(Qt.Key_Backtab)
        keyClick(Qt.Key_Return)
        compare(model.operation, "reject")
        model.inputMode = 2
        model.lifecycleState = 4
        const retry = findChild(dialog, "retryButton")
        tryCompare(retry, "activeFocus", true)
        keyClick(Qt.Key_Return)
        compare(model.operation, "retry")
        tryCompare(findChild(dialog, "responseField"), "activeFocus", true)
    }

    function test_keyboardIdentityReselectionAndBusyCancellation() {
        model.identities = identityModel
        model.selectedIdentity = "user"
        model.lifecycleState = 1
        const selector = findChild(dialog, "identitySelector")
        tryCompare(selector, "activeFocus", true)
        tryCompare(selector, "currentIndex", 1)
        keyClick(Qt.Key_Space)
        tryCompare(selector.popup, "visible", true)
        keyClick(Qt.Key_Up)
        keyClick(Qt.Key_Return)
        compare(model.operation, "identity:root")
        compare(model.lifecycleState, 3)
        compare(findChild(dialog, "responseField").enabled, false)
        const cancel = findChild(dialog, "cancelButton")
        verify(cancel.enabled)
        tryCompare(cancel, "activeFocus", true)
        keyClick(Qt.Key_Return)
        compare(model.operation, "cancel")
        model.lifecycleState = 2
        tryCompare(findChild(dialog, "responseField"), "activeFocus", true)
        model.lifecycleState = 4
        tryCompare(findChild(dialog, "retryButton"), "activeFocus", true)
        keyClick(Qt.Key_Return)
        tryCompare(selector, "activeFocus", true)
    }

    function test_sensitiveClearingSelectionAndAccessibility() {
        const field = findChild(dialog, "responseField")
        tryCompare(field, "activeFocus", true)
        field.text = "synthetic-marker"
        keyClick(Qt.Key_A, Qt.ControlModifier)
        keyClick(Qt.Key_C, Qt.ControlModifier)
        keyClick(Qt.Key_X, Qt.ControlModifier)
        compare(field.selectedText, "")
        compare(field.text, "synthetic-marker")
        compare(field.Accessible.name, "Authentication response")
        compare(field.Accessible.description, "Authentication response")
        compare(field.Accessible.role, Accessible.EditableText)
        field.text = "synthetic-marker"
        model.sessionGeneration++
        compare(field.text, "")
        field.text = "synthetic-marker"
        model.requestToken += "next"
        compare(field.text, "")
        field.text = "synthetic-marker"
        model.inputMode = 1
        compare(field.text, "")
        field.text = "synthetic-marker"
        model.lifecycleState = 3
        compare(field.text, "")
    }

    function test_hostileContextRemainsInertAndBounded() {
        const hostile = "<img src='file:///never-load'><a href='https://invalid'>link</a> $(touch /never-run)"
        model.requestMessage = hostile.repeat(100)
        model.requesterDetails = ({application: hostile, vendor: "Vendor", command: "must not appear"})
        const details = findChild(dialog, "requesterDetails")
        compare(details.textFormat, Text.PlainText)
        verify(!details.text.includes("must not appear"))
        verify(dialog.height <= Screen.height - 48)
        verify(dialog.width <= Screen.width - 48)
        compare(findChild(dialog, "cancelButton").Accessible.name, "Cancel")
    }

    function test_controlsFollowPaletteAndShowKeyboardFocus() {
        const field = findChild(dialog, "responseField")
        const cancel = findChild(dialog, "cancelButton")
        compare(dialog.color, Qt.rgba(0, 0, 0, 0))
        compare(field.palette.base, HoloniightPalette.surfaceRaised)
        compare(cancel.palette.buttonText, HoloniightPalette.textPrimary)
        tryCompare(field, "activeFocus", true)
        keyClick(Qt.Key_Tab)
        keyClick(Qt.Key_Tab)
        verify(cancel.visualFocus)
        keyClick(Qt.Key_Backtab)
        keyClick(Qt.Key_Backtab)
        compare(field.background.border.color, HoloniightPalette.accentCyan)
        verify(field.background.border.width > 1)
    }
    function test_revealKeyboardAndEveryResetSignal() {
        const field = findChild(dialog, "responseField")
        const reveal = findChild(dialog, "revealButton")
        const prompt = findChild(dialog, "authenticationPrompt")
        tryCompare(field, "activeFocus", true)
        field.text = "secret"
        keyClick(Qt.Key_Tab)
        tryCompare(reveal, "activeFocus", true)
        keyClick(Qt.Key_Space)
        compare(field.echoMode, TextInput.Normal)
        compare(reveal.Accessible.name, "Hide password")
        field.forceActiveFocus()
        keyClick(Qt.Key_A, Qt.ControlModifier)
        keyClick(Qt.Key_C, Qt.ControlModifier)
        keyClick(Qt.Key_X, Qt.ControlModifier)
        compare(field.selectedText, "")
        compare(field.text, "secret")
        verify(!field.selectByMouse)
        verify((field.inputMethodHints & Qt.ImhSensitiveData) !== 0)
        const resets = [() => model.clearSensitiveInput(), () => model.promptChanged(),
            () => model.requestToken += "x", () => model.sessionGeneration++,
            () => model.lifecycleState = 3]
        for (const reset of resets) {
            prompt.revealed = true
            field.text = "secret"
            reset()
            compare(field.text, "")
            compare(prompt.revealed, false)
            compare(field.echoMode, TextInput.Password)
        }
    }

    function test_keypadEnterAndVisibleInputCycle() {
        model.inputMode = 1
        const field = findChild(dialog, "responseField")
        const cancel = findChild(dialog, "cancelButton")
        const authenticate = findChild(dialog, "authenticateButton")
        tryCompare(field, "activeFocus", true)
        verify(!findChild(dialog, "revealButton").visible)
        field.text = "challenge response"
        keyClick(Qt.Key_Enter)
        compare(model.response, "challenge response")
        compare(model.fieldAtSubmission, "")
        keyClick(Qt.Key_Tab)
        tryCompare(cancel, "activeFocus", true)
        keyClick(Qt.Key_Tab)
        tryCompare(authenticate, "activeFocus", true)
        keyClick(Qt.Key_Tab)
        tryCompare(field, "activeFocus", true)
    }

    function test_accountPresentationAndFrontendKinds() {
        model.identities = identityModel
        model.selectedIdentity = "user"
        model.selectedAccount = ({fullName: "Current User", username: "user", displayLabel: "Original", avatarUrl: ""})
        const selector = findChild(dialog, "identitySelector")
        const name = findChild(dialog, "accountName")
        const username = findChild(dialog, "accountUsername")
        verify(selector.visible)
        verify(!selector.enabled)
        compare(name.text, "Current User")
        compare(username.text, "user")
        verify(username.visible)
        const fontSize = name.font.pixelSize
        model.selectedAccount = ({username: "user", displayLabel: "Original"})
        compare(name.text, "user")
        compare(name.font.pixelSize, fontSize)
        verify(!username.visible)
        model.selectedAccount = ({displayLabel: "unix-group:123"})
        compare(name.text, "unix-group:123")
        model.lifecycleState = 3
        verify(selector.visible)
        verify(!selector.enabled)
        model.lifecycleState = 1
        verify(selector.enabled)
        model.identities = [{stableId: "user", displayLabel: "user", username: "user", fullName: "", avatarUrl: ""}]
        tryCompare(selector, "count", 1)
        verify(selector.visible)
        verify(!selector.enabled)
        for (const kind of [0, 2, 3]) {
            model.frontendKind = kind
            verify(!selector.visible)
            verify(findChild(dialog, "frontendSubtitle").text !== "POLKIT SYSTEM REQUEST")
        }
    }

    function test_avatarFailureAndRemoteSourceFallback() {
        model.identities = identityModel
        const control = findChild(dialog, "accountAvatar")
        const avatar = findChild(control, "hnAvatarImage")
        const fallback = findChild(control, "hnAvatarFallback")
        const effect = findChild(control, "hnAvatarEffect")
        model.selectedAccount = ({username: "user", avatarUrl: "https://invalid/avatar.png"})
        compare(String(avatar.source), "")
        compare(effect.source, fallback)
        ignoreWarning(/.*Cannot open.*missing-auth-avatar.*/)
        model.selectedAccount = ({username: "user", avatarUrl: "file:///tmp/missing-auth-avatar.png"})
        tryCompare(avatar, "status", Image.Error)
        compare(effect.source, fallback)
    }

    function test_promptWordingAndMetadataVisibility() {
        const label = findChild(dialog, "promptLabel")
        model.currentPrompt = "Verification code (not echoed):"
        compare(label.text, model.currentPrompt)
        model.currentPrompt = ""
        compare(label.text, "Password")
        model.inputMode = 1
        compare(label.text, "Response")
        model.inputMode = 0
        verify(!label.visible)
        verify(!findChild(dialog, "responseField").visible)
        model.requestReference = ""
        model.requesterDetails = ({})
        verify(!findChild(dialog, "metadataCard").visible)
        model.requesterDetails = ({application: "Settings"})
        verify(findChild(dialog, "metadataCard").visible)
        verify(!findChild(dialog, "actionDetails").visible)
    }

    function beginIdentitySelection() {
        model.identities = identityModel
        model.selectedIdentity = "user"
        model.selectedAccount = ({username: "user", fullName: "Current User", avatarUrl: ""})
        model.inputMode = 0
        model.currentPrompt = ""
        model.lifecycleState = 1
        const selector = findChild(dialog, "identitySelector")
        tryCompare(selector, "currentIndex", 1)
        tryCompare(selector, "activeFocus", true)
        return selector
    }

    function test_continueConfirmsDisplayedAccount() {
        beginIdentitySelection()
        const button = findChild(dialog, "continueButton")
        verify(button.visible && button.enabled)
        verify(findChild(dialog, "identityExplanation").visible)
        verify(!findChild(dialog, "responseField").visible)
        compare(model.operation, "")
        button.clicked()
        compare(model.operation, "identity:user")
        verify(!button.visible)
    }

    function test_enterConfirmsAndRetryRequiresConfirmation() {
        beginIdentitySelection()
        keyClick(Qt.Key_Enter)
        compare(model.operation, "identity:user")
        model.lifecycleState = 4
        findChild(dialog, "retryButton").clicked()
        compare(model.lifecycleState, 1)
        compare(model.operation, "retry")
        const selector = findChild(dialog, "identitySelector")
        tryCompare(selector, "activeFocus", true)
        keyClick(Qt.Key_Return)
        compare(model.operation, "identity:user")
    }

    function test_openPopupEnterConfirmsHighlightedAccount() {
        const selector = beginIdentitySelection()
        keyClick(Qt.Key_Space)
        tryCompare(selector.popup, "opened", true)
        keyClick(Qt.Key_Up)
        compare(model.operation, "")
        keyClick(Qt.Key_Return)
        compare(model.operation, "identity:root")
    }

    function test_dropdownRolesAvatarsAndConstrainedScrolling() {
        beginIdentitySelection()
        manyIdentities.clear()
        for (let i = 0; i < 12; ++i) {
            manyIdentities.append({stableId: "account-" + i, username: "user-" + i,
                fullName: "<b>Long plain-text account name</b> ".repeat(5),
                displayLabel: "Account " + i, avatarUrl: "https://invalid/avatar.png"})
        }
        model.identities = manyIdentities
        model.selectedIdentity = "account-0"
        dialog.width = 400
        dialog.height = 450
        const selector = findChild(dialog, "identitySelector")
        tryCompare(selector, "currentIndex", 0)
        selector.popup.open()
        tryCompare(selector.popup, "opened", true)
        const list = selector.popup.contentItem
        tryVerify(() => list.contentHeight > list.height)
        tryVerify(() => list.itemAtIndex(0) !== null)
        const first = list.itemAtIndex(0)
        compare(first.height, 82)
        compare(first.text, manyIdentities.get(0).fullName)
        verify(first.highlighted)
        const avatar = findChild(first, "identityOptionAvatar")
        compare(avatar.width, 56)
        compare(avatar.height, 56)
        compare(String(avatar.source), "")
        const position = list.mapToItem(dialog.contentItem, 0, 0)
        verify(position.y >= 0)
        verify(position.y + list.height <= dialog.height)
        list.positionViewAtIndex(11, ListView.Contain)
        tryVerify(() => list.contentY > 0)
        selector.popup.close()
    }

    function test_stableGeometryAndNewRequestReset() {
        beginIdentitySelection()
        const initialHeight = dialog.height
        compare(initialHeight, Math.min(720, dialog.maximumHeight))
        compare(dialog.width, Math.min(740, dialog.maximumWidth))
        for (const state of [3, 2, 3, 4, 1]) {
            model.lifecycleState = state
            model.inputMode = state === 2 ? 2 : 0
            model.currentPrompt = state === 2 ? "Password:" : ""
            waitForRendering(dialog.contentItem)
            compare(dialog.height, initialHeight)
        }
        // A smaller compositor configure is preserved through state transitions.
        dialog.height = 350
        model.lifecycleState = 2
        model.inputMode = 2
        waitForRendering(dialog.contentItem)
        compare(dialog.height, 350)
        model.frontendKind = 2
        model.requestToken += "askpass"
        tryCompare(dialog, "height", Math.min(480, dialog.maximumHeight))
        model.frontendKind = 1
        model.requestToken += "polkit"
        tryCompare(dialog, "height", initialHeight)
    }

    function test_overflowKeepsActionsAccessible() {
        model.requestMessage = "Very long request ".repeat(500)
        model.currentPrompt = "Long challenge ".repeat(200)
        dialog.width = 400
        dialog.height = 450
        const cancel = findChild(dialog, "cancelButton")
        const authenticate = findChild(dialog, "authenticateButton")
        const scroll = findChild(dialog, "authenticationBodyScroll")
        tryVerify(() => scroll.contentHeight > scroll.availableHeight)
        for (const button of [cancel, authenticate]) {
            const point = button.mapToItem(dialog.contentItem, 0, 0)
            verify(point.x >= 0 && point.x + button.width <= dialog.width)
            verify(point.y >= 0 && point.y + button.height <= dialog.height)
        }
        const frame = findChild(dialog, "authenticationOuterFrame")
        compare(frame.height, dialog.height - 6)
        const field = findChild(dialog, "responseField")
        field.forceActiveFocus()
        dialog.revealFocusedControl()
        tryVerify(() => scroll.contentItem.contentY > 0)
        const focused = field.mapToItem(scroll, 0, 0)
        verify(focused.y >= 0 && focused.y + field.height <= scroll.height)
    }

}

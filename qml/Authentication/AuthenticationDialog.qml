pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls
import Holonight.Authentication

Controls.ApplicationWindow {
    id: root
    objectName: "authenticationDialog"
    required property var promptModel
    readonly property bool textInput: promptModel.inputMode === 1 || promptModel.inputMode === 2
    readonly property bool awaitingTextInput: promptModel.lifecycleState === 2 && textInput
    readonly property string requester: [promptModel.requesterDetails.application || "",
        promptModel.requesterDetails.vendor || ""].filter(value => value.length > 0).join(" · ")
    readonly property string subtitle: {
        switch (promptModel.frontendKind) {
        case 1: return qsTr("POLKIT SYSTEM REQUEST")
        case 2: return qsTr("SUDO AUTHENTICATION REQUEST")
        case 3: return qsTr("SSH AUTHENTICATION REQUEST")
        default: return qsTr("APPLICATION AUTHENTICATION REQUEST")
        }
    }
    readonly property int preferredWidth: 740
    readonly property int preferredHeight: promptModel.frontendKind === 1 ? 720 : 480
    readonly property bool selectingIdentity: promptModel.lifecycleState === 1
    width: preferredWidth
    height: preferredHeight
    AuthenticationWindowGeometry { id: windowGeometry; window: root }
    function resetSize() { windowGeometry.resetSize(preferredWidth, preferredHeight) }
    visible: promptModel.lifecycleState !== 0 && promptModel.lifecycleState !== 5 && promptModel.lifecycleState !== 6
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.FramelessWindowHint
    title: qsTr("Authentication Required")
    font.family: HolonightTheme.uiFont
    palette.window: HoloniightPalette.surface
    palette.windowText: HoloniightPalette.textPrimary
    palette.base: HoloniightPalette.surfaceRaised
    palette.text: HoloniightPalette.textPrimary
    palette.button: HoloniightPalette.surfaceRaised
    palette.buttonText: HoloniightPalette.textPrimary
    palette.highlight: HoloniightPalette.accentCyan
    palette.highlightedText: HoloniightPalette.textPrimary
    color: "transparent"

    function cycleFocus(forward) {
        const controls = [identitySelector, responseField.inputControl, responseField.revealControl,
            cancelButton, continueButton, authenticateButton, retryButton, rejectButton, allowButton]
            .filter(control => control.visible && control.enabled)
        if (controls.length === 0)
            return
        const index = controls.indexOf(root.activeFocusItem)
        const next = (index + (forward ? 1 : controls.length - 1)) % controls.length
        controls[next].forceActiveFocus(forward ? Qt.TabFocusReason : Qt.BacktabFocusReason)
    }
    function focusInitialControl() {
        if (responseField.visible && responseField.enabled)
            responseField.forceFocus()
        else if (identitySelector.visible && identitySelector.enabled)
            identitySelector.forceActiveFocus(Qt.TabFocusReason)
        else if (retryButton.visible && retryButton.enabled)
            retryButton.forceActiveFocus(Qt.TabFocusReason)
        else if (cancelButton.visible && cancelButton.enabled)
            cancelButton.forceActiveFocus(Qt.TabFocusReason)
    }
    function revealFocusedControl() {
        if (!activeFocusItem || !bodyScroll.contentItem)
            return
        let ancestor = activeFocusItem.parent
        while (ancestor && ancestor !== body)
            ancestor = ancestor.parent
        if (!ancestor)
            return
        const point = activeFocusItem.mapToItem(body, 0, 0)
        if (point.y < 0 || point.y >= body.height)
            return
        const flickable = bodyScroll.contentItem as Flickable
        if (!flickable)
            return
        if (point.y < flickable.contentY)
            flickable.contentY = point.y
        else if (point.y + activeFocusItem.height > flickable.contentY + bodyScroll.availableHeight)
            flickable.contentY = Math.min(body.height - bodyScroll.availableHeight,
                                         point.y + activeFocusItem.height - bodyScroll.availableHeight)
    }
    onActiveFocusItemChanged: Qt.callLater(root.revealFocusedControl)
    onVisibleChanged: {
        if (!visible)
            return
        requestActivate()
        Qt.callLater(root.focusInitialControl)
    }
    onClosing: function(close) { promptModel.cancel(); close.accepted = false }

    HnSurfaceFrame {
        anchors.fill: parent
        objectName: "authenticationOuterFrame"
        anchors.margins: 3
        surfaceRole: HnSurfaceRole.Window
        cornerStyleOverride: HnCornerStyle.Chamfered
        chamferedCornersOverride: HnCornerMask.TopRight | HnCornerMask.BottomLeft
        chamferOverride: 32
        radiusOverride: 12
        fillColor: HoloniightPalette.background
        borderColor: Qt.alpha(HoloniightPalette.accentViolet, 0.45)
    }
    HnSurfaceFrame {
        anchors.fill: parent
        anchors.margins: 7
        surfaceRole: HnSurfaceRole.Window
        cornerStyleOverride: HnCornerStyle.Chamfered
        chamferedCornersOverride: HnCornerMask.TopRight | HnCornerMask.BottomLeft
        chamferOverride: 30
        radiusOverride: 10
        fillColor: HoloniightPalette.surface
        borderColor: Qt.alpha(HoloniightPalette.accentCyan, 0.22)
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 0
        RowLayout {
            id: header
            Layout.fillWidth: true
            Layout.leftMargin: 38
            Layout.rightMargin: 30
            Layout.topMargin: 24
            Layout.bottomMargin: 24
            spacing: 28
            AuthenticationIcon {
                size: 54
                Layout.alignment: Qt.AlignVCenter
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                HnLabel {
                    Layout.fillWidth: true
                    rawText: qsTr("Authentication Required")
                    role: HnTypographyRole.Heading
                    wrapMode: Text.Wrap
                    color: HoloniightPalette.textPrimary
                }
                HnLabel {
                    objectName: "frontendSubtitle"
                    Layout.fillWidth: true
                    rawText: root.subtitle
                    role: HnTypographyRole.MicroHeader
                    wrapMode: Text.Wrap
                    color: HoloniightPalette.accentViolet
                    font.family: HolonightTheme.uiFont
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: Qt.alpha(HoloniightPalette.accentCyan, 0.6) }
                GradientStop { position: 1; color: Qt.alpha(HoloniightPalette.accentViolet, 0.6) }
            }
        }
        Controls.ScrollView {
            id: bodyScroll
            objectName: "authenticationBodyScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            Layout.leftMargin: root.width < 500 ? 16 : 40
            Layout.rightMargin: root.width < 500 ? 16 : 40
            Layout.topMargin: 28
            Layout.bottomMargin: 28
            clip: true
            contentWidth: availableWidth
            contentHeight: body.implicitHeight
            Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff
            ColumnLayout {
                id: body
                width: bodyScroll.availableWidth
                spacing: 20
                HnLabel {
                    objectName: "requestMessage"
                    Layout.fillWidth: true
                    visible: text.length > 0
                    rawText: root.promptModel.requestMessage
                    role: HnTypographyRole.Subheading
                    wrapMode: Text.Wrap
                    color: HoloniightPalette.textPrimary
                }
                Rectangle {
                    objectName: "metadataCard"
                    Layout.fillWidth: true
                    implicitHeight: metadata.implicitHeight + 32
                    visible: root.promptModel.requestReference.length > 0 || root.requester.length > 0
                    radius: 6
                    color: HoloniightPalette.surfaceRaised
                    border.color: HoloniightPalette.borderPassive
                    ColumnLayout {
                        id: metadata
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 14
                        MetadataRow {
                            objectName: "actionDetails"
                            visible: value.length > 0
                            label: qsTr("Action")
                            value: root.promptModel.requestReference
                        }
                        Rectangle {
                            visible: root.promptModel.requestReference.length > 0 && root.requester.length > 0
                            Layout.fillWidth: true
                            implicitHeight: 1
                            color: HoloniightPalette.borderPassive
                        }
                        MetadataRow {
                            visible: value.length > 0
                            label: qsTr("Requested by")
                            value: root.requester
                            valueObjectName: "requesterDetails"
                        }
                    }
                }
                IdentitySelector {
                    id: identitySelector
                    Layout.fillWidth: true
                    visible: root.promptModel.frontendKind === 1 && count > 0
                    promptModel: root.promptModel
                    onNavigate: function(forward) { root.cycleFocus(forward) }
                }
                HnLabel {
                    objectName: "identityExplanation"
                    Layout.fillWidth: true
                    visible: root.selectingIdentity
                    rawText: qsTr("Continue with this account, or choose another account to authenticate.")
                    role: HnTypographyRole.Caption
                    wrapMode: Text.Wrap
                    color: HoloniightPalette.textMuted
                }
                MessageList { Layout.fillWidth: true; promptModel: root.promptModel }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumHeight: root.promptModel.frontendKind === 1 && !root.textInput ? 140 : 0
                    spacing: 12
                    HnLabel {
                        objectName: "promptLabel"
                        Layout.fillWidth: true
                        rawText: root.promptModel.currentPrompt || (root.textInput
                              ? (root.promptModel.inputMode === 2 ? qsTr("Password") : qsTr("Response")) : "")
                        visible: root.awaitingTextInput
                        role: HnTypographyRole.Subheading
                        wrapMode: Text.Wrap
                        color: HoloniightPalette.textPrimary
                    }
                    AuthenticationPrompt {
                        id: responseField
                        Layout.fillWidth: true
                        visible: root.awaitingTextInput
                        promptModel: root.promptModel
                        onNavigate: function(forward) { root.cycleFocus(forward) }
                        onSubmit: function(value) { root.promptModel.respond(value) }
                    }
                    HnLabel {
                        objectName: "responseHelper"
                        Layout.fillWidth: true
                        visible: root.awaitingTextInput
                        rawText: root.promptModel.inputMode === 1 ? qsTr("Enter the response requested by the authentication service.")
                              : root.promptModel.frontendKind === 1
                                ? qsTr("Your response is sent directly to the system authentication service.")
                                : qsTr("Your response is returned to the requesting application.")
                        role: HnTypographyRole.Caption
                        wrapMode: Text.Wrap
                        color: HoloniightPalette.textMuted
                    }
                }
                Controls.BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: root.promptModel.lifecycleState === 3
                    visible: running
                    Accessible.name: qsTr("Authenticating")
                }
            }
        }
        GridLayout {
            id: actionRow
            objectName: "actionRow"
            Layout.alignment: Qt.AlignRight
            Layout.rightMargin: root.width < 500 ? 16 : 40
            Layout.leftMargin: 16
            Layout.bottomMargin: 30
            columns: root.width < 500 ? 1 : 3
            columnSpacing: 20
            rowSpacing: 10
            visible: root.promptModel.inputMode !== 4
            ActionButton {
                id: cancelButton
                objectName: "cancelButton"
                text: qsTr("Cancel")
                hintKeys: [[Qt.Key_Escape]]
                enabled: root.promptModel.lifecycleState >= 1 && root.promptModel.lifecycleState <= 4
                onClicked: root.promptModel.cancel()
            }
            ActionButton {
                id: continueButton
                objectName: "continueButton"
                text: qsTr("Continue")
                hintKeys: [[Qt.Key_Return]]
                primary: true
                visible: root.selectingIdentity
                enabled: visible && identitySelector.currentIndex >= 0
                onClicked: identitySelector.confirmDisplayedIdentity()
            }
            ActionButton {
                id: authenticateButton
                objectName: "authenticateButton"
                text: qsTr("Authenticate")
                hintKeys: [[Qt.Key_Return]]
                primary: true
                visible: root.textInput && root.promptModel.lifecycleState !== 4
                enabled: root.promptModel.lifecycleState === 2
                onClicked: responseField.submitResponse()
            }
            ActionButton {
                id: retryButton
                objectName: "retryButton"
                text: qsTr("Retry")
                hintKeys: [[Qt.Key_Return]]
                primary: true
                visible: root.promptModel.lifecycleState === 4
                enabled: visible
                onClicked: root.promptModel.retry()
            }
            ActionButton {
                id: rejectButton
                objectName: "rejectButton"
                text: qsTr("Reject")
                visible: root.promptModel.inputMode === 3
                enabled: visible && root.promptModel.lifecycleState === 2
                onClicked: root.promptModel.confirm(false)
            }
            ActionButton {
                id: allowButton
                objectName: "allowButton"
                text: qsTr("Allow")
                primary: true
                visible: root.promptModel.inputMode === 3
                enabled: visible && root.promptModel.lifecycleState === 2
                onClicked: root.promptModel.confirm(true)
            }
        }
    }
    component MetadataRow: RowLayout {
        id: row
        required property string label
        required property string value
        property string valueObjectName: ""
        Layout.fillWidth: true
        spacing: 16
        HnLabel {
            Layout.preferredWidth: root.width < 500 ? 90 : 138
            rawText: row.label
            role: HnTypographyRole.Body
            wrapMode: Text.Wrap
            color: HoloniightPalette.textMuted
        }
        HnLabel {
            objectName: row.valueObjectName
            Layout.fillWidth: true
            rawText: row.value
            role: HnTypographyRole.Code
            wrapMode: Text.WrapAnywhere
            color: HoloniightPalette.textSecondary
        }
    }
    component ActionButton: Controls.Button {
        id: button
        property var hintKeys: []
        property bool primary: false
        leftPadding: 20
        rightPadding: 20
        implicitHeight: Math.max(54, buttonContent.implicitHeight + 20)
        implicitWidth: buttonContent.implicitWidth + 40
        focusPolicy: Qt.StrongFocus
        Accessible.name: text
        Accessible.ignored: !visible
        contentItem: RowLayout {
            id: buttonContent
            spacing: 18
            HnLabel {
                rawText: button.text
                role: HnTypographyRole.Subheading
                color: button.enabled ? HoloniightPalette.textPrimary : HoloniightPalette.textMuted
            }
            HnKeyHint { visible: button.hintKeys.length > 0; keyGroups: button.hintKeys; Accessible.ignored: true }
        }
        background: Rectangle {
            radius: 5
            color: button.primary ? Qt.alpha(HoloniightPalette.accentCyan, button.down ? 0.25 : 0.13)
                                  : HoloniightPalette.surfaceRaised
            border.color: button.activeFocus || button.primary ? HoloniightPalette.accentCyan : HoloniightPalette.borderPassive
            border.width: button.activeFocus ? 2 : 1
        }
        Keys.onTabPressed: function(event) { root.cycleFocus(true); event.accepted = true }
        Keys.onBacktabPressed: function(event) { root.cycleFocus(false); event.accepted = true }
        Keys.onReturnPressed: function(event) { button.clicked(); event.accepted = true }
        Keys.onEnterPressed: function(event) { button.clicked(); event.accepted = true }
    }
    Component.onCompleted: {
        root.resetSize()
        if (visible) Qt.callLater(root.focusInitialControl)
    }
    Connections {
        target: root.promptModel
        function onRequestTokenChanged() { Qt.callLater(root.resetSize) }
        function onLifecycleStateChanged() { Qt.callLater(root.focusInitialControl) }
        function onPromptChanged() { Qt.callLater(root.focusInitialControl) }
    }
    Shortcut { sequences: [StandardKey.Cancel]; onActivated: root.promptModel.cancel() }
}

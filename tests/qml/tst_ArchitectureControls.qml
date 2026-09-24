import QtQuick
import QtTest
import HolonightShell

TestCase {
    name: "ArchitectureControls"
    when: windowShown

    Component {
        id: profileComponent
        ProfileButton {
            profileName: "balanced"
            caption: qsTr("Balanced")
        }
    }

    Component {
        id: networkComponent
        NetworkToggleRow {}
    }

    function test_profileKeyboardAndDisabledState() {
        const button = createTemporaryObject(profileComponent, this)
        verify(button)
        const spy = signalSpy.createObject(this, { "target": button, "signalName": "activated" })
        verify(spy)
        compare(button.Accessible.role, Accessible.RadioButton)
        compare(button.Accessible.name, "Balanced")
        button.forceActiveFocus()
        verify(button.activeFocus)
        keyClick(Qt.Key_Space)
        compare(spy.count, 1)
        compare(spy.signalArguments[0][0], "balanced")

        button.isEnabled = false
        compare(button.enabled, false)
        compare(button.Accessible.ignored, false)
        keyClick(Qt.Key_Space)
        compare(spy.count, 1)
        mouseClick(button, button.width / 2, button.height / 2)
        compare(spy.count, 1)
    }

    function test_networkSwitchSemantics() {
        const row = createTemporaryObject(networkComponent, this)
        verify(row)
        const control = findChild(row, "wifiSwitch")
        verify(control)
        compare(control.Accessible.role, Accessible.CheckBox)
        compare(control.Accessible.name, "Wi-Fi")
        compare(control.Accessible.checked, true)
        control.forceActiveFocus()
        verify(control.activeFocus)
    }

    Component {
        id: signalSpy
        SignalSpy {}
    }
}

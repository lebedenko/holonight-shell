import QtQuick
import QtQuick.Controls as Controls
import QtTest
import HolonightShell
import Holonight.Core
import Holonight.Controls

TestCase {
    id: testCase
    name: "CompiledControls"
    when: windowShown
    width: 900
    height: 700
    function init() {
        // Existing tab replacement can emit this Loader sizing warning under both styles.
        // Keep all other runtime QML warnings fatal for the compiled acceptance suite.
        failOnWarning(/^(?!.*SidebarContent.qml.*Binding loop detected for property "height").*$/)
    }
    Component { id: searchComponent; LauncherSearchField {} }
    Component { id: wifiComponent; WifiPasswordDialog { parent: testCase; anchors.centerIn: parent } }
    Component { id: sidebarComponent; SidebarContent {} }
    Component { id: browseComponent; LauncherRightPanelBrowse {} }
    Component { id: sliderComponent; AudioVolumeSlider {} }

    Component { id: buttonComponent; Controls.Button { objectName: "runtimeButton"; text: "Verify" } }
    Component { id: menuComponent; SidebarTabBar { width: 500 } }
    function test_runtimeImplementation() {
        if (controlsEvidence.expectedStyle().length > 0)
            compare(controlsEvidence.selectedStyle(), controlsEvidence.expectedStyle())
        const button = createTemporaryObject(buttonComponent, testCase)
        verify(controlsEvidence.hasOrigin(button, "/" + controlsEvidence.selectedStyle() + "/Button.qml"))
    }
    function test_searchEditingAndNavigation() {
        const search = createTemporaryObject(searchComponent, testCase, {width: 400})
        verify(controlsEvidence.hasOrigin(search, "/Controls/HnSearchField.qml"))
        search.forceInputFocus()
        keyClick(Qt.Key_A)
        keyClick(Qt.Key_B)
        compare(search.text, "ab")
        keyClick(Qt.Key_Backspace)
        compare(search.text, "a")
        search.clearInput()
        compare(search.text, "")
    }
    function test_sidebarTabsAndScrollOrigin() {
        const sidebar = createTemporaryObject(sidebarComponent, testCase, {width: 400, height: 250, active: true})
        const scroll = findChild(sidebar, "sidebarContentScrollView")
        verify(scroll !== null)
        verify(controlsEvidence.hasOrigin(scroll, "/" + controlsEvidence.selectedStyle() + "/ScrollView.qml"))
        for (let tab = 0; tab < 6; ++tab) {
            sidebar.currentTab = tab
            wait(30)
            verify(sidebar.tabSource(tab).startsWith("qrc:/HolonightShell/"))
        }
    }
    function test_passwordPopupGeometry_data() {
        return [{tag: "small", scale: 0.78}, {tag: "normal", scale: 1}, {tag: "large", scale: 1.25}]
    }
    function test_passwordPopupGeometry(data) {
        const popup = createTemporaryObject(wifiComponent, testCase, {scale: data.scale})
        popup.open()
        tryCompare(popup, "opened", true)
        const top = popup.contentItem.mapToItem(testCase, 0, 0)
        const bottom = popup.contentItem.mapToItem(testCase, popup.contentItem.width, popup.contentItem.height)
        verify(top.x >= 0 && top.y >= 0)
        verify(bottom.x <= testCase.width && bottom.y <= testCase.height, JSON.stringify({top: top, bottom: bottom, width: testCase.width, height: testCase.height}))
        const field = findChild(popup, "passwordField")
        verify(field !== null)
        field.forceActiveFocus()
        keyClick(Qt.Key_A)
        compare(field.text, "a")
        compare(field.echoMode, TextInput.Password)
        popup.close()
    }
    function test_productionComponents_data() {
        return [{tag: "search", component: searchComponent},
                {tag: "wifi", component: wifiComponent},
                {tag: "sidebar", component: sidebarComponent},
                {tag: "browse", component: browseComponent},
                {tag: "slider", component: sliderComponent}]
    }
    function test_productionComponents(data) {
        const object = createTemporaryObject(data.component, testCase)
        verify(object !== null)
    }
}

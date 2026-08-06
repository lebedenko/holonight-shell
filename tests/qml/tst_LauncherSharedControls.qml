import QtQuick
import QtTest
import Holonight.Core
import Holonight.Controls
import HolonightShell

TestCase {
    id: root

    name: "LauncherSharedControlsQmlTests"
    when: windowShown

    Component {
        id: searchFieldComponent

        LauncherSearchField {
            width: 720
            height: 62
        }
    }

    Component {
        id: resultRowComponent

        LauncherResultRow {
            width: 600
            isBestMatch: true
            appName: "Firefox"
            appSubtitle: "Web Browser"
            appIconName: "firefox"
            appDesktopFile: "firefox.desktop"
        }
    }

    Component {
        id: browsePanelComponent

        LauncherRightPanelBrowse {
            width: 256
            height: 420
        }
    }

    Component {
        id: searchPanelComponent

        LauncherRightPanelSearch {
            width: 256
            height: 420
        }
    }

    function init() {
        LauncherService.setQuery("")
        LauncherService.setActiveCategory("")
        RecentAppsTracker.setEmpty(false)
    }

    function test_search_field_preserves_query_and_clear_contract() {
        const field = createTemporaryObject(searchFieldComponent, root)
        verify(field)
        compare(field.sizeRole, HnControlSize.Hero)
        compare(field.clearButtonVisible, false)

        field.text = "firefox"
        compare(LauncherService.query, "firefox")
        compare(field.text, "firefox")

        LauncherService.setQuery("terminal")
        compare(field.text, "terminal")

        field.clearInput()
        compare(field.text, "")
        compare(LauncherService.query, "")
    }

    function test_search_field_preserves_keyboard_signals_and_focus() {
        const field = createTemporaryObject(searchFieldComponent, root)
        verify(field)
        const moveSpy = signalSpy.createObject(field, { "target": field, "signalName": "moveSelection" })
        const launchSpy = signalSpy.createObject(field, { "target": field, "signalName": "launchRequested" })
        const closeSpy = signalSpy.createObject(field, { "target": field, "signalName": "closeRequested" })
        verify(moveSpy)
        verify(launchSpy)
        verify(closeSpy)

        field.forceInputFocus()
        verify(field.activeFocus)
        keyClick(Qt.Key_Down)
        compare(moveSpy.count, 1)
        compare(moveSpy.signalArguments[0][0], 1)
        keyClick(Qt.Key_Up)
        compare(moveSpy.count, 2)
        compare(moveSpy.signalArguments[1][0], -1)
        keyClick(Qt.Key_Return)
        compare(launchSpy.count, 1)

        field.text = "query"
        keyClick(Qt.Key_Escape)
        compare(field.text, "")
        compare(closeSpy.count, 0)
        keyClick(Qt.Key_Escape)
        compare(closeSpy.count, 1)
    }

    function test_result_row_maps_roles_and_shared_key_hint() {
        const row = createTemporaryObject(resultRowComponent, root, { "highlighted": true })
        verify(row)
        compare(row.title, "Firefox")
        compare(row.subtitle, "Web Browser")
        compare(row.appDesktopFile, "firefox.desktop")
        compare(row.selected, true)
        compare(row.sizeRole, HnControlSize.Hero)
        compare(row.implicitHeight, 76)
        tryVerify(function() { return row.trailingItem !== null })
        compare(row.trailingItem.text, "Enter")

        row.isBestMatch = false
        row.highlighted = false
        compare(row.sizeRole, HnControlSize.Large)
        compare(row.implicitHeight, 60)
        compare(row.dividerVisible, true)
        tryVerify(function() { return row.trailingItem === null })
    }

    function test_browse_panel_uses_shared_recent_category_and_empty_components() {
        const panel = createTemporaryObject(browsePanelComponent, root)
        verify(panel)

        const recent = findChild(panel, "recentApplicationDelegate-0")
        const category = findChild(panel, "launcherCategoryDelegate-0")
        const emptyState = findChild(panel, "recentApplicationsEmptyState")
        verify(recent)
        verify(category)
        verify(emptyState)
        compare(recent.title, "Firefox")
        compare(recent.height, 48)
        compare(category.title, "Internet")
        compare(category.badgeText, "5")
        compare(category.checked, false)
        compare(emptyState.titleText, "No recent apps")
        compare(emptyState.visible, false)

        category.clicked()
        compare(LauncherService.activeCategory, "Internet")
        compare(category.checked, true)

        RecentAppsTracker.setEmpty(true)
        tryCompare(panel, "recentAppsEmpty", true)
    }

    function test_search_filters_use_explicit_values_and_existing_adapter() {
        const panel = createTemporaryObject(searchPanelComponent, root)
        verify(panel)
        const control = findChild(panel, "launcherSearchFilterControl")
        verify(control)
        compare(control.modelCount(), 3)
        compare(control.valueAt(0), "all")
        compare(control.valueAt(1), "applications")
        compare(control.valueAt(2), "actions")
        compare(panel.activeFilter, "")
        compare(control.currentIndex, 0)

        control.activate(1)
        compare(panel.activeFilter, "apps")
        compare(control.currentIndex, 1)
        control.activate(2)
        compare(panel.activeFilter, "actions")
        compare(control.currentIndex, 2)
        panel.resetFilter()
        compare(panel.activeFilter, "")
        compare(control.currentIndex, 0)
    }

    Component {
        id: signalSpy

        SignalSpy {}
    }
}

import QtQuick
import QtTest
import Holonight.Core
import HolonightShell

TestCase {
    name: "RightSidebarSharedControlsQmlTests"
    width: 420
    height: 640
    when: windowShown

    Component {
        id: tabComponent

        SidebarTabButton {
            width: 220
            label: "Notifications"
            iconName: "notifications"
            isActive: true
            badgeCount: 3
        }
    }

    Component {
        id: notificationsComponent

        SidebarNotifications {
            width: 380
            height: 640
        }
    }

    Component {
        id: brightnessComponent

        BrightnessSlider {
            width: 320
        }
    }

    Component {
        id: defaultAppComponent

        DefaultAppRow {
            width: 320
            label: "Text Editor"
            mimeTypesForFilter: ["text/plain"]
            currentDefault: "selected.desktop"
        }
    }

    function test_tab_preserves_contract_and_routes_activation() {
        const tab = createTemporaryObject(tabComponent, this)
        verify(tab)
        compare(tab.objectName, "sidebarNavigationDelegate")
        compare(tab.title, "Notifications")
        compare(tab.badgeText, "3")
        verify(tab.selected)
        compare(tab.iconColor, HoloniightPalette.selectionIndicator)

        tab.isActive = false
        compare(tab.iconColor, HoloniightPalette.textSecondary)

        const spy = signalSpy.createObject(tab, { target: tab, signalName: "clicked" })
        verify(spy)
        tab.clicked()
        compare(spy.count, 1)
    }

    function test_notification_rules_use_shared_rows_and_combos() {
        const notifications = createTemporaryObject(notificationsComponent, this)
        verify(notifications)
        tryVerify(function() {
            return findChild(notifications, "notificationRuleRow") !== null
        })
        verify(findChild(notifications, "notificationRuleEnabled"))
        verify(findChild(notifications, "notificationUrgencyCombo"))
        verify(findChild(notifications, "notificationEmptyState"))
    }

    function test_brightness_preserves_throttled_control() {
        const brightness = createTemporaryObject(brightnessComponent, this)
        verify(brightness)
        compare(brightness.objectName, "brightnessSettingsRow")
        verify(brightness.stacked)
        verify(findChild(brightness, "brightnessControl"))
        verify(findChild(brightness, "writeThrottle"))
    }

    function test_default_app_syncs_after_shared_combo_loader_is_ready() {
        const row = createTemporaryObject(defaultAppComponent, this)
        verify(row)
        const combo = findChild(row, "defaultAppCombo")
        verify(combo)
        tryCompare(combo, "currentIndex", 1)
        compare(combo.displayText, "Selected Candidate")
        compare(combo.currentIconSource, "image://icon/selected-candidate-icon")

        row.currentDefault = "first.desktop"
        compare(combo.currentIndex, 0)
        compare(combo.displayText, "First Candidate")
        compare(combo.currentIconSource, "image://icon/first-candidate-icon")

        row.currentDefault = "missing.desktop"
        compare(combo.currentIndex, -1)
        compare(combo.displayText, "Current app unavailable")
        compare(combo.currentIconSource, "")
    }

    Component {
        id: signalSpy

        SignalSpy {}
    }
}

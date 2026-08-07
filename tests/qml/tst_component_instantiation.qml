import QtQuick
import QtTest
import Holonight.Core

import HolonightShell

TestCase {
    name: "ComponentInstantiationTests"

    Component {
        id: launcherSearchFieldComponent

        LauncherSearchField {
            width: 720
            height: 62
        }
    }

    Component {
        id: sidebarTabBarComponent

        SidebarTabBar {
            currentTab: 2
            active: true
            height: 420
        }
    }

    Component {
        id: sidebarNotificationsComponent

        SidebarNotifications {
            width: 380
            height: 300
        }
    }

    Component {
        id: sidebarContentComponent

        SidebarContent {
            width: 380
            height: 300
            currentTab: 2
            active: false
        }
    }

    Component {
        id: sidebarCalendarComponent

        SidebarOverviewCalendar {
            width: 380
        }
    }

    Component {
        id: rightSidebarComponent

        RightSidebar {
            barMonitorName: "DP-1"
            active: false
            width: 464
            height: 0
            panelHeight: 600
        }
    }

    Component {
        id: audioTabSidebarComponent

        AudioTabSidebar {
            currentTab: 1
        }
    }

    Component {
        id: keyboardStatusPopupComponent

        StatusPopup {
            popupId: "keyboard-layout"
            width: 420
            height: 260
        }
    }

    Component {
        id: wifiPasswordDialogComponent

        WifiPasswordDialog {}
    }

    Component {
        id: toastActionBarComponent

        ToastActionBar {
            notifId: 42
            width: 300
            accentColor: HoloniightPalette.accentCyan
            actions: [
                { "key": "reply", "label": "Reply" },
                { "key": "ignore", "label": "Ignore" }
            ]
        }
    }

    Component {
        id: timeToEventWidgetComponent

        WidgetSurface {
            widgetType: "time-to-event"
            barMonitorName: "DP-1"
            titleText: "Launch Event"
            remainingText: "02:14:05"
            deadlineLabelText: "June 20, 2026"
        }
    }

    Component {
        id: clockWidgetComponent

        WidgetSurface {
            widgetType: "clock"
            barMonitorName: "DP-1"
            timeText: "12:34"
            secondsText: "56"
            dateText: "Saturday, June 20"
        }
    }

    function test_launcher_search_field_tracks_fake_service_query() {
        LauncherService.setQuery("firefox")

        var searchField = createTemporaryObject(launcherSearchFieldComponent, null)
        verify(searchField !== null, "LauncherSearchField should instantiate with fake launcher service")
        compare(LauncherService.query, "firefox")

        searchField.clearInput()
        compare(LauncherService.query, "")
    }

    function test_sidebar_tab_bar_exposes_stable_tab_metadata() {
        var tabBar = createTemporaryObject(sidebarTabBarComponent, null)
        verify(tabBar !== null, "SidebarTabBar should instantiate")
        compare(tabBar.width, 224)
        compare(tabBar.currentTab, 2)
        compare(tabBar.active, true)
        compare(tabBar.tabMeta.length, 6)
        compare(tabBar.tabMeta[0].iconName, "overview")
        compare(tabBar.tabMeta[2].iconName, "notifications")

        const appTitle = findChild(tabBar, "shellAppTitle")
        verify(appTitle !== null)
        compare(appTitle.applicationName, "Shell")
        compare(appTitle.iconSource.toString(), "qrc:/HolonightShell/holonight-shell.svg")
    }

    function test_notification_sidebar_reports_preferred_height_beyond_viewport() {
        var notifications = createTemporaryObject(sidebarNotificationsComponent, null)
        verify(notifications !== null, "SidebarNotifications should instantiate with fake notification rules")
        tryVerify(function() { return notifications.preferredHeight > notifications.height })
    }

    function test_sidebar_content_includes_scroll_insets_in_preferred_height() {
        const content = createTemporaryObject(sidebarContentComponent, this)
        verify(content)
        compare(content.preferredHeightForContent(793), 793 + content.verticalInset)
        compare(content.preferredHeightForContent(0), 0)
    }

    function test_sidebar_calendar_uses_fake_calendar_service() {
        var calendar = createTemporaryObject(sidebarCalendarComponent, null)
        verify(calendar !== null, "SidebarOverviewCalendar should instantiate with fake calendar service")
        compare(calendar.dayHeaders[0], "Mon")
        compare(calendar.monthLabel(2026, 5), "June 2026")

        var days = calendar.buildDayModel(2026, 5, "Mon")
        compare(days.length, 35)
        compare(days[0].day, 1)
        compare(days[0].isCurrentMonth, true)
        compare(days[5].isWeekend, true)
    }

    function test_right_sidebar_defers_open_until_surface_has_height() {
        var sidebar = createTemporaryObject(rightSidebarComponent, null)
        verify(sidebar !== null, "RightSidebar should instantiate")

        sidebar.active = true
        compare(sidebar.openDeferredUntilSized, true)

        sidebar.height = 760
        compare(sidebar.openDeferredUntilSized, false)
    }

    function test_right_sidebar_finishes_open_at_latest_requested_height() {
        const sidebar = createTemporaryObject(rightSidebarComponent, this)
        verify(sidebar)
        sidebar.height = 900
        sidebar.active = true

        const clip = findChild(sidebar, "sidebarClipContainer")
        verify(clip)
        sidebar.panelHeight = 760
        wait(650)
        compare(clip.height, 760)
    }

    function test_status_popup_placeholder_variant() {
        var popup = createTemporaryObject(keyboardStatusPopupComponent, null)
        verify(popup !== null, "Keyboard status popup should instantiate")
        compare(popup.popupId, "keyboard-layout")
        compare(popup.displayTitle, "Keyboard Layout")
        compare(popup.contentSource, "")
        compare(popup.showTitle, true)
        compare(popup.panelLeft, 24)
        compare(popup.panelTop, 15)
        compare(popup.panelShapeProfile.kind, HnShapeKind.Hybrid)
        compare(popup.panelChamferedCorners, HnCornerMask.TopRight | HnCornerMask.BottomLeft)
        compare(popup.panelFillColor, HoloniightPalette.surfaceRaised)
        compare(popup.panelBorderColor, HoloniightPalette.borderPassive)
        compare(popup.panelBorderWidth, HnMetrics.borderWidth)
    }

    function test_wifi_password_dialog_uses_semantic_popup_frame() {
        const dialog = createTemporaryObject(wifiPasswordDialogComponent, null)
        verify(dialog !== null, "Wi-Fi password dialog should instantiate")
        compare(dialog.background.surfaceRole, HnSurfaceRole.Popup)
        compare(dialog.background.fillColor, HoloniightPalette.surfaceRaised)
        compare(dialog.background.borderColor, HoloniightPalette.borderPassive)
        compare(dialog.background.borderWidth, HnMetrics.borderWidth)
    }

    function test_audio_tab_sidebar_exposes_expected_tabs() {
        var sidebar = createTemporaryObject(audioTabSidebarComponent, null)
        verify(sidebar !== null, "AudioTabSidebar should instantiate")
        compare(sidebar.currentTab, 1)
        compare(sidebar.implicitWidth, 180)
        compare(sidebar.tabLabels.length, 3)
        compare(sidebar.tabLabels[0], "Output Devices")
        compare(sidebar.tabLabels[1], "Input Devices")
        compare(sidebar.tabLabels[2], "Applications")
    }

    function test_toast_action_bar_instantiates_actions() {
        var actionBar = createTemporaryObject(toastActionBarComponent, null)
        verify(actionBar !== null, "ToastActionBar should instantiate")
        compare(actionBar.notifId, 42)
        compare(actionBar.actions.length, 2)
        compare(actionBar.actions[0].key, "reply")
        compare(actionBar.actions[1].label, "Ignore")
    }

    function test_widget_surface_time_to_event_properties() {
        var widget = createTemporaryObject(timeToEventWidgetComponent, null)
        verify(widget !== null, "Time-to-event widget surface should instantiate")
        compare(widget.widgetType, "time-to-event")
        compare(widget.barMonitorName, "DP-1")
        compare(widget.titleText, "Launch Event")
        compare(widget.remainingText, "02:14:05")
        compare(widget.deadlineLabelText, "June 20, 2026")
    }

    function test_widget_surface_clock_properties() {
        var widget = createTemporaryObject(clockWidgetComponent, null)
        verify(widget !== null, "Clock widget surface should instantiate")
        compare(widget.widgetType, "clock")
        compare(widget.barMonitorName, "DP-1")
        compare(widget.timeText, "12:34")
        compare(widget.secondsText, "56")
        compare(widget.dateText, "Saturday, June 20")
    }
}

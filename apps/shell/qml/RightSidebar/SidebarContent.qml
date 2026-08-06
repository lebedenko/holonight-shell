import QtQuick

import QtQuick.Controls as Controls

import HolonightShell

Item {
    id: root

    property int currentTab: 0
    property bool active: false
    property real contentOpacity: 0.0

    readonly property int verticalInset: 24

    // Bubbles up from the loaded tab component so RightSidebar can resize the surface.
    // qmllint disable missing-property
    property int preferredHeight: root.preferredHeightForContent(loader.item?.preferredHeight ?? 0)
    // qmllint enable missing-property

    // Tab switch requested by a child component (e.g. "View all" in overview notifications).
    signal switchTab(int index)

    readonly property var tabDefinitions: [
        { source: "qrc:/HolonightShell/RightSidebar/Tabs/Overview/SidebarOverview.qml", width: 360 },
        { source: "qrc:/HolonightShell/RightSidebar/Tabs/Calendar/SidebarCalendar.qml", width: 400 },
        { source: "qrc:/HolonightShell/RightSidebar/Tabs/Notifications/SidebarNotifications.qml", width: 380 },
        { source: "qrc:/HolonightShell/RightSidebar/Tabs/System/SidebarSystem.qml", width: 340 },
        { source: "qrc:/HolonightShell/RightSidebar/Tabs/QuickSettings/SidebarQuickSettings.qml", width: 320 },
        { source: "qrc:/HolonightShell/RightSidebar/Tabs/Media/SidebarMedia.qml", width: 300 }
    ]

    function preferredWidthForTab(idx) {
        return root.tabDefinitions[idx]?.width ?? root.tabDefinitions[0].width
    }

    function tabSource(idx) {
        return root.tabDefinitions[idx]?.source ?? root.tabDefinitions[0].source
    }

    function preferredHeightForContent(contentHeight) {
        return contentHeight > 0 ? contentHeight + root.verticalInset : 0
    }

    Connections {
        target: loader.item
        ignoreUnknownSignals: true
        function onSwitchTab(index) { root.switchTab(index) }
    }

    Controls.ScrollView {
        id: scrollView

        objectName: "sidebarContentScrollView"
        anchors.fill: parent
        anchors.topMargin: root.verticalInset / 2
        anchors.bottomMargin: root.verticalInset / 2
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        clip: true
        opacity: root.contentOpacity
        contentWidth: availableWidth
        contentHeight: loader.height
        Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff
        Controls.ScrollBar.vertical.policy: contentHeight > availableHeight
                                            ? Controls.ScrollBar.AsNeeded
                                            : Controls.ScrollBar.AlwaysOff

        Loader {
            id: loader

            width: Math.max(0, root.width - 16)
            // Let a tab request its natural content height so the surface can grow. If screen
            // bounds clamp that height, ScrollView keeps the overflowing content scrollable.
            // qmllint disable missing-property
            height: Math.max(scrollView.availableHeight, loader.item?.preferredHeight ?? 0)
            // qmllint enable missing-property
            // Pausing the Loader when hidden destroys the tab component, stopping all its
            // timers and signal connections (REQ-NF-001/002).
            active: root.active
            source: root.tabSource(root.currentTab)
        }
    }

}

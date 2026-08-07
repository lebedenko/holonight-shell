import QtQuick
import QtQuick.Shapes
import Holonight.Core
import HolonightShell
import "../Controls"

import "../Utility" as Utility

Item {
    id: root

    required property string barMonitorName
    required property bool active

    Utility.AppearanceReloadBridge {}

    property bool ready: false
    property bool openDeferredUntilSized: false
    property int currentTab: 0
    property int panelHeight: 600

    readonly property int topMargin: 8
    readonly property int rightMargin: 24
    readonly property int bottomMargin: 24
    readonly property int panelWidth: tabBar.width + contentArea.width
    readonly property int boundedPanelHeight: Math.max(0, Math.min(root.panelHeight, root.height - root.topMargin - root.bottomMargin))

    focus: true
    Keys.onEscapePressed: SidebarManager.close(root.barMonitorName)

    MouseArea {
        anchors.fill: parent
        enabled: root.active
        onClicked: SidebarManager.close(root.barMonitorName)
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: "red"
        border.width: 1
        visible: AppearanceService.debugOverlays
        z: 99999
    }

    Component.onCompleted: root.ready = true

    onActiveChanged: {
        if (!root.ready) {
            return
        }
        if (root.active) {
            closeAnimation.stop()
            tabSwitchAnimation.stop()
            if (root.boundedPanelHeight > 0) {
                root.openDeferredUntilSized = false
                openAnimation.start()
            } else {
                root.openDeferredUntilSized = true
                clipContainer.height = 0
                contentArea.width = 0
                contentArea.contentOpacity = 0.0
            }
        } else {
            root.openDeferredUntilSized = false
            openAnimation.stop()
            tabSwitchAnimation.stop()
            closeAnimation.start()
        }
    }

    onHeightChanged: {
        if (root.active) {
            clipContainer.height = root.boundedPanelHeight
        }
    }

    onBoundedPanelHeightChanged: {
        if (root.active && root.openDeferredUntilSized && root.boundedPanelHeight > 0) {
            root.openDeferredUntilSized = false
            openAnimation.start()
        } else if (root.active) {
            clipContainer.height = root.boundedPanelHeight
        }
    }

    onCurrentTabChanged: {
        SidebarManager.onCurrentTabChanged(root.barMonitorName, root.currentTab)
        if (!root.active) {
            return
        }
        tabSwitchAnimation.to = contentArea.preferredWidthForTab(root.currentTab)
        tabSwitchAnimation.start()
    }

    // Clip container — grows from 0 to full height during phase 1, then widens during phase 2.
    // Anchored to the right: as width grows it expands leftward (tab bar + content slide in together).
    Item {
        id: clipContainer

        objectName: "sidebarClipContainer"
        anchors.top: parent.top
        anchors.topMargin: root.topMargin
        anchors.right: parent.right
        anchors.rightMargin: root.rightMargin
        width: root.panelWidth
        height: 0
        clip: true

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        // Panel background and border — must be first so it renders below content.
        HudFrame {
            id: panelFrame

            anchors.fill: parent
            variant: HudFrame.Sidebar
        }

        // Subtle overlay on tab bar column to distinguish it from the content area.
        Item {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: tabBar.width
            clip: true

            Shape {
                x: panelFrame.frameInset
                y: panelFrame.frameInset
                width: clipContainer.width - 2 * panelFrame.frameInset
                height: clipContainer.height - 2 * panelFrame.frameInset
                preferredRendererType: Shape.CurveRenderer
                Accessible.ignored: true

                ShapePath {
                    fillColor: Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                                       HoloniightPalette.surface.b, 0.3)
                    strokeColor: "transparent"

                    PathSvg {
                        path: panelFrame.pathData
                    }
                }
            }
        }

        SidebarTabBar {
            id: tabBar

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            currentTab: root.currentTab
            active: root.active
            onTabSelected: (idx) => {
                root.currentTab = idx
            }
        }

        SidebarContent {
            id: contentArea

            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: tabBar.right
            width: 0
            currentTab: root.currentTab
            active: root.active
        }
    }

    Connections {
        target: contentArea
        function onPreferredHeightChanged() {
            if (contentArea.preferredHeight > 0) {
                SidebarManager.onContentHeightChanged(root.barMonitorName, contentArea.preferredHeight)
            }
        }
        function onSwitchTab(index) { root.currentTab = index }
    }

    // Phase 1 grow-down → phase 2 slide-in → phase 3 fade-in.
    SequentialAnimation {
        id: openAnimation

        onFinished: clipContainer.height = root.boundedPanelHeight

        NumberAnimation {
            target: clipContainer
            property: "height"
            from: 0
            to: root.boundedPanelHeight
            duration: 160
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: contentArea
            property: "width"
            from: 0
            to: contentArea.preferredWidthForTab(root.currentTab)
            duration: 200
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: contentArea
            property: "contentOpacity"
            from: 0.0
            to: 1.0
            duration: 120
            easing.type: Easing.OutCubic
        }
    }

    // Reverse: fade-out → slide-left → shrink-up → hide via C++ callback.
    SequentialAnimation {
        id: closeAnimation

        NumberAnimation {
            target: contentArea
            property: "contentOpacity"
            from: 1.0
            to: 0.0
            duration: 100
            easing.type: Easing.InCubic
        }
        NumberAnimation {
            target: contentArea
            property: "width"
            to: 0
            duration: 160
            easing.type: Easing.InCubic
        }
        NumberAnimation {
            target: clipContainer
            property: "height"
            to: 0
            duration: 140
            easing.type: Easing.InCubic
        }
        ScriptAction {
            script: SidebarManager.onClosingAnimationFinished(root.barMonitorName)
        }
    }

    // Tab switch: width-only, no grow or fade.
    NumberAnimation {
        id: tabSwitchAnimation

        target: contentArea
        property: "width"
        duration: 180
        easing.type: Easing.OutCubic
    }

    function preferredWidthForTab(idx) {
        return contentArea.preferredWidthForTab(idx)
    }
}

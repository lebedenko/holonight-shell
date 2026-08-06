pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import HolonightShell
import Holonight.Core
import Holonight.Components

import "../Topbar"

Item {
    id: root

    required property int index
    required property int size
    property bool normalizeIcon: false
    required property string barMonitorName
    required property string service
    required property string objectPath
    required property string iconName
    required property string attentionIconName
    required property string iconPixmapUrl
    required property string attentionPixmapUrl
    required property string status
    required property string title
    required property string itemKey
    required property string tooltipTitle
    required property string tooltipDescription
    required property string tooltipIconName
    required property bool hasUnread
    property bool overflowVisible: true
    readonly property int visualIconSize: Math.min(22, Math.max(18, Math.round(root.size * 0.6)))
    readonly property bool needsAttention: root.status === "NeedsAttention"
    readonly property bool showShellBadge: root.needsAttention || root.hasUnread
    readonly property string effectiveIconName: root.needsAttention && root.attentionIconName !== ""
                                               ? root.attentionIconName
                                               : root.iconName
    readonly property string effectivePixmapUrl: root.needsAttention && root.attentionPixmapUrl !== ""
                                                ? root.attentionPixmapUrl
                                                : root.iconPixmapUrl

    width: root.size
    height: root.size
    // opacity-gated (not root.status directly) so the Passive-state fade-out Behavior below
    // gets to play before the item disappears, instead of vanishing on the same frame.
    visible: root.opacity > 0 && root.overflowVisible
    Accessible.role: Accessible.Button
    Accessible.name: root.tooltipTitle.length > 0 ? root.tooltipTitle
                     : root.title.length > 0 ? root.title
                     : "Tray item"
    Accessible.description: root.tooltipDescription.length > 0 ? root.tooltipDescription
                            : root.status
    Accessible.ignored: !root.visible

    // Slide-in from left when appearing in a slot.
    property real slideOffset: 0.0
    transform: Translate { x: root.slideOffset }

    Component.onCompleted: {
        root.slideOffset = -(root.size + 8)
        slideAnim.start()
        root.opacity = root.status !== "Passive" ? 1.0 : 0.0
    }

    NumberAnimation {
        id: slideAnim
        target: root
        property: "slideOffset"
        to: 0
        duration: 100
        easing.type: Easing.OutCubic
    }

    opacity: 0.0
    Behavior on opacity {
        NumberAnimation { duration: 100; easing.type: Easing.OutCubic }
    }

    onStatusChanged: root.opacity = root.status !== "Passive" ? 1.0 : 0.0

    Rectangle {
        anchors.fill: parent
        radius: 7
        color: hoverHandler.hovered
            ? Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                      HoloniightPalette.surface.b, 0.72)
            : Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                      HoloniightPalette.surface.b, 0.36)
        border.color: hoverHandler.hovered
            ? Qt.rgba(HoloniightPalette.borderActive.r, HoloniightPalette.borderActive.g,
                      HoloniightPalette.borderActive.b, 0.1)
            : Qt.rgba(HoloniightPalette.borderPassive.r, HoloniightPalette.borderPassive.g,
                      HoloniightPalette.borderPassive.b, 0.1)
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
        }

        Behavior on border.color {
            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    // MultiEffect must be declared BEFORE the icon to render behind it.
    // qmllint disable incompatible-type
    MultiEffect {
        source: iconVisual
        anchors.fill: iconVisual
        visible: hoverHandler.hovered
        shadowEnabled: true
        shadowColor: HoloniightPalette.accentCyan
        shadowBlur: 0.58
        shadowOpacity: 0.26
        shadowScale: 1.04
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    Item {
        id: iconVisual
        anchors.centerIn: parent
        width: root.visualIconSize
        height: root.visualIconSize
        opacity: root.normalizeIcon
            ? (hoverHandler.hovered ? 0.9 : 0.78)
            : (hoverHandler.hovered ? 1.0 : 0.92)

        Behavior on opacity {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }

        ExternalIcon {
            objectName: "externalIcon"
            anchors.fill: parent
            iconName: root.effectiveIconName
            pixmapUrl: root.effectivePixmapUrl
            iconSize: root.visualIconSize
        }
    }
    // qmllint enable incompatible-type

    // Urgent badge: violet glowing dot at bottom-right corner of the icon.
    // MultiEffect must be declared BEFORE the dot to render behind it.
    MultiEffect {
        source: badgeDot
        anchors.fill: badgeDot
        visible: root.showShellBadge
        shadowEnabled: true
        shadowColor: HoloniightPalette.accentViolet
        shadowBlur: 0.7
        shadowOpacity: badgePulse.value
        shadowScale: 1.6
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    Rectangle {
        id: badgeDot
        width: 6
        height: 6
        radius: 3
        color: HoloniightPalette.accentViolet
        visible: root.showShellBadge
        anchors {
            right: iconVisual.right
            bottom: iconVisual.bottom
        }
    }

    SequentialAnimation {
        id: badgePulse
        running: root.showShellBadge
        loops: Animation.Infinite

        property real value: 0.0

        NumberAnimation {
            target: badgePulse
            property: "value"
            to: 0.9
            duration: 900
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: badgePulse
            property: "value"
            to: 0.25
            duration: 900
            easing.type: Easing.InOutSine
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onPressed: tooltipArea.dismissForClick()
        onClicked: (mouse) => {
            if (root.status === "Passive") { return }
            var pos = root.mapToGlobal(0, 0)
            if (mouse.button === Qt.LeftButton) {
                TrayModel.activate(root.itemKey, pos.x, pos.y)
            } else if (mouse.button === Qt.RightButton) {
                TrayModel.openContextMenu(root.itemKey, pos.x, pos.y)
            } else if (mouse.button === Qt.MiddleButton) {
                TrayModel.secondaryActivate(root.itemKey, pos.x, pos.y)
            }
        }
    }

    WheelHandler {
        onWheel: (event) => {
            if (root.status === "Passive") { return }
            var delta = event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x
            var orientation = event.angleDelta.y !== 0 ? "vertical" : "horizontal"
            TrayModel.scroll(root.itemKey, delta, orientation)
        }
    }

    HoverHandler { id: hoverHandler }

    BarTooltipArea {
        id: tooltipArea

        barMonitorName: root.barMonitorName
        title: root.tooltipTitle.length > 0 ? root.tooltipTitle
             : root.title.length > 0 ? root.title
             : "Tray item"
        description: root.tooltipDescription.length > 0 ? root.tooltipDescription
                   : root.status
        iconName: root.tooltipIconName.length > 0 ? root.tooltipIconName : root.iconName
        anchorToPointerX: true
        blockRetriggerUntilExit: true
    }
}

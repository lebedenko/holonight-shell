import QtQuick
import QtQuick.Effects
import Holonight.Core

import HolonightShell

// Sidebar quick-action that holds a logind idle inhibitor while toggled on.
// Mirrors the ProfileButton visual pattern: circular disc, tinted symbolic icon, glow when active.
Item {
    id: root

    implicitWidth: 52
    implicitHeight: 52

    readonly property bool active: IdleService.idleInhibited

    scale: hoverHandler.hovered ? 1.06 : 1.0
    transformOrigin: Item.Center
    Behavior on scale {
        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }

    MultiEffect {
        source: disc
        anchors.fill: disc
        visible: root.active
        shadowEnabled: true
        shadowColor: HoloniightPalette.accentCyan
        shadowBlur: 0.6
        shadowOpacity: 0.5
        shadowScale: 1.03
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    Rectangle {
        id: disc
        anchors.fill: parent
        radius: width / 2
        color: root.active
            ? Qt.rgba(HoloniightPalette.accentCyan.r, HoloniightPalette.accentCyan.g,
                      HoloniightPalette.accentCyan.b, 0.16)
            : Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                      HoloniightPalette.surface.b,
                      hoverHandler.hovered ? 0.55 : 0.3)
        border.width: root.active ? 1.5 : 1
        border.color: root.active
            ? HoloniightPalette.accentCyan
            : hoverHandler.hovered
                ? Qt.rgba(HoloniightPalette.accentCyan.r, HoloniightPalette.accentCyan.g,
                          HoloniightPalette.accentCyan.b, 0.45)
                : Qt.rgba(HoloniightPalette.borderPassive.r, HoloniightPalette.borderPassive.g,
                          HoloniightPalette.borderPassive.b, 0.6)

        Behavior on color {
            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
        Behavior on border.color {
            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    Image {
        id: glyph
        anchors.centerIn: parent
        width: 24
        height: 24
        sourceSize.width: 24
        sourceSize.height: 24
        source: "image://icon/weather-clear-night-symbolic"
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    MultiEffect {
        source: glyph
        anchors.fill: glyph
        colorization: 1.0
        colorizationColor: root.active
            ? HoloniightPalette.accentCyan
            : hoverHandler.hovered
                ? HoloniightPalette.textPrimary
                : HoloniightPalette.textMuted

        Behavior on colorizationColor {
            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    HoverHandler { id: hoverHandler }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: false
        cursorShape: Qt.PointingHandCursor
        onClicked: IdleService.idleInhibited = !IdleService.idleInhibited
    }
}

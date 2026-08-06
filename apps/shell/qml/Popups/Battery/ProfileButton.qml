import QtQuick
import QtQuick.Effects
import Holonight.Core

// A circular power-profile toggle. The glyph comes from the system icon theme; theme symbolic icons
// are near-black (#222) so they are recolored to the state color with HnIcon. The caption is owned
// by the parent and only shown on hover (this control exposes `hovered` + `caption`).
//
// States: active (cyan disc fill + sharp cyan border + cyan glow), inactive (muted disc), and
// disabled (dimmed, non-interactive, forbidden cursor). REQ-F-014..018, REQ-F-024, REQ-F-025.
Item {
    id: root

    property string profileName: ""
    property string iconName: ""
    property string caption: ""
    property bool isActive: false
    property bool isEnabled: true

    readonly property alias hovered: hoverHandler.hovered

    signal activated(string profile)

    implicitWidth: 52
    implicitHeight: 52
    opacity: root.isEnabled ? 1.0 : 0.45

    scale: (root.isEnabled && hoverHandler.hovered) ? 1.06 : 1.0
    transformOrigin: Item.Center
    Behavior on scale {
        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }

    // Cyan ambient glow behind the disc for the active profile. Declared before the disc so it
    // renders behind it (MultiEffect z-order rule).
    MultiEffect {
        source: disc
        anchors.fill: disc
        visible: root.isActive
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
        color: root.isActive
            ? Qt.rgba(HoloniightPalette.accentCyan.r, HoloniightPalette.accentCyan.g,
                      HoloniightPalette.accentCyan.b, 0.16)
            : Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                      HoloniightPalette.surface.b,
                      (root.isEnabled && hoverHandler.hovered) ? 0.55 : 0.3)
        border.width: root.isActive ? 1.5 : 1
        border.color: root.isActive
            ? HoloniightPalette.accentCyan
            : (root.isEnabled && hoverHandler.hovered)
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

    // qmllint disable import unresolved-type
    HnIcon {
        id: glyph
        anchors.centerIn: parent
        size: 24
        source: root.iconName
        tinted: true
        iconState: HnIcon.Normal
        normalColor: root.isActive
            ? HoloniightPalette.accentCyan
            : (root.isEnabled && hoverHandler.hovered)
                ? HoloniightPalette.textPrimary
                : HoloniightPalette.textMuted

        Behavior on normalColor {
            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }
    // qmllint enable import unresolved-type

    HoverHandler {
        id: hoverHandler
        enabled: root.isEnabled
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: false
        cursorShape: root.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
        onClicked: {
            if (root.isEnabled) {
                root.activated(root.profileName)
            }
        }
    }
}

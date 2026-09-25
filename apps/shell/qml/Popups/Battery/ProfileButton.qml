import QtQuick
import QtQuick.Effects
import QtQuick.Controls as Controls
import Holonight.Core

// A circular power-profile toggle. The glyph comes from the system icon theme; theme symbolic icons
// are near-black (#222) so they are recolored to the state color with HnIcon. The caption is owned
// by the parent and shown on hover or keyboard focus.
//
// States: active (cyan disc fill + sharp cyan border + cyan glow), inactive (muted disc), and
// disabled (dimmed and non-interactive). REQ-F-014..018, REQ-F-024, REQ-F-025.
Controls.RadioButton {
    id: root

    property string profileName: ""
    property string iconName: ""
    property string caption: ""
    property bool isActive: false
    property bool isEnabled: true

    signal activated(string profile)

    implicitWidth: 52
    implicitHeight: 52
    enabled: root.isEnabled
    checked: root.isActive
    Accessible.role: Accessible.RadioButton
    Accessible.name: root.caption
    Accessible.checked: checked
    opacity: root.enabled ? 1.0 : 0.45
    onClicked: root.activated(root.profileName)

    scale: (root.enabled && root.hovered) ? 1.06 : 1.0
    transformOrigin: Item.Center
    Behavior on scale {
        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }

    // Cyan ambient glow behind the disc for the active profile. Declared before the disc so it
    // renders behind it (MultiEffect z-order rule).
    contentItem: Item {}
    indicator: Item {
      implicitWidth: 52
      implicitHeight: 52
      x: 0
      y: (root.height - height) / 2

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
                          (root.enabled && root.hovered) ? 0.55 : 0.3)
            border.width: root.activeFocus ? 2 : root.isActive ? 1.5 : 1
            border.color: root.isActive
                ? HoloniightPalette.accentCyan
                : (root.activeFocus || root.hovered)
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
            name: root.iconName
            rendering: HnIcon.Semantic
            iconState: HnIcon.Normal
            normalColor: root.isActive
                ? HoloniightPalette.accentCyan
                : (root.enabled && root.hovered)
                    ? HoloniightPalette.textPrimary
                    : HoloniightPalette.textMuted

            Behavior on normalColor {
                ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
            }
        }
        // qmllint enable import unresolved-type

    }
}

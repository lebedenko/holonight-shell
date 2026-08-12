import QtQuick
import QtQuick.Effects
import Holonight.Core
import Holonight.Components

// Forces a solid tint on device/UI icons regardless of whether the resolved theme SVG carries
// KDE-style ColorScheme-* classes (ExternalIcon.usesSemanticTint). On this system's icon theme,
// names like "audio-card"/"audio-input-microphone"/"audio-volume-high" have no such markup, so
// plain ExternalIcon falls through to an un-tinted Image and renders near-black/invisible on the
// dark popup surface. MultiEffect colorization overlay is the documented recoloring workaround
// for theme-provided pixmaps (CLAUDE.md "Recoloring symbolic theme icons"). Not used for
// AudioStreamDelegate's per-app icons — those intentionally keep their original brand colors.
Item {
  id: root

  property string iconName: ""
  property string fallbackIconName: ""
  property int iconSize: 24
  property color tintColor: HoloniightPalette.textSecondary

  implicitWidth: root.iconSize
  implicitHeight: root.iconSize

  ExternalIcon {
    id: sourceIcon
    anchors.fill: parent
    iconName: root.iconName
    fallbackIconName: root.fallbackIconName
    iconSize: root.iconSize
    tintColor: root.tintColor
  }

  MultiEffect {
    objectName: "audioIconTintEffect"
    anchors.fill: sourceIcon
    source: sourceIcon
    brightness: 1.0
    colorization: 1.0
    colorizationColor: root.tintColor
  }
}

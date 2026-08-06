pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import Holonight.Core

import HolonightShell

// OSD view root: the card surface, the entrance/exit choreography, and the dispatch between the two
// content renderers. Everything C++ pushes lands on the properties below; nothing here talks to a
// service directly.
//
// Named OsdView, not OsdSurface, because the C++ layer-shell class already exports the type name
// `OsdSurface` into the HolonightShell module -- a QML file of the same name makes every import of
// the module ambiguous, which qmllint reports project-wide.
//
// REQ-F-024: this view never accepts input -- no pointer handlers and no keyboard focus, here or in
// either renderer. The surface is click-through at the Wayland level (an empty input region) and
// nothing in QML may quietly take that back.
Item {
  id: root

  // Injected from OsdSurface::createSurface via setInitialProperties. Not read by the view -- kept
  // so the monitor a given OSD instance belongs to is visible when debugging a multi-monitor issue.
  required property string monitorName

  property string kind: "level"  // "level" | "selection"
  property string channel: ""
  property int value: 0
  property bool muted: false
  property string shortLabel: ""
  property string fullLabel: ""
  property bool hiding: false
  property bool poseAnimationEnabled: true

  // REQ-NF-009: false until LayerSurface::configured() fires, so the entrance never plays against a
  // surface the compositor has not mapped yet.
  property bool configured: false

  // The view's own record of which channel is on screen. Distinct from OsdController's
  // current_channel_, which exists for the controller's own timer logic -- these two are never
  // compared and do not need to agree.
  property string currentChannel: ""

  // Transparent room around the card for the glow to bleed into. The C++ side reads this root's
  // implicit size verbatim, so the margin is counted exactly once, here.
  readonly property int glowMargin: 16

  // Loader.item is typed QObject, so it is narrowed once here rather than at each use.
  readonly property Item contentItem: contentLoader.item as Item
  readonly property real cardWidth: root.contentItem ? root.contentItem.implicitWidth : 0
  readonly property real cardHeight: root.contentItem ? root.contentItem.implicitHeight : 0

  implicitWidth: root.cardWidth > 0 ? root.cardWidth + root.glowMargin * 2 : 0
  implicitHeight: root.cardHeight > 0 ? root.cardHeight + root.glowMargin * 2 : 0

  opacity: 0
  scale: 0.85

  // Gated on the animated property, not on `hiding`: `visible: false` takes effect on the frame the
  // condition flips and would cut the fade-out off before it renders (CLAUDE.md).
  visible: root.configured && root.opacity > 0

  // REQ-F-016: 200 ms in, 150 ms out.
  Behavior on opacity {
    enabled: root.poseAnimationEnabled

    NumberAnimation {
      id: opacityAnim

      duration: root.hiding ? 150 : 200
      easing.type: Easing.OutCubic
      // The exit is only complete once opacity has actually reached 0 while hiding; every other
      // stop of this animation (entrance, same-channel update, replay) leaves the OSD on screen.
      onRunningChanged: {
        if (opacityAnim.running || !root.hiding || root.opacity > 0)
          return;
        OsdSurface.onHideAnimationFinished();
      }
    }
  }

  Behavior on scale {
    enabled: root.poseAnimationEnabled

    NumberAnimation {
      duration: root.hiding ? 150 : 200
      easing.type: Easing.OutCubic
    }
  }

  // The single place that decides update-in-place vs. replay. Called explicitly by OsdSurface after
  // every content push -- deliberately not an `onChannelChanged` handler. Consecutive events on one
  // channel leave `channel` untouched, so a change-driven show simply would not run; if the previous
  // event's fade-out were still in flight the OSD would settle at opacity 0 with `hiding` already
  // cleared, which suppresses the teardown too and strands the surface alive but permanently blank.
  function present(): void {
    if (!root.configured)
      return;
    if (root.channel !== root.currentChannel) {
      // REQ-F-018: a different channel snaps back to the entrance start, so the 200 ms entrance
      // replays without a hide-then-show gap.
      root.currentChannel = root.channel;
      root.poseAnimationEnabled = false;
      root.opacity = 0;
      root.scale = 0.85;
      root.poseAnimationEnabled = true;
    }
    // Cleared before the opacity write so the Behavior picks the 200 ms entrance duration, and so a
    // show arriving mid-fade-out retargets that animation back to 1 instead of finishing the exit.
    root.hiding = false;
    // REQ-F-017: same channel while already shown -- these are no-ops and only the renderers' own
    // Behaviors animate.
    root.opacity = 1;
    root.scale = 1;
  }

  onHidingChanged: {
    if (!root.hiding)
      return;
    if (root.opacity === 0) {
      // Already invisible, so assigning 0 starts no animation and the exit callback below -- which
      // is what requests the teardown -- would never fire, leaking the surface.
      OsdSurface.onHideAnimationFinished();
      return;
    }
    root.opacity = 0;
    root.scale = 0.85;
  }

  // Card background, then its glow, then the content -- in that order, because MultiEffect renders
  // over anything declared before it (CLAUDE.md z-order rule).
  HudFrame {
    id: card

    anchors.centerIn: parent
    width: root.cardWidth
    height: root.cardHeight
    variant: HudFrame.Popup
  }

  MultiEffect {
    source: card
    anchors.fill: card
    shadowEnabled: true
    shadowColor: HoloniightPalette.accentCyan
    shadowBlur: 0.5
    shadowOpacity: 0.14
    shadowScale: 1.01
    shadowHorizontalOffset: 0
    shadowVerticalOffset: 0
    autoPaddingEnabled: true
  }

  Loader {
    id: contentLoader

    anchors.centerIn: parent
    sourceComponent: root.kind === "selection" ? selectionComponent : levelComponent
  }

  // Each renderer's properties are bound here, at the declaration site, so its required properties
  // are always initialized -- the documented per-branch `setInitialProperties` trap does not apply.
  Component {
    id: levelComponent

    OsdLevelRenderer {
      channel: root.channel
      value: root.value
      muted: root.muted
    }
  }

  Component {
    id: selectionComponent

    OsdSelectionRenderer {
      shortLabel: root.shortLabel
      fullLabel: root.fullLabel
    }
  }
}

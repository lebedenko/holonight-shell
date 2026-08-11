pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import HolonightShell
import Holonight.Core
import Holonight.Components

import "../Controls"

// A single toast notification card in HoloNight HUD framing: accent-colored HudFrame border,
// minimal ambient glow, app icon + summary/body, and optional outlined action buttons. The
// accent follows the model's accentKind ("critical" → red, "violet" → social, else cyan).
// Entry uses a fade + slide-down (~12 px) per the design's "premium, not web-app" animation.
Item {
  id: root

  required property var model

  readonly property int notifId: root.model.notifId ?? 0
  readonly property string summaryText: root.model.summary ?? ""
  readonly property string bodyText: root.model.body ?? ""
  readonly property string appIconName: root.model.appIcon ?? ""
  readonly property var actionList: root.model.actions ?? []
  readonly property bool hasDefaultAction: root.model.hasDefaultAction ?? false
  readonly property string accentKind: root.model.accentKind ?? "cyan"
  readonly property real createdAtMs: root.model.createdAtMs ?? Date.now()

  readonly property color accentColor: {
    if (root.accentKind === "critical")
      return HoloniightPalette.error;
    if (root.accentKind === "violet")
      return HoloniightPalette.accentViolet;
    return HoloniightPalette.accentCyan;
  }

  readonly property int horizontalPadding: 16
  readonly property int verticalPadding: 14
  readonly property int iconTileSize: 44

  // Sanitizes notification body markup for Text.StyledText. The freedesktop "body-markup"
  // capability covers <b>/<i>/<u>/<a>; <img> is dropped because we advertise only "icon-static" —
  // inline images are unsupported and would otherwise attempt network/file loads. Both self-closing
  // (<img .../>) and bare (<img ...>) forms are removed.
  //
  // Anchors are rewritten to cyan underlined text rather than left as <a href> (REQ-F-028). Text's
  // linkColor does not override StyledText's hardcoded default blue on this Qt build, and dropping
  // the href makes links genuinely non-clickable. The color comes from the palette (no hex literal).
  function sanitizeBody(text) {
    if (!text)
      return "";
    const stripped = text.replace(/<img\b[^>]*\/?>/gi, "");
    const linkStyle = "" + HoloniightPalette.accentCyan;
    return stripped.replace(/<a\b[^>]*>([\s\S]*?)<\/a>/gi, '<font color="' + linkStyle + '"><u>$1</u></font>');
  }

  // Body-click routing (REQ-F-024): if the notification declares a "default" action, clicking the
  // body invokes it (emitting ActionInvoked(id, "default")); otherwise the click dismisses the toast.
  function activateBody() {
    if (root.hasDefaultAction) {
      NotificationService.invokeAction(root.notifId, "default");
      return;
    }
    NotificationService.dismiss(root.notifId);
  }

  function relativeTime() {
    const ageMs = Math.max(0, Date.now() - root.createdAtMs);
    const minute = 60000;
    const hour = 60 * minute;
    if (ageMs < minute)
      return "now";
    if (ageMs < hour)
      return Math.floor(ageMs / minute) + "m ago";
    return Math.floor(ageMs / hour) + "h ago";
  }

  implicitWidth: 430
  implicitHeight: Math.max(88, Math.max(root.iconTileSize, textColumn.implicitHeight, utilityRow.implicitHeight)
                           + (root.actionList.length > 0 ? 12 + actionBar.implicitHeight : 0)
                           + root.verticalPadding * 2)

  // Fade + slide-down entry. enterProgress drives both opacity and a non-layout Translate so
  // the slide does not fight the Column positioner that owns this item's y.
  property real enterProgress: 0
  opacity: root.enterProgress
  transform: Translate {
    y: (1 - root.enterProgress) * -12
  }

  Component.onCompleted: root.enterProgress = 1

  Behavior on enterProgress {
    NumberAnimation { duration: 170; easing.type: Easing.OutCubic }
  }

  // HUD frame background, drawn first so the glow and content render above it.
  HudFrame {
    id: frame
    anchors.fill: parent
    variant: HudFrame.Popup
    frameInset: 1
    frameStroke: root.accentColor
    strokeWidth: 1
    innerGlowColor: root.accentColor
    innerGlowOpacity: root.accentKind === "critical" ? 0.07 : 0.04
    strokeOpacity: 1.0
    topGradientOpacity: 0.055
    bottomGradientOpacity: 0.065
  }

  // Soft ambient glow keyed to the accent. Declared before content so content stays on top.
  MultiEffect {
    source: frame
    anchors.fill: frame
    shadowEnabled: true
    shadowColor: root.accentColor
    shadowBlur: 0.5
    shadowOpacity: root.accentKind === "critical" ? 0.22 : 0.12
    shadowScale: 1.008
    autoPaddingEnabled: true
  }

  // Click on the body (outside the action buttons, which sit above this) invokes the default
  // action if present, otherwise dismisses the toast (REQ-F-024).
  MouseArea {
    anchors.fill: parent
    z: 1
    acceptedButtons: Qt.LeftButton
    onClicked: root.activateBody()
  }

  // Hover pauses the auto-dismiss timer; leaving resumes it with the remaining time.
  HoverHandler {
    onHoveredChanged: {
      if (hovered)
        NotificationService.hoverEntered(root.notifId);
      else
        NotificationService.hoverLeft(root.notifId);
    }
  }

  Timer {
    interval: 60000
    running: root.visible
    repeat: true
    onTriggered: timeText.text = root.relativeTime()
  }

  Item {
    id: contentArea
    z: 2
    anchors {
      left: parent.left
      right: parent.right
      top: parent.top
      bottom: parent.bottom
      leftMargin: root.horizontalPadding
      rightMargin: root.horizontalPadding
      topMargin: root.verticalPadding
      bottomMargin: root.verticalPadding
    }

    Item {
      id: headerArea
      anchors {
        left: parent.left
        right: parent.right
        top: parent.top
      }
      height: Math.max(root.iconTileSize, textColumn.implicitHeight, utilityRow.implicitHeight)
    }

    Item {
      id: iconTile
      width: root.iconTileSize
      height: root.iconTileSize
      anchors {
        left: headerArea.left
        verticalCenter: headerArea.verticalCenter
      }

      HudFrame {
        anchors.fill: parent
        variant: HudFrame.Widget
        frameInset: 1
        frameStroke: root.accentColor
        strokeWidth: 1
        strokeOpacity: 0.78
        innerGlowColor: root.accentColor
        innerGlowOpacity: 0.06
        topGradientOpacity: 0.05
        bottomGradientOpacity: 0.05
      }

      Rectangle {
        anchors {
          fill: parent
          margins: 2
        }
        radius: 5
        color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b,
                       root.accentKind === "critical" ? 0.1 : 0.14)
      }

      ExternalIcon {
        anchors.centerIn: parent
        iconName: root.appIconName
        iconSize: 32
        tintColor: root.accentColor
        fallbackIconName: "dialog-information"
      }
    }

    Column {
      id: textColumn
      anchors {
        left: iconTile.right
        right: utilityRow.left
        top: headerArea.top
        leftMargin: 13
        rightMargin: 12
      }
      spacing: 5

      Text {
        width: parent.width
        text: root.summaryText
        color: HoloniightPalette.textPrimary
        elide: Text.ElideRight
        maximumLineCount: 1
        font.weight: Font.DemiBold
        font.pointSize: 10.5
      }

      Text {
        width: parent.width
        visible: root.bodyText.length > 0
        text: root.sanitizeBody(root.bodyText)
        textFormat: Text.StyledText
        color: HoloniightPalette.textMuted
        wrapMode: Text.WordWrap
        maximumLineCount: root.actionList.length > 0 ? 2 : 3
        elide: Text.ElideRight
        font.pointSize: 9
        lineHeight: 0.98
      }
    }

    Row {
      id: utilityRow
      anchors {
        right: headerArea.right
        top: headerArea.top
      }
      spacing: 10

      Text {
        id: timeText
        width: 44
        anchors.verticalCenter: parent.verticalCenter
        text: root.relativeTime()
        color: HoloniightPalette.textSecondary
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignRight
        font.pointSize: 8.25
      }

      Item {
        width: 24
        height: 24
        z: 4

        Rectangle {
          anchors.fill: parent
          radius: 5
          color: closeHover.hovered
            ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.14)
            : Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                      HoloniightPalette.surface.b, 0.24)
          border.color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b,
                                closeHover.hovered ? 0.7 : 0.22)
          border.width: 1

          Rectangle {
            width: 10
            height: 1
            radius: 1
            color: closeHover.hovered ? root.accentColor : HoloniightPalette.textSecondary
            anchors.centerIn: parent
            rotation: 45
          }

          Rectangle {
            width: 10
            height: 1
            radius: 1
            color: closeHover.hovered ? root.accentColor : HoloniightPalette.textSecondary
            anchors.centerIn: parent
            rotation: -45
          }
        }

        HoverHandler {
          id: closeHover
        }

        TapHandler {
          onTapped: NotificationService.dismiss(root.notifId)
        }
      }
    }

    ToastActionBar {
      id: actionBar
      anchors {
        left: parent.left
        right: utilityRow.left
        top: headerArea.bottom
        topMargin: 12
        rightMargin: 12
      }
      visible: root.actionList.length > 0
      height: visible ? implicitHeight : 0
      z: 4
      notifId: root.notifId
      accentColor: root.accentColor
      actions: root.actionList
    }
  }
}

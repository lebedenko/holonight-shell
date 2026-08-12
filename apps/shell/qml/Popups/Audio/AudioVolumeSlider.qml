import QtQuick
import Holonight.Core

// Reusable themed volume slider (0–100). Used by device/stream delegates and the master bar.
//
// Drag stability (REQ-F-031): while the user is dragging, incoming `value` updates from the
// model are ignored. The thumb tracks an internal `displayValue` that only resyncs from
// `value` when not dragging — so an external volume change on another row never yanks this
// thumb mid-drag. Emits `valueChanging` continuously during drag and `valueCommitted` on
// release.
Item {
  id: root

  property int value: 0                              // IN: bound to model role
  property color accentColor: HoloniightPalette.accentCyan
  property bool dragging: false                      // OUT: true while the user drags
  property int displayValue: value                   // internal thumb position

  signal valueChanging(int value)
  signal valueCommitted(int value)

  implicitHeight: 18
  implicitWidth: 160

  readonly property int thumbRadius: 7
  readonly property real trackLeft: thumbRadius
  readonly property real trackSpan: Math.max(1, width - (2 * thumbRadius))

  onValueChanged: {
    if (!root.dragging)
      root.displayValue = root.value;
  }

  function posToValue(posX: real): int {
    const ratio = (posX - root.trackLeft) / root.trackSpan;
    return Math.round(Math.max(0, Math.min(1, ratio)) * 100);
  }

  // Track
  Rectangle {
    objectName: "audioVolumeSliderTrack"
    anchors.verticalCenter: parent.verticalCenter
    x: root.trackLeft
    width: root.trackSpan
    height: 4
    radius: 2
    color: HoloniightPalette.surface
  }

  // Fill
  Rectangle {
    objectName: "audioVolumeSliderFill"
    anchors.verticalCenter: parent.verticalCenter
    x: root.trackLeft
    width: root.trackSpan * (root.displayValue / 100.0)
    height: 4
    radius: 2
    color: root.accentColor
  }

  // Thumb
  Rectangle {
    objectName: "audioVolumeSliderThumb"
    width: root.thumbRadius * 2
    height: root.thumbRadius * 2
    radius: root.thumbRadius
    anchors.verticalCenter: parent.verticalCenter
    x: root.trackLeft + (root.trackSpan * (root.displayValue / 100.0)) - root.thumbRadius
    color: root.accentColor
    border.color: HoloniightPalette.surface
    border.width: 2
  }

  MouseArea {
    anchors.fill: parent
    preventStealing: true
    cursorShape: Qt.PointingHandCursor

    onPressed: (mouse) => {
      root.dragging = true;
      root.displayValue = root.posToValue(mouse.x);
      root.valueChanging(root.displayValue);
    }
    onPositionChanged: (mouse) => {
      if (root.dragging) {
        root.displayValue = root.posToValue(mouse.x);
        root.valueChanging(root.displayValue);
      }
    }
    onReleased: (mouse) => {
      root.displayValue = root.posToValue(mouse.x);
      root.dragging = false;
      root.valueCommitted(root.displayValue);
    }
  }
}

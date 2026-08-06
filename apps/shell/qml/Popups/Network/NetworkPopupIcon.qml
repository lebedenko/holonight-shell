import QtQuick
import QtQuick.Shapes
import Holonight.Core

Item {
  id: root

  property string name: ""
  property color iconColor: HoloniightPalette.textSecondary
  property real strokeWidth: 1.6

  readonly property bool recognized: root.name === "refresh"
                                     || root.name === "settings"
                                     || root.name === "info"
                                     || root.name === "chevron"
                                     || root.name === "lock"
  readonly property color resolvedColor: root.iconColor

  implicitWidth: 24
  implicitHeight: 24
  Accessible.ignored: true

  Shape {
    objectName: "refreshGlyph"
    anchors.centerIn: parent
    width: 24
    height: 24
    scale: Math.min(root.width, root.height) / 24
    visible: root.name === "refresh"
    preferredRendererType: Shape.CurveRenderer

    ShapePath {
      fillColor: "transparent"
      strokeColor: root.iconColor
      strokeWidth: root.strokeWidth
      capStyle: ShapePath.RoundCap
      joinStyle: ShapePath.RoundJoin

      PathSvg {
        path: "M20 11a8 8 0 0 0-14.93-3.92L3 10 M3 5v5h5 M4 13a8 8 0 0 0 14.93 3.92L21 14 M21 19v-5h-5"
      }
    }
  }

  Shape {
    objectName: "settingsGlyph"
    anchors.centerIn: parent
    width: 24
    height: 24
    scale: Math.min(root.width, root.height) / 24
    visible: root.name === "settings"
    preferredRendererType: Shape.CurveRenderer

    ShapePath {
      fillColor: "transparent"
      strokeColor: root.iconColor
      strokeWidth: root.strokeWidth
      capStyle: ShapePath.RoundCap
      joinStyle: ShapePath.RoundJoin

      PathSvg {
        path: "M12.22 2h-.44a2 2 0 0 0-2 2v.18a2 2 0 0 1-1 1.73l-.43.25a2 2 0 0 1-2 0l-.15-.08a2 2 0 0 0-2.73.73l-.22.38a2 2 0 0 0 .73 2.73l.15.09a2 2 0 0 1 1 1.74v.5a2 2 0 0 1-1 1.74l-.15.09a2 2 0 0 0-.73 2.73l.22.38a2 2 0 0 0 2.73.73l.15-.08a2 2 0 0 1 2 0l.43.25a2 2 0 0 1 1 1.73V20a2 2 0 0 0 2 2h.44a2 2 0 0 0 2-2v-.18a2 2 0 0 1 1-1.73l.43-.25a2 2 0 0 1 2 0l.15.08a2 2 0 0 0 2.73-.73l.22-.38a2 2 0 0 0-.73-2.73l-.15-.09a2 2 0 0 1-1-1.74v-.5a2 2 0 0 1 1-1.74l.15-.09a2 2 0 0 0 .73-2.73l-.22-.38a2 2 0 0 0-2.73-.73l-.15.08a2 2 0 0 1-2 0l-.43-.25a2 2 0 0 1-1-1.73V4a2 2 0 0 0-2-2z M12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6z"
      }
    }
  }

  Shape {
    objectName: "infoGlyph"
    anchors.centerIn: parent
    width: 24
    height: 24
    scale: Math.min(root.width, root.height) / 24
    visible: root.name === "info"
    preferredRendererType: Shape.CurveRenderer

    ShapePath {
      fillColor: "transparent"
      strokeColor: root.iconColor
      strokeWidth: root.strokeWidth
      capStyle: ShapePath.RoundCap
      joinStyle: ShapePath.RoundJoin

      PathSvg {
        path: "M12 22a10 10 0 1 0 0-20 10 10 0 0 0 0 20z M12 10v6 M12 7h.01"
      }
    }
  }

  Shape {
    objectName: "lockGlyph"
    anchors.centerIn: parent
    width: 24
    height: 24
    scale: Math.min(root.width, root.height) / 24
    visible: root.name === "lock"
    preferredRendererType: Shape.CurveRenderer

    ShapePath {
      fillColor: "transparent"
      strokeColor: root.iconColor
      strokeWidth: root.strokeWidth
      capStyle: ShapePath.RoundCap
      joinStyle: ShapePath.RoundJoin

      PathSvg {
        path: "M7 10V8a5 5 0 0 1 10 0v2 M6 10h12a1 1 0 0 1 1 1v8a1 1 0 0 1-1 1H6a1 1 0 0 1-1-1v-8a1 1 0 0 1 1-1z"
      }
    }
  }

  Shape {
    objectName: "chevronGlyph"
    anchors.centerIn: parent
    width: 24
    height: 24
    scale: Math.min(root.width, root.height) / 24
    visible: root.name === "chevron"
    preferredRendererType: Shape.CurveRenderer

    ShapePath {
      fillColor: "transparent"
      strokeColor: root.iconColor
      strokeWidth: root.strokeWidth
      capStyle: ShapePath.RoundCap
      joinStyle: ShapePath.RoundJoin

      PathSvg { path: "M9 18l6-6-6-6" }
    }
  }
}

import QtQuick
import Holonight.Core
import HolonightShell

import "../Controls"

Item {
    id: root

    property int percent: 0
    property bool charging: false
    property bool discharging: false
    property bool fullyCharged: false
    property bool present: true

    readonly property bool charged: fullyCharged && !charging && !discharging
    readonly property bool highDischarging: discharging && percent >= 20
    readonly property bool low: discharging && percent < 20
    readonly property bool critical: discharging && percent < 10
    readonly property bool pulseGlow: root.charging || root.highDischarging || root.low
    readonly property color statusGlowColor: charging
        ? HoloniightPalette.accentViolet
        : low
            ? HoloniightPalette.error
            : HoloniightPalette.accentCyan

    visible: present
    width: 40
    height: 24

    QtObject {
        id: glowPulse
        property real value: root.charging ? 0.55 : (root.critical ? 0.34 : 0.28)
    }

    BarIcon {
        id: batteryIcon
        objectName: "batteryIcon"
        anchors.fill: parent
        name: root.low ? "battery_low" : "battery"
        batteryPercent: root.percent
        charging: root.charging
        glowEnabled: root.pulseGlow
        glowOpacity: glowPulse.value
        glowColor: root.statusGlowColor
        glowBlur: root.charging ? 0.92 : (root.critical ? 0.86 : 0.78)
        glowScale: root.charging ? 1.32 : (root.critical ? 1.28 : 1.22)
    }

    Behavior on percent {
        NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
    }

    SequentialAnimation {
        running: root.visible && root.pulseGlow
        loops: Animation.Infinite

        NumberAnimation {
            target: glowPulse
            property: "value"
            to: root.critical ? 0.94 : 0.72
            duration: root.critical ? 320 : (root.charging ? 620 : 700)
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: glowPulse
            property: "value"
            to: root.charging ? 0.55 : (root.critical ? 0.34 : 0.28)
            duration: root.critical ? 320 : (root.charging ? 620 : 700)
            easing.type: Easing.InOutSine
        }
    }

    SequentialAnimation {
        running: root.visible && root.critical
        loops: Animation.Infinite

        NumberAnimation {
            target: batteryIcon
            property: "glyphScale"
            to: 1.08
            duration: 260
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: batteryIcon
            property: "glyphScale"
            to: 1.0
            duration: 260
            easing.type: Easing.InOutSine
        }
    }

    states: State {
        name: "idle"
        when: !root.critical
        PropertyChanges {
            batteryIcon.glyphScale: 1.0
        }
    }
}

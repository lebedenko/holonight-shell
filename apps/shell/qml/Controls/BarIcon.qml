import QtQuick
import QtQuick.Effects
import Holonight.Core

Item {
    id: root

    property string name: ""
    property real glyphScale: 1.0
    property bool glowEnabled: false
    property real glowOpacity: 0.28
    property color glowColor: HoloniightPalette.accentCyan
    property real glowBlur: 0.55
    property real glowScale: 1.08
    property int batteryPercent: 100
    property bool charging: false
    property real bodyOpacity: 1.0
    property int signalStrength: 100

    implicitWidth: 24
    implicitHeight: 24

    readonly property string _iconSource: {
        if (root.name.startsWith("audio-volume-")) {
            return "AudioIcon.qml"
        }
        if (root.name.startsWith("wifi_") || root.name === "network-wired-symbolic") {
            return "NetworkIcon.qml"
        }
        if (root.name.startsWith("battery_") || root.name === "battery") {
            return "BatteryIcon.qml"
        }
        if (root.name === "clock" || root.name === "keyboard" || root.name === "window"
                || root.name === "workspace" || root.name === "system" || root.name === "notification"
                || root.name === "brightness") {
            return "UtilityIcon.qml"
        }
        return "SessionIcon.qml"
    }

    MultiEffect {
        source: iconLoader
        anchors.fill: iconLoader
        visible: root.glowEnabled && iconLoader.status === Loader.Ready
        shadowEnabled: true
        shadowColor: root.glowColor
        shadowBlur: root.glowBlur
        shadowOpacity: root.glowOpacity
        shadowScale: root.glowScale
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        autoPaddingEnabled: true
    }

    Loader {
        id: iconLoader

        anchors.centerIn: parent
        width: parent.width * root.glyphScale
        height: parent.height * root.glyphScale
        source: root._iconSource
    }

    Binding { target: iconLoader.item; property: "name"; value: root.name; when: iconLoader.item !== null }
    Binding { target: iconLoader.item; property: "bodyOpacity"; value: root.bodyOpacity; when: iconLoader.item !== null }
    Binding {
        target: iconLoader.item
        property: "batteryPercent"
        value: root.batteryPercent
        when: iconLoader.item !== null
    }
    Binding {
        target: iconLoader.item
        property: "charging"
        value: root.charging
        when: iconLoader.item !== null && root._iconSource === "BatteryIcon.qml"
    }
    Binding {
        target: iconLoader.item
        property: "signalStrength"
        value: root.signalStrength
        when: iconLoader.item !== null
    }
}

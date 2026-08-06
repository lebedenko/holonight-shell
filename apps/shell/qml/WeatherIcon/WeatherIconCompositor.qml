pragma ComponentBehavior: Bound

import QtQuick
import HolonightShell

Item {
    id: root

    required property int conditionCode
    required property bool isDay
    property string conditionDescription: ""
    property int windSpeedKmh: 0
    property date date: new Date()
    property var moonPhase: undefined
    property int iconSize: 128

    width: iconSize
    height: iconSize

    readonly property var layerNames: {
        var mp = root.moonPhase
        return WeatherIconBridge.layersForWeather(root.conditionCode, root.isDay, root.date,
                                                  root.conditionDescription, root.windSpeedKmh, mp)
    }

    Repeater {
        model: root.layerNames

        delegate: Image {
            required property string modelData
            required property int index
            anchors.fill: parent
            z: index
            source: "qrc:/HolonightShell/weather-png/512x512/" + modelData + ".png"
            sourceSize: Qt.size(root.iconSize, root.iconSize)
            smooth: true
            mipmap: true
            fillMode: Image.PreserveAspectFit
        }
    }
}

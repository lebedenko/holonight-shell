import QtQuick
import HolonightShell

Image {
    id: root
    property string provider: ""
    property int conditionCode: -1
    property bool isDay: false
    property bool daylightAvailable: false
    property int iconSize: 128
    width: iconSize
    height: iconSize
    source: WeatherIconBridge.iconFor(provider, conditionCode, isDay, daylightAvailable)
    sourceSize: Qt.size(iconSize * Screen.devicePixelRatio, iconSize * Screen.devicePixelRatio)
    fillMode: Image.PreserveAspectFit
    smooth: true
    mipmap: true
    Accessible.ignored: true
}

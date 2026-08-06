import QtQuick

Item {
    enum IconState {
        Normal = 0,
        Muted = 1,
        Disabled = 2,
        Active = 3
    }

    required property url source

    property int size: 24
    property int iconState: HnIcon.Normal
    property bool tinted: true
    property color normalColor: "white"
    property color mutedColor: "white"
    property color disabledColor: "white"
    property color activeColor: "white"

    readonly property color resolvedColor: normalColor
    readonly property url _renderSource: source
}

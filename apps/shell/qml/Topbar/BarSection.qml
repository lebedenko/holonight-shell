import QtQuick

Rectangle {
    id: root

    color: "transparent"
    implicitHeight: 64

    signal wheeled(real angleDelta)

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        onWheel: (wheel) => root.wheeled(wheel.angleDelta.y)
    }

    default property alias content: container.data

    Item {
        id: container
        anchors {
            fill: parent
            leftMargin: 8
            rightMargin: 8
        }
    }
}

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

Item {
    id: root

    required property string text
    required property color color
    required property string fontFamily
    required property real pointSize

    implicitWidth: glyphRow.implicitWidth
    implicitHeight: glyphRow.implicitHeight

    TextMetrics {
        id: zeroMetrics
        font.family: root.fontFamily
        font.pointSize: root.pointSize
        text: "0"
    }

    TextMetrics {
        id: oneMetrics
        font.family: root.fontFamily
        font.pointSize: root.pointSize
        text: "1"
    }

    TextMetrics {
        id: twoMetrics
        font.family: root.fontFamily
        font.pointSize: root.pointSize
        text: "2"
    }

    TextMetrics {
        id: threeMetrics
        font.family: root.fontFamily
        font.pointSize: root.pointSize
        text: "3"
    }

    TextMetrics {
        id: fourMetrics
        font.family: root.fontFamily
        font.pointSize: root.pointSize
        text: "4"
    }

    TextMetrics {
        id: fiveMetrics
        font.family: root.fontFamily
        font.pointSize: root.pointSize
        text: "5"
    }

    TextMetrics {
        id: sixMetrics
        font.family: root.fontFamily
        font.pointSize: root.pointSize
        text: "6"
    }

    TextMetrics {
        id: sevenMetrics
        font.family: root.fontFamily
        font.pointSize: root.pointSize
        text: "7"
    }

    TextMetrics {
        id: eightMetrics
        font.family: root.fontFamily
        font.pointSize: root.pointSize
        text: "8"
    }

    TextMetrics {
        id: nineMetrics
        font.family: root.fontFamily
        font.pointSize: root.pointSize
        text: "9"
    }

    readonly property real digitWidth: Math.max(
        zeroMetrics.width,
        oneMetrics.width,
        twoMetrics.width,
        threeMetrics.width,
        fourMetrics.width,
        fiveMetrics.width,
        sixMetrics.width,
        sevenMetrics.width,
        eightMetrics.width,
        nineMetrics.width
    )

    RowLayout {
        id: glyphRow
        anchors.centerIn: parent
        spacing: 0

        Repeater {
            model: root.text.length

            Text {
                required property int index

                readonly property string glyph: root.text.charAt(index)
                readonly property bool digit: glyph >= "0" && glyph <= "9"

                Layout.preferredWidth: digit ? root.digitWidth : implicitWidth
                text: glyph
                textFormat: Text.PlainText
                color: root.color
                horizontalAlignment: Text.AlignHCenter
                font.family: root.fontFamily
                font.pointSize: root.pointSize
            }
        }
    }
}

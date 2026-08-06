import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import HolonightShell
import Holonight.Core

// Clock widget body: a large 24-hour HH:mm with smaller inline seconds and a glow, above a localized
// date row. All strings are computed in C++ (WidgetManager); this component only styles them. Colors
// come from the HoloNight palette and fonts from AppearanceService — no hardcoded values (project convention).
Item {
    id: root

    required property string timeText
    required property string secondsText   // empty string when show_seconds = false → seconds hidden
    required property string dateText

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 8

        // Container so the glow can fill the whole time row (a MultiEffect cannot anchor to a layout
        // grandchild). The glow is declared before the content so it renders behind it.
        Item {
            id: timeContainer
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: timeContent.implicitWidth
            implicitHeight: timeContent.implicitHeight

            MultiEffect {
                source: timeContent
                anchors.fill: timeContent
                shadowEnabled: true
                shadowColor: HoloniightPalette.primary
                shadowBlur: 0.7
                shadowOpacity: 0.6
                shadowScale: 1.05
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 0
                autoPaddingEnabled: true
            }

            Item {
                id: timeContent
                anchors.centerIn: parent
                readonly property int bigPixelSize: Math.round(AppearanceService.clockFontSize * 4.0)
                readonly property int smallPixelSize: Math.round(AppearanceService.clockFontSize * 2.0)
                readonly property int secondsSpacing: 2
                implicitWidth: bigTime.implicitWidth
                                + (smallSeconds.visible ? timeContent.secondsSpacing + smallSeconds.implicitWidth : 0)
                implicitHeight: bigTime.implicitHeight

                // Per-size font metrics, so the smaller seconds can be lifted to share the big digits'
                // baseline rather than their box bottoms (the larger font has the deeper descent).
                FontMetrics {
                    id: bigMetrics
                    font.family: AppearanceService.clockFont
                    font.pixelSize: timeContent.bigPixelSize
                }
                FontMetrics {
                    id: smallMetrics
                    font.family: AppearanceService.clockFont
                    font.pixelSize: timeContent.smallPixelSize
                }

                StableDigitsText {
                    id: bigTime
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.timeText
                    color: HoloniightPalette.primary
                    fontFamily: AppearanceService.clockFont
                    pixelSize: timeContent.bigPixelSize
                }

                // Seconds (with a ":" prefix) at half the time's pixel size, lifted by the descent
                // difference so its baseline lines up with the big digits — one continuous "HH:mm:ss".
                StableDigitsText {
                    id: smallSeconds
                    visible: root.secondsText.length > 0
                    anchors.left: bigTime.right
                    anchors.leftMargin: timeContent.secondsSpacing
                    anchors.bottom: bigTime.bottom
                    anchors.bottomMargin: Math.round(bigMetrics.descent - smallMetrics.descent)
                    text: ":" + root.secondsText
                    color: HoloniightPalette.primary
                    fontFamily: AppearanceService.clockFont
                    pixelSize: timeContent.smallPixelSize
                }
            }
        }

        Text {
            id: dateLabel
            Layout.alignment: Qt.AlignRight
            text: root.dateText
            color: HoloniightPalette.textSecondary
            font.family: AppearanceService.uiFont
            font.pixelSize: AppearanceService.uiFontSize
        }
    }
}

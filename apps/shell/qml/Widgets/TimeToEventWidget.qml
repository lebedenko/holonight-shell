import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import HolonightShell
import Holonight.Core

// Time-to-event widget body: a big title, a bigger glowing countdown, and a small event-date label.
// All text/strings are computed in C++; this component only styles them. Colors come from the
// HoloNight palette and fonts from AppearanceService — no hardcoded values (per project convention).
Item {
    id: root

    required property string titleText
    required property string remainingText
    required property string deadlineLabel

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 6

        Text {
            id: titleLabel
            Layout.alignment: Qt.AlignHCenter
            text: root.titleText
            color: HoloniightPalette.textMuted
            font.family: AppearanceService.uiFont
            font.pixelSize: Math.round(AppearanceService.uiFontSize * 2.0)
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 2
        }

        // Container so the glow can anchor to its sibling countdown label (a MultiEffect cannot anchor
        // to a layout grandchild). The glow is declared before the label so it renders behind it.
        Item {
            id: countdownContainer
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: countdownLabel.implicitWidth
            implicitHeight: countdownLabel.implicitHeight

            MultiEffect {
                source: countdownLabel
                anchors.fill: countdownLabel
                shadowEnabled: true
                shadowColor: HoloniightPalette.primary
                shadowBlur: 0.7
                shadowOpacity: 0.6
                shadowScale: 1.05
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 0
                autoPaddingEnabled: true
            }

            StableDigitsText {
                id: countdownLabel
                anchors.centerIn: parent
                text: root.remainingText
                color: HoloniightPalette.primary
                fontFamily: AppearanceService.displayFont
                pixelSize: Math.round(AppearanceService.displayFontSize * 2.0)
            }
        }

        Text {
            id: dateLabel
            Layout.alignment: Qt.AlignHCenter
            text: root.deadlineLabel
            color: HoloniightPalette.textSecondary
            font.family: AppearanceService.uiFont
            font.pixelSize: AppearanceService.uiFontSize
        }
    }
}

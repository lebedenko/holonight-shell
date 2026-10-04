import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls
import HolonightShell
import "../Status/PopupMetrics.js" as PopupMetrics

PopupContentLayout {
    id: root

    property bool outputExpanded: true
    property bool inputExpanded: false
    readonly property real separatorBleed: 0
    naturalWidth: AudioService.available && headerItem ? Math.max(headerItem.implicitWidth, outputSection.implicitWidth, inputSection.implicitWidth) : unavailable.implicitWidth
    viewportItem.objectName: "audioPopupViewport"

    Component.onCompleted: AudioService.startInputLevelMonitoring()
    Component.onDestruction: AudioService.stopInputLevelMonitoring()

    header: Component {
        ColumnLayout {
            objectName: "audioPopupPinnedHeader"
            spacing: PopupMetrics.sectionGap

            AudioPopupHeader {
                id: popupHeader
                Layout.fillWidth: true
                nextTabItem: masterPanel.volumeSlider
            }
            HnSeparator {
                objectName: "audioHeaderSeparator"
                Layout.fillWidth: true
                fadeMode: HnSeparator.Solid
            }
            AudioMasterPanel {
                id: masterPanel
                objectName: "audioMasterPanel"
                Layout.fillWidth: true
                visible: AudioService.available
                previousTabItem: popupHeader.settingsButtonItem
            }
            HnSeparator {
                objectName: "audioHeroSeparator"
                Layout.fillWidth: true
                visible: AudioService.available
                fadeMode: HnSeparator.Solid
            }
        }
    }

    HnLabel {
        id: unavailable
        Layout.fillWidth: true
        visible: !AudioService.available
        rawText: qsTr("Audio service unavailable")
        role: HnTypographyRole.Body
        color: HoloniightPalette.textSecondary
        wrapMode: Text.Wrap
    }
    AudioDeviceSection {
        id: outputSection
        objectName: "outputDeviceSection"
        Layout.fillWidth: true
        visible: AudioService.available
        isInput: false
        expanded: root.outputExpanded
        onExpandRequested: root.outputExpanded = !root.outputExpanded
    }
    HnSeparator {
        objectName: "audioOutputSeparator"
        Layout.fillWidth: true
        visible: AudioService.available
        fadeMode: HnSeparator.Solid
    }
    AudioApplicationsSection {
        Layout.fillWidth: true
        visible: AudioService.available
    }
    HnSeparator {
        objectName: "audioApplicationsSeparator"
        Layout.fillWidth: true
        visible: AudioService.available
        fadeMode: HnSeparator.Solid
    }
    AudioDeviceSection {
        id: inputSection
        objectName: "inputDeviceSection"
        Layout.fillWidth: true
        visible: AudioService.available
        isInput: true
        expanded: root.inputExpanded
        onExpandRequested: root.inputExpanded = !root.inputExpanded
    }
    footer: AudioService.available ? hintFooter : null
    property Component hintFooter: Component {
        KeyboardHintFooter {
            focusItem: root.Window.window ? root.Window.window.activeFocusItem : null
        }
    }
}

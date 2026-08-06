import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls
import HolonightShell

import "../../../Popups/Audio"

// Full-width brightness slider for SidebarQuickSettings.
// Hidden entirely when no backlight device is present (BrightnessService.hasBacklight == false).
// Throttles D-Bus writes to 100 ms during drag; writes immediately on release.
// Incoming external changes (Fn keys) update the slider only when not dragging.
HnSettingsRow {
    id: root

    objectName: "brightnessSettingsRow"
    readonly property var brightnessControl: root.controlItem
    visible: BrightnessService.hasBacklight
    titleText: qsTr("Brightness")
    stacked: true
    control: Component {
        AudioVolumeSlider {
            id: brightnessSlider
            objectName: "brightnessControl"
            width: parent ? parent.width : implicitWidth
            value: BrightnessService.brightnessPercent

            onValueChanging: (val) => {
                writeThrottle.pending = val;
                if (!writeThrottle.running) {
                    writeThrottle.start();
                }
            }

            onValueCommitted: (val) => {
                writeThrottle.stop();
                BrightnessService.setBrightnessPercent(val);
            }
        }
    }

    property Timer writeThrottle: Timer {
        id: writeThrottle
        objectName: "writeThrottle"
        interval: 100
        repeat: true
        property int pending: 0
        onTriggered: BrightnessService.setBrightnessPercent(writeThrottle.pending)
    }

    property Connections brightnessConnections: Connections {
        target: BrightnessService
        function onBrightnessPercentChanged(percent) {
            if (root.brightnessControl && !root.brightnessControl.dragging) {
                root.brightnessControl.value = percent;
            }
        }
    }
}

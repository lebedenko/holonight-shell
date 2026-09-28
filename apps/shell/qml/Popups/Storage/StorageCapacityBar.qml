pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls

// REQ-F-009/REQ-F-010: capacity bar for a mounted volume. Thresholds are hardcoded QML
// constants per REQ-C-003 (no holonight-config wiring this cycle).
ColumnLayout {
    id: root

    required property real usedBytes
    required property real totalBytes
    required property real freeBytes

    readonly property real freeRatio: root.totalBytes > 0 ? root.freeBytes / root.totalBytes : 1.0
    readonly property color fillColor: root.freeRatio < 0.05 ? HoloniightPalette.error
                                        : (root.freeRatio < 0.10 ? HoloniightPalette.warning
                                                                  : HoloniightPalette.primary)
    readonly property real fillRatio: root.totalBytes > 0
                                       ? Math.max(0, Math.min(1, root.usedBytes / root.totalBytes)) : 0

    function formatBytes(bytes) {
        const units = ["B", "KB", "MB", "GB", "TB"]
        var value = bytes
        var unitIndex = 0
        while (value >= 1000 && unitIndex < units.length - 1) {
            value /= 1000
            unitIndex += 1
        }
        const text = value < 10 ? value.toFixed(1) : Math.round(value).toString()
        return (text.endsWith(".0") ? text.slice(0, -2) : text) + " " + units[unitIndex]
    }

    spacing: 4

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 6
        radius: 3
        color: HoloniightPalette.surfaceElevated

        Rectangle {
            width: parent.width * root.fillRatio
            height: parent.height
            radius: parent.radius
            color: root.fillColor

            Behavior on width {
                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
            }
        }
    }

    HnLabel {
        Layout.fillWidth: true
        role: HnTypographyRole.Caption
        color: HoloniightPalette.textMuted
        rawText: qsTr("%1 used of %2").arg(root.formatBytes(root.usedBytes)).arg(root.formatBytes(root.totalBytes))
    }
}

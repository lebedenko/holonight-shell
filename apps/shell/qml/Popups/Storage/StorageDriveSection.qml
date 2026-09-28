pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Controls
import Holonight.Components
import HolonightShell

// REQ-F-007: ListView.section.delegate — a collapsible drive header. Only receives the
// implicit `section` string under ComponentBehavior: Bound (no row data) — every other field
// is fetched via StorageService drive-level invokables keyed by driveId (DESIGN.md §4.3).
ColumnLayout {
    id: root

    required property string section
    property bool collapsed: false

    signal toggleCollapsed

    // Read the service's NOTIFY property directly: an intermediate count-valued property
    // does not notify its dependents when drive state changes but the count stays the same.
    readonly property string driveOperationText: {
        StorageService.count;
        return StorageService.driveOperationText(root.section);
    }
    readonly property string driveErrorText: {
        StorageService.count;
        return StorageService.driveErrorText(root.section);
    }
    readonly property bool driveCanEject: {
        StorageService.count;
        return StorageService.driveCanEject(root.section);
    }
    readonly property bool driveCanPowerOff: {
        StorageService.count;
        return StorageService.driveCanPowerOff(root.section);
    }

    Layout.fillWidth: true
    Layout.topMargin: 12
    spacing: 6

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        ExternalIcon {
            iconName: StorageService.driveIconName(root.section)
            fallbackIconName: "drive-removable-media-symbolic"
            iconSize: 22
            tintColor: HoloniightPalette.textSecondary
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            HnLabel {
                Layout.fillWidth: true
                rawText: StorageService.driveLabel(root.section)
                role: HnTypographyRole.Body
                font.bold: true
                elide: Text.ElideRight
            }

            HnLabel {
                Layout.fillWidth: true
                rawText: [StorageService.driveSubtitle(root.section), StorageService.driveCapacityText(root.section)]
                          .filter(part => part.length > 0).join(" • ")
                role: HnTypographyRole.Caption
                color: HoloniightPalette.textMuted
                elide: Text.ElideRight
            }
        }

        HnIconButton {
            objectName: "driveCollapseToggle"
            icon.source: "pan-down-symbolic"
            icon.color: HoloniightPalette.textSecondary
            rotation: root.collapsed ? 0 : 180
            Accessible.name: root.collapsed ? qsTr("Expand") : qsTr("Collapse")
            onClicked: root.toggleCollapsed()
        }
        HnIconButton {
            icon.source: "system-shutdown-symbolic"
            icon.color: HoloniightPalette.textSecondary
            visible: root.driveCanPowerOff
            Accessible.name: qsTr("Power off")
            onClicked: StorageService.powerOff(root.section)
        }
        HnIconButton {
            icon.source: "media-eject-symbolic"
            icon.color: HoloniightPalette.textSecondary
            visible: root.driveCanEject
            Accessible.name: qsTr("Eject")
            onClicked: StorageService.eject(root.section)
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 30
        spacing: 8
        visible: root.driveErrorText.length > 0 || root.driveOperationText.length > 0

        HnLabel {
            Layout.fillWidth: true
            visible: root.driveErrorText.length > 0
            rawText: root.driveErrorText
            wrapMode: Text.Wrap
            role: HnTypographyRole.Caption
            color: HoloniightPalette.error
        }
        Controls.Button {
            text: qsTr("Try Again")
            visible: root.driveErrorText.length > 0
            onClicked: StorageService.retry(root.section)
        }
        HnLabel {
            Layout.fillWidth: true
            visible: root.driveErrorText.length === 0 && root.driveOperationText.length > 0
            rawText: root.driveOperationText
            role: HnTypographyRole.Caption
            color: HoloniightPalette.textMuted
        }
    }
}

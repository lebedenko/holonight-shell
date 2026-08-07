import QtQuick
import QtQuick.Layouts
import HolonightShell
import Holonight.Core

// Root content of the battery popup, loaded by StatusPopup.qml's popupId-keyed Loader. Owns no
// surfaces; the popup outline/glow remain StatusPopup.qml's responsibility. Compact fixed-size
// layout (the surface is sized 300x360 in StatusPopupSurface): charge hero + fill bar, state and
// time-remaining line, conditional Health/Cycles rows, and the power-profile selector at the
// bottom. REQ-F-003..009, REQ-F-014, REQ-F-021..026.
Item {
    id: root

    readonly property bool low: BatteryService.percent < 20 && BatteryService.discharging

    readonly property string stateLabel: BatteryService.charging
        ? qsTr("Charging")
        : BatteryService.fullyCharged
            ? qsTr("Fully charged")
            : BatteryService.discharging
                ? qsTr("Discharging")
                : qsTr("Idle")

    readonly property string timeText: root.formatTimeRemaining(BatteryService.timeRemaining, BatteryService.charging)

    readonly property color dividerColor: Qt.rgba(HoloniightPalette.borderPassive.r,
                                                  HoloniightPalette.borderPassive.g,
                                                  HoloniightPalette.borderPassive.b, 0.55)

    // Caption shown beneath the profile buttons only while hovering.
    readonly property string hoveredCaption: saverButton.hovered
        ? saverButton.caption
        : balancedButton.hovered
            ? balancedButton.caption
            : performanceButton.hovered
                ? performanceButton.caption
                : ""

    function formatTimeRemaining(seconds, charging) {
        if (seconds < 60) {
            return ""
        }
        const hours = Math.floor(seconds / 3600)
        const minutes = Math.floor((seconds % 3600) / 60)
        let duration = ""
        if (hours > 0) {
            duration += hours + "h"
        }
        if (minutes > 0) {
            duration += (duration.length > 0 ? " " : "") + minutes + "m"
        }
        return charging ? (duration + " " + qsTr("to full")) : (duration + " " + qsTr("remaining"))
    }

    component MetricRow: RowLayout {
        id: metricRow
        property string label: ""
        property string value: ""
        Layout.fillWidth: true
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: metricRow.label
            color: HoloniightPalette.textSecondary
            font.family: AppearanceService.uiFont
            font.pixelSize: 13
            elide: Text.ElideRight
        }

        Text {
            text: metricRow.value
            color: HoloniightPalette.textPrimary
            font.family: AppearanceService.monospaceFont
            font.pixelSize: 13
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 4
        anchors.bottomMargin: 4
        spacing: 10

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("BATTERY")
            color: HoloniightPalette.accentBlue
            font.family: AppearanceService.uiFont
            font.pixelSize: 11
            font.weight: Font.DemiBold
            font.letterSpacing: 2
        }

        // Charge hero: large percentage with a smaller percent sign.
        RowLayout {
            // Nested layouts default Layout.fillWidth to true (which would left-pack the row);
            // disable it so Layout.alignment can center the intrinsic-width row.
            Layout.fillWidth: false
            Layout.alignment: Qt.AlignHCenter
            spacing: 2

            Text {
                text: BatteryService.percent
                color: HoloniightPalette.textPrimary
                font.family: AppearanceService.monospaceFont
                font.pixelSize: 46
                font.weight: Font.Medium
            }

            Text {
                Layout.alignment: Qt.AlignBottom
                Layout.bottomMargin: 8
                text: "%"
                color: HoloniightPalette.textMuted
                font.family: AppearanceService.monospaceFont
                font.pixelSize: 22
            }
        }

        // Subtle charge fill bar — a visual echo of the bar's battery glyph (REQ-F design idea).
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 8
            radius: 4
            color: Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                           HoloniightPalette.surface.b, 0.5)

            Rectangle {
                width: parent.width * Math.max(0, Math.min(100, BatteryService.percent)) / 100
                height: parent.height
                radius: parent.radius
                color: root.low ? HoloniightPalette.error : HoloniightPalette.accentCyan

                Behavior on width {
                    NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                }
                Behavior on color {
                    ColorAnimation { duration: 200; easing.type: Easing.OutCubic }
                }
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: root.timeText.length > 0 ? (root.stateLabel + " · " + root.timeText) : root.stateLabel
            color: HoloniightPalette.textMuted
            font.family: AppearanceService.uiFont
            font.pixelSize: 13
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 2
            Layout.preferredHeight: 1
            color: root.dividerColor
        }

        MetricRow {
            visible: BatteryService.health > 0
            label: qsTr("Health")
            value: BatteryService.health + "%"
        }

        MetricRow {
            visible: BatteryService.chargeCycles > 0
            label: qsTr("Cycles")
            value: BatteryService.chargeCycles
        }

        // Pushes the profile selector to the bottom of the fixed-size panel.
        Item { Layout.fillHeight: true }

        // Power-profile selector — only shown when power-profiles-daemon is available. Caption and
        // button row are direct children of the outer ColumnLayout so Qt.AlignHCenter centers them
        // against the full panel width (a nested ColumnLayout collapses to its content width and
        // left-aligns).
        Text {
            Layout.alignment: Qt.AlignHCenter
            visible: PowerProfilesService.available && root.hoveredCaption.length > 0
            text: root.hoveredCaption
            color: HoloniightPalette.accentCyan
            font.family: AppearanceService.uiFont
            font.pixelSize: 13
            font.weight: Font.Medium
        }

        RowLayout {
            // Disable nested-layout fillWidth so Qt.AlignHCenter centers the intrinsic-width row.
            Layout.fillWidth: false
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 2
            visible: PowerProfilesService.available
            spacing: 22

            ProfileButton {
                id: saverButton
                profileName: "power-saver"
                iconName: "power-profile-power-saver-symbolic"
                caption: qsTr("Power Saver")
                isActive: PowerProfilesService.activeProfile === "power-saver"
                isEnabled: PowerProfilesService.hasPowerSaver
                onActivated: profile => PowerProfilesService.setProfile(profile)
            }

            ProfileButton {
                id: balancedButton
                profileName: "balanced"
                iconName: "power-profile-balanced-symbolic"
                caption: qsTr("Balanced")
                isActive: PowerProfilesService.activeProfile === "balanced"
                isEnabled: PowerProfilesService.hasBalanced
                onActivated: profile => PowerProfilesService.setProfile(profile)
            }

            ProfileButton {
                id: performanceButton
                profileName: "performance"
                iconName: "power-profile-performance-symbolic"
                caption: qsTr("Performance")
                isActive: PowerProfilesService.activeProfile === "performance"
                isEnabled: PowerProfilesService.hasPerformance
                onActivated: profile => PowerProfilesService.setProfile(profile)
            }
        }
    }
}

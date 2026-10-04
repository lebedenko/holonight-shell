import QtQuick
import QtQuick.Layouts
import HolonightShell
import Holonight.Core

// Charge hero, optional metrics, and power profiles share the popup measurement contract.
PopupContentLayout {
    id: root
    property bool profilesAvailable: PowerProfilesService.available

    readonly property bool low: BatteryService.percent < 20 && BatteryService.discharging
    readonly property string stateLabel: BatteryService.charging ? qsTr("Charging") : BatteryService.fullyCharged ? qsTr("Fully charged") : BatteryService.discharging ? qsTr("Discharging") : qsTr("Idle")
    readonly property string timeText: root.formatTimeRemaining(BatteryService.timeRemaining, BatteryService.charging)
    readonly property color dividerColor: Qt.rgba(HoloniightPalette.borderPassive.r, HoloniightPalette.borderPassive.g, HoloniightPalette.borderPassive.b, 0.55)
    // Keep the caption visible for keyboard focus as well as hover.
    readonly property string hoveredCaption: saverButton.hovered || saverButton.activeFocus ? saverButton.caption : balancedButton.hovered || balancedButton.activeFocus ? balancedButton.caption : performanceButton.hovered || performanceButton.activeFocus ? performanceButton.caption : ""

    function formatTimeRemaining(seconds, charging) {
        if (seconds < 60)
            return "";

        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        let duration = "";
        if (hours > 0)
            duration += hours + "h";

        if (minutes > 0)
            duration += (duration.length > 0 ? " " : "") + minutes + "m";

        return charging ? (duration + " " + qsTr("to full")) : (duration + " " + qsTr("remaining"));
    }

    naturalWidth: profiles.implicitWidth

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
            font.pointSize: AppearanceService.displayFontSize * 1.4375
            font.weight: Font.Medium
        }

        Text {
            Layout.alignment: Qt.AlignBottom
            Layout.bottomMargin: 8
            text: "%"
            color: HoloniightPalette.textMuted
            font.family: AppearanceService.monospaceFont
            font.pointSize: AppearanceService.displayFontSize * 0.6875
        }

    }

    // Subtle charge fill bar — a visual echo of the bar's battery glyph (REQ-F design idea).
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 8
        radius: 4
        color: Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g, HoloniightPalette.surface.b, 0.5)

        Rectangle {
            width: parent.width * Math.max(0, Math.min(100, BatteryService.percent)) / 100
            height: parent.height
            radius: parent.radius
            color: root.low ? HoloniightPalette.error : HoloniightPalette.accentCyan

            Behavior on width {
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.OutCubic
                }

            }

            Behavior on color {
                ColorAnimation {
                    duration: 200
                    easing.type: Easing.OutCubic
                }

            }

        }

    }

    HnLabel {
        Layout.alignment: Qt.AlignHCenter
        rawText: root.timeText.length > 0 ? (root.stateLabel + " · " + root.timeText) : root.stateLabel
        role: HnTypographyRole.Caption
        color: HoloniightPalette.textMuted
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

    // Power-profile selector — only shown when power-profiles-daemon is available. Caption and
    // button row are direct children of the outer ColumnLayout so Qt.AlignHCenter centers them
    // against the full panel width (a nested ColumnLayout collapses to its content width and
    // left-aligns).
    HnLabel {
        Layout.alignment: Qt.AlignHCenter
        visible: root.profilesAvailable
        rawText: root.hoveredCaption.length > 0 ? root.hoveredCaption : " "
        role: HnTypographyRole.Caption
        color: HoloniightPalette.accentCyan
        font.weight: Font.Medium
    }

    RowLayout {
        id: profiles

        // Disable nested-layout fillWidth so Qt.AlignHCenter centers the intrinsic-width row.
        Layout.fillWidth: false
        Layout.alignment: Qt.AlignHCenter
        Layout.bottomMargin: 2
        visible: root.profilesAvailable
        spacing: 22

        ProfileButton {
            id: saverButton

            profileName: "power-saver"
            iconName: "power-profile-power-saver-symbolic"
            caption: qsTr("Power Saver")
            isActive: PowerProfilesService.activeProfile === "power-saver"
            isEnabled: PowerProfilesService.hasPowerSaver
            onActivated: (profile) => {
                return PowerProfilesService.setProfile(profile);
            }
        }

        ProfileButton {
            id: balancedButton

            objectName: "balancedProfileButton"
            profileName: "balanced"
            iconName: "power-profile-balanced-symbolic"
            caption: qsTr("Balanced")
            isActive: PowerProfilesService.activeProfile === "balanced"
            isEnabled: PowerProfilesService.hasBalanced
            onActivated: (profile) => {
                return PowerProfilesService.setProfile(profile);
            }
        }

        ProfileButton {
            id: performanceButton

            profileName: "performance"
            iconName: "power-profile-performance-symbolic"
            caption: qsTr("Performance")
            isActive: PowerProfilesService.activeProfile === "performance"
            isEnabled: PowerProfilesService.hasPerformance
            onActivated: (profile) => {
                return PowerProfilesService.setProfile(profile);
            }
        }

    }

    header: Component {
        PopupHeader {
            title: qsTr("Battery")
            iconSource: "battery-symbolic"
        }

    }

    component MetricRow: RowLayout {
        id: metricRow

        property string label: ""
        property string value: ""

        Layout.fillWidth: true
        spacing: 8

        HnLabel {
            Layout.fillWidth: true
            rawText: metricRow.label
            role: HnTypographyRole.Caption
            color: HoloniightPalette.textSecondary
            elide: Text.ElideRight
        }

        HnLabel {
            rawText: metricRow.value
            role: HnTypographyRole.Code
            color: HoloniightPalette.textPrimary
        }

    }

}

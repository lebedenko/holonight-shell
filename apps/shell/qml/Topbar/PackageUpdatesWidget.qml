pragma ComponentBehavior: Bound
import QtQuick
import HolonightShell
import Holonight.Core
import Holonight.Controls
import Holonight.Components

BarSection {
    id: root
    required property string barMonitorName
    property var service: PackageUpdatesService
    readonly property bool busy: root.service.loading || root.service.checking
    readonly property bool problem: root.service.errorMessage.length > 0
        || (root.service.checkMetadataAvailable && root.service.lastCheckTime > 0
            && !root.service.lastCheckSucceeded)
        || root.service.launchError.length > 0
    readonly property string badgeText: root.service.count > 99 ? "99+" : root.service.count.toString()
    readonly property string tooltipText: {
        let lines = [qsTr("%1 pending updates").arg(root.service.count)]
        if (root.service.ignoredCount > 0)
            lines.push(qsTr("%1 ignored").arg(root.service.ignoredCount))
        lines.push(qsTr("Download: %1 MiB").arg((root.service.downloadSizeBytes / 1048576).toFixed(1)))
        lines.push(qsTr("Data as of: %1").arg(root.timestamp(root.service.dataAsOf)))
        lines.push(qsTr("Last check: %1").arg(root.timestamp(root.service.lastCheckTime)))
        lines.push(qsTr("Snapshot fetched: %1").arg(root.timestamp(root.service.snapshotFetchedAt)))
        if (root.service.checking) lines.push(qsTr("Checking for updates…"))
        else if (root.service.loading) lines.push(qsTr("Loading updates…"))
        if (root.problem) lines.push(qsTr("Previous results may be stale"))
        if (root.service.errorMessage.length > 0) lines.push(root.service.errorMessage)
        if (root.service.checkMetadataAvailable && root.service.lastCheckTime > 0
                && !root.service.lastCheckSucceeded)
            lines.push(qsTr("Online check failed: %1").arg(root.service.lastCheckError || qsTr("unknown error")))
        if (root.service.launchError.length > 0) lines.push(root.service.launchError)
        return lines.join("\n")
    }
    function timestamp(seconds: real): string {
        return seconds > 0 ? new Date(seconds * 1000).toLocaleString(Qt.locale(), Locale.ShortFormat) : qsTr("unknown")
    }
    visible: root.service.launchError.length > 0
        || (root.service.available && (root.busy || root.problem || root.service.count > 0))
    enabled: visible
    implicitWidth: visible ? 44 : 0

    ExternalIcon {
        id: packageIcon
        anchors.centerIn: parent
        iconSize: 22
        iconName: "holonight-pkg-manager"
        tintColor: root.busy ? HoloniightPalette.primary
            : root.problem ? HoloniightPalette.warning : HoloniightPalette.primary
        SequentialAnimation on opacity {
            running: root.busy && root.visible
            loops: Animation.Infinite
            NumberAnimation { to: 0.35; duration: 600 }
            NumberAnimation { to: 1; duration: 600 }
            onRunningChanged: if (!running) packageIcon.opacity = 1
        }
    }
    Rectangle {
        visible: root.service.count > 0
        width: Math.max(15, badgeLabel.implicitWidth + 8)
        height: 15
        radius: height / 2
        color: root.busy ? HoloniightPalette.primary
            : root.problem ? HoloniightPalette.warning : HoloniightPalette.primary
        anchors.right: packageIcon.right
        anchors.top: packageIcon.top
        anchors.rightMargin: -6
        anchors.topMargin: -4
        HnLabel {
            id: badgeLabel
            anchors.centerIn: parent
            rawText: root.badgeText
            role: HnTypographyRole.Caption
            color: HoloniightPalette.onPrimary
        }
    }
    BarTooltipArea {
        id: tooltip
        barMonitorName: root.barMonitorName
        title: qsTr("Packages")
        description: root.tooltipText
    }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: {
            tooltip.dismissForClick()
            root.service.openPackages()
        }
    }
}

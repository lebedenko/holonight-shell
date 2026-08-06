import QtQuick
import QtTest
import Holonight.Core

import HolonightShell

TestCase {
    id: root

    name: "TrayItem"

    Component {
        id: trayItemComponent

        TrayItem {
            index: 0
            size: 36
            barMonitorName: "DP-1"
            service: "org.example.Tray"
            objectPath: "/StatusNotifierItem"
            iconName: ""
            attentionIconName: ""
            iconPixmapUrl: ""
            attentionPixmapUrl: ""
            status: "Active"
            title: "Tray"
            itemKey: "org.example.Tray:/StatusNotifierItem"
            tooltipTitle: ""
            tooltipDescription: ""
            tooltipIconName: ""
            hasUnread: false
        }
    }

    function semanticIconCandidate() {
        const candidates = [
            "folder-symbolic",
            "power-profile-balanced-symbolic",
            "kate-symbolic",
            "printer-warning"
        ]
        for (let index = 0; index < candidates.length; index++) {
            if (HnIconProvider.supportsSemanticColors(candidates[index]))
                return candidates[index]
        }
        return ""
    }

    function externalIcon(item) {
        const icon = findChild(item, "externalIcon")
        verify(icon)
        return icon
    }

    function test_semantic_named_icon_uses_tinted_renderer_when_available() {
        const candidate = root.semanticIconCandidate()
        if (candidate.length === 0)
            skip("No semantic theme icon available in this environment")

        const item = createTemporaryObject(trayItemComponent, null, {
            "iconName": candidate
        })
        verify(item)
        compare(root.externalIcon(item).usesSemanticTint, true)
    }

    function test_normal_named_app_icon_stays_on_plain_provider() {
        const item = createTemporaryObject(trayItemComponent, null, {
            "iconName": "rog-control-center"
        })
        verify(item)
        const icon = root.externalIcon(item)
        compare(icon.usesSemanticTint, false)
        compare(String(icon.resolvedExactSource), "image://icon/rog-control-center")
    }

    function test_pixmap_url_stays_on_plain_provider() {
        const item = createTemporaryObject(trayItemComponent, null, {
            "iconPixmapUrl": "image://tray/org.example.Tray:/StatusNotifierItem?v=1"
        })
        verify(item)
        const icon = root.externalIcon(item)
        compare(icon.usesSemanticTint, false)
        compare(String(icon.resolvedExactSource), "image://tray/org.example.Tray:/StatusNotifierItem?v=1")
    }
}

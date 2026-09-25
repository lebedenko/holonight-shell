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

    function externalIcon(item) {
        const icon = findChild(item, "externalIcon")
        verify(icon)
        return icon
    }

    function test_semantic_named_icon_uses_tinted_renderer_when_available() {
        const item = createTemporaryObject(trayItemComponent, null, {
            "iconName": "folder-symbolic"
        })
        verify(item)
        compare(root.externalIcon(item).usesSemanticTint, true)
    }

    function test_normal_named_app_icon_uses_semantic_renderer_without_forced_tint() {
        const item = createTemporaryObject(trayItemComponent, null, {
            "iconName": "rog-control-center"
        })
        verify(item)
        const icon = root.externalIcon(item)
        compare(icon.usesSemanticTint, true)
        compare(String(icon.resolvedExactSource), "")
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

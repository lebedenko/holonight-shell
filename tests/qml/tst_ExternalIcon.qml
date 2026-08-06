import QtQuick
import QtTest
import Holonight.Core

import Holonight.Components

TestCase {
    id: root

    name: "ExternalIcon"

    Component {
        id: iconComponent
        ExternalIcon {}
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

    function test_defaults() {
        const icon = createTemporaryObject(iconComponent, null)
        verify(icon)
        compare(icon.iconSize, 24)
        compare(icon.implicitWidth, 24)
        compare(icon.implicitHeight, 24)
        compare(icon.usesSemanticTint, false)
        compare(String(icon.resolvedExactSource), "")
    }

    function test_semantic_named_icon_uses_tinted_path_when_available() {
        const candidate = root.semanticIconCandidate()
        if (candidate.length === 0)
            skip("No semantic theme icon available in this environment")

        const icon = createTemporaryObject(iconComponent, null, {
            iconName: candidate
        })
        verify(icon)
        compare(icon.usesSemanticTint, true)
        compare(String(icon.resolvedExactSource), "")
    }

    function test_non_semantic_named_icon_uses_icon_provider() {
        const icon = createTemporaryObject(iconComponent, null, {
            iconName: "nonexistent-holonight-test-icon"
        })
        verify(icon)
        compare(icon.usesSemanticTint, false)
        compare(String(icon.resolvedExactSource), "image://icon/nonexistent-holonight-test-icon")
    }

    function test_prefer_semantic_tint_false_keeps_named_icon_exact() {
        const icon = createTemporaryObject(iconComponent, null, {
            iconName: root.semanticIconCandidate(),
            fallbackIconName: "application-x-executable",
            preferSemanticTint: false
        })
        verify(icon)
        compare(icon.usesSemanticTint, false)
        verify(String(icon.resolvedExactSource).startsWith("image://icon/"))
    }

    function test_absolute_path_becomes_file_url() {
        const icon = createTemporaryObject(iconComponent, null, {
            iconName: "/tmp/example.svg"
        })
        verify(icon)
        compare(icon.usesSemanticTint, false)
        compare(String(icon.resolvedExactSource), "file:///tmp/example.svg")
    }

    function test_file_url_remains_file_url() {
        const icon = createTemporaryObject(iconComponent, null, {
            iconName: "file:///tmp/example.svg"
        })
        verify(icon)
        compare(icon.usesSemanticTint, false)
        compare(String(icon.resolvedExactSource), "file:///tmp/example.svg")
    }

    function test_resource_url_remains_resource_url() {
        const icon = createTemporaryObject(iconComponent, null, {
            iconName: "qrc:/HolonightShell/logo.png"
        })
        verify(icon)
        compare(icon.usesSemanticTint, false)
        compare(String(icon.resolvedExactSource), "qrc:/HolonightShell/logo.png")
    }

    function test_fallback_icon_used_when_empty() {
        const icon = createTemporaryObject(iconComponent, null, {
            fallbackIconName: "dialog-information",
            preferSemanticTint: false
        })
        verify(icon)
        compare(icon.usesSemanticTint, false)
        compare(String(icon.resolvedExactSource), "image://icon/dialog-information")
    }

    function test_pixmap_url_used_without_named_icon() {
        const icon = createTemporaryObject(iconComponent, null, {
            pixmapUrl: "image://tray/example"
        })
        verify(icon)
        compare(icon.usesSemanticTint, false)
        compare(String(icon.resolvedExactSource), "image://tray/example")
    }
}

import QtQuick
import QtTest
import Holonight.Core

import HolonightShell

TestCase {
    id: root

    name: "HudFrame"

    Component {
        id: frameComponent

        HudFrame {
            width: 160
            height: 80
        }
    }

    function test_variants_map_to_semantic_roles() {
        const frame = createTemporaryObject(frameComponent, null)
        verify(frame)

        frame.variant = HudFrame.Popup
        compare(frame.surfaceRole, HnSurfaceRole.Popup)
        compare(frame.frameFill, HoloniightPalette.surfaceRaised)
        compare(frame.frameStroke, HoloniightPalette.borderPassive)
        frame.variant = HudFrame.Section
        compare(frame.surfaceRole, HnSurfaceRole.Hud)
        compare(frame.frameFill, HoloniightPalette.glassTint)
        compare(frame.frameStroke, HoloniightPalette.borderActive)
        frame.variant = HudFrame.Widget
        compare(frame.surfaceRole, HnSurfaceRole.Card)
        compare(frame.frameFill, HoloniightPalette.surfaceElevated)
        frame.variant = HudFrame.Sidebar
        compare(frame.surfaceRole, HnSurfaceRole.Panel)
        compare(frame.frameFill, HoloniightPalette.surfaceRaised)
        compare(frame.strokeWidth, HoloniightPalette.borderWidth)
    }

    function test_semantic_radius_and_explicit_override() {
        const frame = createTemporaryObject(frameComponent, null, { variant: HudFrame.Widget })
        verify(frame)
        verify(Number.isNaN(frame.cornerRadius))
        compare(frame.effectiveRadius, HnAppearance.resolve(HnSurfaceRole.Card, HnCornerStyle.Inherit,
                                                            156, 76, NaN, NaN,
                                                            HnAppearance.revision).radius)

        frame.cornerRadius = 5
        compare(frame.effectiveRadius, 5)
    }

    function test_resolved_path_is_exposed_for_aligned_decorations() {
        const frame = createTemporaryObject(frameComponent, null)
        verify(frame)
        verify(frame.pathData.length > 0)
        verify(frame.chamferedCorners >= 0)
    }
}

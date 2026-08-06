import QtQuick
import QtTest
import HolonightShell

TestCase {
    name: "BarFrame"

    Component {
        id: frameComponent

        BarFrame {
            width: 160
            height: 80
        }
    }

    function test_asymmetric_geometry_is_explicit() {
        const frame = createTemporaryObject(frameComponent, null)
        verify(frame)
        compare(frame.cornerRadius, 4)
        compare(frame.leftTopOffset, 0)
        compare(frame.leftBottomOffset, 0)
        compare(frame.rightTopOffset, 0)
        compare(frame.rightBottomOffset, 0)
        compare(frame.leftCornerCut, 0)
        compare(frame.rightCornerCut, 0)

        frame.leftTopOffset = 12
        frame.rightBottomOffset = 12
        frame.leftCornerCut = 8
        compare(frame.leftTopOffset, 12)
        compare(frame.rightBottomOffset, 12)
        compare(frame.leftCornerCut, 8)
    }
}

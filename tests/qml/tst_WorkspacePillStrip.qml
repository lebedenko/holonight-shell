import QtQuick
import QtTest
import HolonightShell

TestCase {
    name: "WorkspacePillStripQmlTests"

    Component {
        id: stripComponent
        WorkspacePillStrip { barMonitorName: "TEST-1"; windowStart: 1 }
    }

    function test_legacy_strip_remains_instantiable_during_model_migration() {
        const strip = createTemporaryObject(stripComponent, this)
        verify(strip !== null)
        verify(strip.implicitWidth > 0)
    }
}

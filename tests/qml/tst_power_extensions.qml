import QtQuick
import QtQuick.Layouts
import QtTest
import HolonightShell

// T-045: InhibitorSection visibility — hidden when no inhibitors
// T-046: ChargeLimitRow visibility — hidden when chargeLimit == -1 (default in tests)

TestCase {
    name: "PowerExtensionsQmlTests"

    // ── T-045: InhibitorSection ───────────────────────────────────────────────

    Component {
        id: inhibitorSectionComponent
        InhibitorSection {
            width: 300
        }
    }

    function test_inhibitor_section_hidden_when_no_inhibitors() {
        // FakeQmlServices registers SuspendInhibitorService with SkipInit — model is empty.
        compare(SuspendInhibitorService.inhibitorCount, 0,
                "Default fake service should have no inhibitors")

        var section = createTemporaryObject(inhibitorSectionComponent, null)
        verify(section !== null, "InhibitorSection should instantiate")
        compare(section.visible, false, "InhibitorSection must be hidden when count == 0")
    }

    // ── T-046: ChargeLimitRow ─────────────────────────────────────────────────

    Component {
        id: chargeLimitRowComponent
        ChargeLimitRow {
            width: 300
        }
    }

    function test_charge_limit_row_hidden_when_unavailable() {
        // BatteryService uses SkipInit — chargeLimit defaults to -1.
        compare(BatteryService.chargeLimit, -1,
                "Default fake BatteryService should report chargeLimit -1")

        var row = createTemporaryObject(chargeLimitRowComponent, null)
        verify(row !== null, "ChargeLimitRow should instantiate")
        compare(row.visible, false, "ChargeLimitRow must be hidden when chargeLimit == -1")
    }
}

import QtQuick
import QtTest
import Holonight.Core

TestCase {
    id: root

    name: "HolonightThemeSingletonTests"

    function test_theme_families_accessible_without_reference_error() {
        const families = HolonightTheme.themeFamilies
        compare(typeof families, "object")
    }

    function test_accent_options_for_scheme_accessible_without_reference_error() {
        const options = HolonightTheme.accentOptionsForScheme("dark")
        compare(typeof options, "object")
    }
}

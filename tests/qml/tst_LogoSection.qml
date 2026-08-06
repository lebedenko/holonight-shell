import QtQuick
import QtTest
import HolonightShell

TestCase {
    name: "LogoSection"

    readonly property string bundledLogo: "qrc:/HolonightShell/linux-logo/archlinux.svg"

    Component {
        id: logoSectionComponent

        LogoSection {
            barMonitorName: "TEST-1"
        }
    }

    function cleanup() {
        SystemInfoService.setLogo(bundledLogo, true)
    }

    function test_hnicon_reflects_system_info_service_logo_properties_data() {
        return [
            {
                tag: "bundled logo",
                source: bundledLogo,
                tinted: true
            },
            {
                tag: "file override",
                source: "file:///tmp/custom-logo.png",
                tinted: false
            },
            {
                tag: "pixmaps fallback",
                source: "file:///usr/share/pixmaps/linux.png",
                tinted: false
            }
        ]
    }

    function test_hnicon_reflects_system_info_service_logo_properties(data) {
        SystemInfoService.setLogo(data.source, data.tinted)
        const section = createTemporaryObject(logoSectionComponent, null)
        verify(section)

        const icon = findChild(section, "logoIcon")
        verify(icon)
        compare(icon.source, data.source)
        compare(icon.tinted, data.tinted)
    }
}

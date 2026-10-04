import QtQuick
import QtQuick.Layouts
import QtTest
import HolonightShell

TestCase {
    id: root
    name: "PopupContentSizing"
    width: 1200
    height: 1200
    visible: true
    when: windowShown

    Component { id: audio; AudioPopupContent {} }
    Component { id: network; NetworkPopupContent {} }
    Component { id: battery; BatteryPopupContent {} }
    Component { id: storage; StoragePopupContent {} }
    Component { id: weather; WeatherPopupContent {} }

    function test_natural_size_data() {
        return [
            { tag: "audio", component: audio },
            { tag: "network", component: network },
            { tag: "battery", component: battery },
            { tag: "storage", component: storage },
            { tag: "weather", component: weather }
        ]
    }

    function test_natural_size(data) {
        failOnWarning(/.*Binding loop detected.*/)
        const popup = createTemporaryObject(data.component, root, { width: 800, height: 1000 })
        verify(popup)
        tryVerify(() => popup.implicitWidth > 0 && popup.implicitHeight > 0)
        popup.width = Math.ceil(popup.implicitWidth)
        waitForRendering(popup)
        const desiredHeight = popup.implicitHeight
        popup.height = Math.ceil(desiredHeight)
        waitForRendering(popup)
        fuzzyCompare(popup.implicitHeight, desiredHeight, 1)
        const viewport = data.tag === "weather" ? findChild(popup, "weatherViewport") : popup.viewportItem
        verify(viewport)
        verify(!viewport.interactive)
        popup.height = 100
        tryVerify(() => viewport.interactive)
        fuzzyCompare(popup.implicitHeight, desiredHeight, 1)
    }

    function test_header_font_metrics_contribute_to_natural_height() {
        const popup = createTemporaryObject(network, root, { width: 600, height: 800 })
        verify(popup)
        const title = findChild(popup, "popupTitle")
        verify(title)
        waitForRendering(popup)
        const normal = popup.implicitHeight
        title.font.pointSize = title.font.pointSize * 4
        tryVerify(() => popup.implicitHeight > normal)
    }

    function test_audio_expansion_changes_height_without_changing_width() {
        const popup = createTemporaryObject(audio, root, { width: 800, height: 1000 })
        verify(popup)
        popup.outputExpanded = false
        popup.inputExpanded = false
        waitForRendering(popup)
        const collapsed = popup.implicitHeight
        const width = popup.implicitWidth
        popup.outputExpanded = true
        tryVerify(() => popup.implicitHeight > collapsed)
        fuzzyCompare(popup.implicitWidth, width, 1)
        popup.outputExpanded = false
        tryVerify(() => Math.abs(popup.implicitHeight - collapsed) < 1)
    }

    function test_network_information_expands_content() {
        const popup = createTemporaryObject(network, root, { width: 800, height: 1000 })
        verify(popup)
        waitForRendering(popup)
        const collapsed = popup.implicitHeight
        popup.infoOpen = true
        tryVerify(() => popup.implicitHeight > collapsed)
        popup.infoOpen = false
        tryVerify(() => Math.abs(popup.implicitHeight - collapsed) < 1)
    }

    function test_password_entry_survives_popup_resize() {
        const popup = createTemporaryObject(network, root, { width: 600, height: 700 })
        verify(popup)
        popup.passwordPrompt.ssid = "A long network name for the password prompt"
        popup.passwordPrompt.open()
        popup.passwordPrompt.passwordText = "test passphrase"
        popup.width = 320
        popup.height = 240
        waitForRendering(popup)
        compare(popup.passwordPrompt.passwordText, "test passphrase")
        verify(popup.passwordPrompt.height <= popup.height)
        verify(popup.passwordPrompt.width <= popup.width)
        popup.passwordPrompt.close()
    }

    function test_weather_natural_width_fits_six_hourly_cards() {
        const popup = createTemporaryObject(weather, root, { height: 1200 })
        verify(popup)
        popup.width = Math.ceil(popup.implicitWidth)
        const hourly = findChild(popup, "weatherHourlyList")
        verify(hourly)
        tryVerify(() => hourly.itemAtIndex(5) !== null)
        const sixth = hourly.itemAtIndex(5)
        verify(sixth.x + sixth.width <= hourly.width + 1)
    }

    function test_weather_keeps_both_sun_events_inside_narrow_details() {
        const popup = createTemporaryObject(weather, root, { width: 240, height: 1000 })
        verify(popup)
        const sunrise = findChild(popup, "weatherSunrise")
        const sunset = findChild(popup, "weatherSunset")
        verify(sunrise && sunset)
        waitForRendering(popup)
        verify(sunrise.width > 0 && sunset.width > 0)
        verify(sunrise.x + sunrise.width <= sunrise.parent.width + 1)
        verify(sunset.x + sunset.width <= sunset.parent.width + 1)
        verify(sunset.y >= sunrise.y + sunrise.height)
    }

    function cleanup() {
        PopupTestSeed.setStorageVolumeCount(0)
        PopupTestSeed.setAudioDeviceCount(0)
        PopupTestSeed.setBatteryMetrics(false)
        NetworkService.setNetworkCount(1)
    }

    function test_network_list_tracks_actual_count() {
        const popup = createTemporaryObject(network, root, { width: 800, height: 700 })
        verify(popup)
        waitForRendering(popup)
        const single = popup.implicitHeight
        NetworkService.setNetworkCount(8)
        tryVerify(() => popup.implicitHeight > single + 300)
        verify(popup.viewportItem.interactive)
        NetworkService.setNetworkCount(1)
        tryVerify(() => Math.abs(popup.implicitHeight - single) < 1)
        NetworkService.setNetworkCount(0)
        tryVerify(() => popup.implicitHeight < single)
    }

    function test_storage_collapsed_volumes_leave_only_the_header() {
        PopupTestSeed.setStorageVolumeCount(1)
        const popup = createTemporaryObject(storage, root, { width: 600, height: 800 })
        verify(popup)
        waitForRendering(popup)
        popup.toggleDriveCollapsed("popup-drive")
        waitForRendering(popup)
        const collapsed = popup.implicitHeight
        PopupTestSeed.setStorageVolumeCount(6)
        waitForRendering(popup)
        fuzzyCompare(popup.implicitHeight, collapsed, 1)
        popup.toggleDriveCollapsed("popup-drive")
        tryVerify(() => popup.implicitHeight > collapsed + 200)
        PopupTestSeed.setStorageVolumeCount(0)
        tryVerify(() => findChild(popup, "storageEmptyState").visible)
    }

    function test_audio_device_insertion_resizes_without_resetting_expansion() {
        const popup = createTemporaryObject(audio, root, { width: 800, height: 800 })
        verify(popup)
        waitForRendering(popup)
        const empty = popup.implicitHeight
        PopupTestSeed.setAudioDeviceCount(6)
        tryVerify(() => popup.implicitHeight > empty + 200)
        verify(popup.outputExpanded)
        verify(!popup.inputExpanded)
        PopupTestSeed.setAudioDeviceCount(0)
        tryVerify(() => Math.abs(popup.implicitHeight - empty) < 1)
    }

    function test_battery_metrics_resize_but_profile_focus_does_not() {
        const popup = createTemporaryObject(battery, root, { width: 320, height: 800, profilesAvailable: true })
        verify(popup)
        waitForRendering(popup)
        const empty = popup.implicitHeight
        PopupTestSeed.setBatteryMetrics(true)
        tryVerify(() => popup.implicitHeight > empty)
        const withMetrics = popup.implicitHeight
        const profile = findChild(popup, "balancedProfileButton")
        verify(profile)
        verify(profile.visible)
        profile.forceActiveFocus()
        waitForRendering(popup)
        fuzzyCompare(popup.implicitHeight, withMetrics, 1)
    }

    Component {
        id: synthetic
        PopupContentLayout {
            property int rows: 1
            width: 300
            height: Math.min(implicitHeight, 250)
            header: Component { PopupHeader { title: "Test" } }
            footer: Component { Item { implicitHeight: 40 } }
            Repeater {
                model: rows
                Rectangle { Layout.fillWidth: true; implicitHeight: 64 }
            }
        }
    }

    function test_rows_grow_then_scroll_and_shrink_without_blank_space() {
        const popup = createTemporaryObject(synthetic, root)
        verify(popup)
        waitForRendering(popup)
        const single = popup.height
        popup.rows = 8
        tryVerify(() => popup.viewportItem.interactive)
        compare(popup.height, 250)
        popup.viewportItem.contentY = popup.viewportItem.contentHeight - popup.viewportItem.height
        popup.rows = 1
        tryVerify(() => Math.abs(popup.height - single) < 1)
        tryVerify(() => Math.abs(popup.viewportItem.contentY) < 1)
        verify(!popup.viewportItem.interactive)
    }
}

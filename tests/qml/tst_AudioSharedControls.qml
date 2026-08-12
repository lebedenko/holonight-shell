import QtQuick
import QtTest
import Holonight.Core
import Holonight.Controls
import HolonightShell

TestCase {
    id: root

    name: "AudioSharedControlsQmlTests"
    width: 180
    height: 160
    when: windowShown

    Component {
        id: audioPopupComponent

        StatusPopup {
            width: 900
            height: 600
            popupId: "audio"
        }
    }

    Component {
        id: deviceListComponent

        AudioDeviceList {
            width: 640
            height: 128
            accentColor: HoloniightPalette.accentCyan
            isInput: false
            model: ListModel {
                ListElement {
                    deviceId: 7
                    name: "alsa_output.test"
                    description: "Test Speakers"
                    muted: false
                    volume: 72
                    isDefault: true
                    busType: "Analog"
                    channelCount: 2
                    sampleRate: 48000
                    codec: ""
                    iconName: "audio-speakers"
                }
                ListElement {
                    deviceId: 8
                    name: "alsa_output.secondary"
                    description: "Secondary Output"
                    muted: false
                    volume: 38
                    isDefault: false
                    busType: "Digital"
                    channelCount: 2
                    sampleRate: 48000
                    codec: ""
                    iconName: "audio-card"
                }
            }
        }
    }

    Component {
        id: reopenedDeviceListComponent

        AudioDeviceList {
            width: 640
            height: 128
            accentColor: HoloniightPalette.accentCyan
            isInput: false
            model: ListModel {
                ListElement {
                    deviceId: 7
                    name: "alsa_output.test"
                    description: "Test Speakers"
                    muted: false
                    volume: 72
                    isDefault: false
                }
                ListElement {
                    deviceId: 8
                    name: "alsa_output.secondary"
                    description: "Secondary Output"
                    muted: false
                    volume: 38
                    isDefault: true
                }
            }
        }
    }

    Component {
        id: streamListComponent

        ListView {
            width: 640
            height: 128
            model: ListModel {
                ListElement {
                    streamId: 11
                    name: "Now Playing"
                    application: "Test Player"
                    iconName: "multimedia-player"
                    muted: true
                    volume: 38
                }
            }
            delegate: AudioStreamDelegate {}
        }
    }

    Component {
        id: emptyDeviceListComponent

        AudioDeviceList {
            width: 640
            height: 240
            accentColor: HoloniightPalette.accentViolet
            isInput: true
            emptyText: "No microphones found"
        }
    }

    Component {
        id: emptyStreamListComponent

        AudioStreamList {
            width: 640
            height: 240
            emptyText: "No active streams"
        }
    }

    function test_status_popup_enables_rich_content_focus_scope() {
        const popup = createTemporaryObject(audioPopupComponent, root)
        verify(popup)

        const contentLoader = findChild(popup, "statusPopupContentLoader")
        verify(contentLoader)
        compare(contentLoader.active, true)
        compare(contentLoader.focus, true)
    }

    function test_device_delegate_maps_roles_and_application_controls() {
        const list = createTemporaryObject(deviceListComponent, root)
        verify(list)
        tryCompare(list, "count", 2)

        const delegate = list.itemAtIndex(0)
        const secondaryDelegate = list.itemAtIndex(1)
        verify(delegate)
        verify(secondaryDelegate)
        verify(delegate instanceof HnListDelegate)
        compare(delegate.title, "Test Speakers")
        compare(delegate.subtitle, "Analog • 2 channels • 48 kHz")
        compare(delegate.subtitlePresentation, HnListDelegate.SingleLine)
        compare(delegate.leadingContentAlignment, Qt.AlignVCenter)
        compare(delegate.checked, true)
        compare(delegate.sizeRole, HnControlSize.Large)
        compare(delegate.implicitHeight, 64)
        tryVerify(function() { return delegate.leadingItem !== null && delegate.trailingItem !== null })
        compare(findChild(delegate, "deviceVolumeSlider").value, 72)
        compare(findChild(delegate, "deviceVolumeText").text, "72%")
        verify(findChild(delegate, "deviceRadioIndicator"))
        verify(findChild(delegate, "deviceIcon"))

        list.model.setProperty(0, "isDefault", false)
        list.model.setProperty(1, "isDefault", true)
        tryCompare(delegate, "checked", false)
        tryCompare(secondaryDelegate, "checked", true)
        compare(delegate.selected, false)
        compare(secondaryDelegate.selected, true)
    }

    function test_device_list_reopens_without_stale_first_row_selection() {
        const list = createTemporaryObject(reopenedDeviceListComponent, root)
        verify(list)
        tryCompare(list, "count", 2)
        compare(list.currentIndex, -1)

        const firstDelegate = list.itemAtIndex(0)
        const defaultDelegate = list.itemAtIndex(1)
        verify(firstDelegate)
        verify(defaultDelegate)
        compare(firstDelegate.checked, false)
        compare(firstDelegate.selected, false)
        compare(defaultDelegate.checked, true)
        compare(defaultDelegate.selected, true)
    }

    function test_stream_delegate_maps_roles_and_application_controls() {
        const list = createTemporaryObject(streamListComponent, root)
        verify(list)
        tryCompare(list, "count", 1)

        const delegate = list.itemAtIndex(0)
        verify(delegate)
        verify(delegate instanceof HnListDelegate)
        compare(delegate.title, "Test Player")
        compare(delegate.subtitle, "Now Playing")
        compare(delegate.leadingContentAlignment, Qt.AlignVCenter)
        compare(delegate.checked, false)
        compare(delegate.sizeRole, HnControlSize.Large)
        compare(delegate.implicitHeight, 64)
        tryVerify(function() { return delegate.leadingItem !== null && delegate.trailingItem !== null })
        compare(findChild(delegate, "streamVolumeSlider").value, 38)
        compare(findChild(delegate, "streamVolumeText").text, "38%")
        verify(findChild(delegate, "streamMoreOptionsButton"))
    }

    function test_device_list_uses_shared_empty_state() {
        const deviceList = createTemporaryObject(emptyDeviceListComponent, root)
        verify(deviceList)
        compare(deviceList.count, 0)

        const deviceEmptyState = findChild(deviceList, "audioDeviceEmptyState")
        verify(deviceEmptyState)
        compare(deviceEmptyState.titleText, "No microphones found")
    }

    function test_stream_list_uses_shared_empty_state() {
        const streamList = createTemporaryObject(emptyStreamListComponent, root)
        verify(streamList)
        compare(streamList.count, 0)

        const streamEmptyState = findChild(streamList, "audioStreamEmptyState")
        verify(streamEmptyState)
        compare(streamEmptyState.titleText, "No active streams")
    }
}

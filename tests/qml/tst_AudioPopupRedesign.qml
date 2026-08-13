import QtQuick
import QtTest
import Holonight.Core
import HolonightShell

import "../../apps/shell/qml/Popups/Audio/AudioMetadataFormat.js" as AudioMetadataFormat

TestCase {
    id: root

    name: "AudioPopupRedesignQmlTests"
    width: 900
    height: 600
    visible: true
    when: windowShown

    function findDescendantOfType(item, predicate) {
        for (let i = 0; i < item.children.length; i++) {
            const child = item.children[i]
            if (predicate(child))
                return child
            const nested = findDescendantOfType(child, predicate)
            if (nested !== null)
                return nested
        }
        return null
    }

    Component {
        id: audioPopupComponent

        AudioPopupContent {
            width: 900
            height: 600
        }
    }

    Component {
        id: deviceListComponent

        AudioDeviceList {
            width: 640
            height: 240
            accentColor: HoloniightPalette.accentCyan
            isInput: false
            model: ListModel {
                ListElement {
                    deviceId: 1
                    name: "device-one"
                    description: "Device One"
                    muted: false
                    volume: 50
                    isDefault: true
                }
                ListElement {
                    deviceId: 2
                    name: "device-two"
                    description: "Device Two"
                    muted: false
                    volume: 30
                    isDefault: false
                }
            }
        }
    }

    Component {
        id: inputDeviceListComponent

        AudioDeviceList {
            width: 640
            height: 160
            accentColor: HoloniightPalette.accentViolet
            isInput: true
            model: ListModel {
                ListElement {
                    deviceId: 1
                    name: "selected-input"
                    description: "Selected Input"
                    iconName: "audio-headset"
                    muted: false
                    volume: 75
                    isDefault: true
                }
                ListElement {
                    deviceId: 2
                    name: "other-input"
                    description: "Other Input"
                    iconName: "audio-input-microphone"
                    muted: false
                    volume: 25
                    isDefault: false
                }
            }
        }
    }

    Component {
        id: applicationsSectionComponent

        AudioApplicationsSection {
            width: 640
            model: ListModel {
                ListElement { streamId: 1; name: "s1"; application: "App One"; iconName: ""; muted: false; volume: 10 }
                ListElement { streamId: 2; name: "s2"; application: "App Two"; iconName: ""; muted: false; volume: 20 }
                ListElement { streamId: 3; name: "s3"; application: "App Three"; iconName: ""; muted: false; volume: 30 }
                ListElement { streamId: 4; name: "s4"; application: "App Four"; iconName: ""; muted: false; volume: 40 }
                ListElement { streamId: 5; name: "s5"; application: "App Five"; iconName: ""; muted: false; volume: 50 }
                ListElement { streamId: 6; name: "s6"; application: "App Six"; iconName: ""; muted: false; volume: 60 }
            }
        }
    }

    Component {
        id: narrowDeviceListComponent

        AudioDeviceList {
            width: 348
            height: 80
            accentColor: HoloniightPalette.accentCyan
            isInput: false
            model: ListModel {
                ListElement {
                    deviceId: 1
                    name: "device-one"
                    description: "Device One"
                    muted: false
                    volume: 50
                    isDefault: true
                }
            }
        }
    }

    Component {
        id: narrowStreamListComponent

        AudioStreamList {
            width: 348
            height: 80
            model: ListModel {
                ListElement { streamId: 1; name: "s1"; application: "App One"; iconName: ""; muted: false; volume: 10 }
            }
        }
    }

    Component {
        id: inputMeterComponent

        InputLevelMeter {
            width: 100
            height: 18
        }
    }

    Component {
        id: volumeSliderComponent

        AudioVolumeSlider {
            width: 180
            value: 50
            accessibleName: "Test player volume"
        }
    }

    Component {
        id: signalSpy

        SignalSpy {}
    }

    Component {
        id: fourApplicationsSectionComponent

        AudioApplicationsSection {
            width: 640
            model: ListModel {
                ListElement { streamId: 1; name: "s1"; application: "One"; muted: false; volume: 10 }
                ListElement { streamId: 2; name: "s2"; application: "Two"; muted: false; volume: 20 }
                ListElement { streamId: 3; name: "s3"; application: "Three"; muted: false; volume: 30 }
                ListElement { streamId: 4; name: "s4"; application: "Four"; muted: false; volume: 40 }
            }
        }
    }

    Component {
        id: inputDeviceSectionComponent

        AudioDeviceSection {
            width: 380
            isInput: true
            expanded: true
        }
    }

    Component {
        id: emptyApplicationsSectionComponent

        AudioApplicationsSection {
            width: 380
            model: ListModel {}
        }
    }

    function test_default_state_output_expanded_input_collapsed() {
        const popup = createTemporaryObject(audioPopupComponent, root)
        verify(popup)

        const outputSection = findChild(popup, "outputDeviceSection")
        const inputSection = findChild(popup, "inputDeviceSection")
        verify(outputSection)
        verify(inputSection)
        compare(outputSection.expanded, true)
        compare(inputSection.expanded, false)

        const outputList = findChild(outputSection, "audioDeviceList")
        const inputList = findChild(inputSection, "audioDeviceList")
        verify(outputList)
        verify(inputList)
        compare(outputList.visible, true)
        compare(inputList.visible, false)
    }

    function test_device_sections_expand_and_collapse_independently() {
        const popup = createTemporaryObject(audioPopupComponent, root)
        verify(popup)

        const outputSummary = findChild(findChild(popup, "outputDeviceSection"), "audioCurrentDeviceRow")
        const inputSummary = findChild(findChild(popup, "inputDeviceSection"), "audioCurrentDeviceRow")
        verify(outputSummary)
        verify(inputSummary)

        outputSummary.forceActiveFocus()
        keyClick(Qt.Key_Return)
        compare(popup.outputExpanded, false)
        compare(popup.inputExpanded, false)

        inputSummary.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(popup.outputExpanded, false)
        compare(popup.inputExpanded, true)

        outputSummary.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(popup.outputExpanded, true)
        compare(popup.inputExpanded, true)
    }

    function test_hero_is_pinned_above_scrollable_device_content() {
        const popup = createTemporaryObject(audioPopupComponent, root)
        verify(popup)

        const pinnedHeader = findChild(popup, "audioPopupPinnedHeader")
        const headerSeparator = findChild(popup, "audioHeaderSeparator")
        const masterPanel = findChild(popup, "audioMasterPanel")
        const heroSeparator = findChild(popup, "audioHeroSeparator")
        const outputSection = findChild(popup, "outputDeviceSection")
        const viewport = findChild(popup, "audioPopupViewport")
        verify(pinnedHeader)
        verify(headerSeparator)
        verify(masterPanel)
        verify(heroSeparator)
        verify(outputSection)
        verify(viewport)
        verify(headerSeparator.y + headerSeparator.height <= masterPanel.y)
        compare(viewport.y, pinnedHeader.y + pinnedHeader.height)
        verify(masterPanel.y + masterPanel.height <= viewport.y)
        verify(outputSection.y >= 16)
    }

    function test_separators_keep_the_shared_control_opacity() {
        const popup = createTemporaryObject(audioPopupComponent, root)
        verify(popup)

        const headerSeparator = findChild(popup, "audioHeaderSeparator")
        const heroSeparator = findChild(popup, "audioHeroSeparator")
        const outputSeparator = findChild(popup, "audioOutputSeparator")
        const applicationsSeparator = findChild(popup, "audioApplicationsSeparator")
        const footerSeparator = findChild(popup, "audioFooterSeparator")
        verify(headerSeparator)
        verify(heroSeparator)
        verify(outputSeparator)
        verify(applicationsSeparator)
        verify(footerSeparator)
        compare(headerSeparator.centerOpacity, 1)
        compare(heroSeparator.centerOpacity, 1)
        compare(footerSeparator.centerOpacity, 1)
        compare(headerSeparator.x, -popup.separatorBleed)
        compare(headerSeparator.width, popup.width + popup.separatorBleed * 2)
        compare(heroSeparator.x, -popup.separatorBleed)
        compare(heroSeparator.width, popup.width + popup.separatorBleed * 2)
        compare(outputSeparator.x, -popup.separatorBleed)
        compare(outputSeparator.width, popup.width + popup.separatorBleed * 2)
        compare(applicationsSeparator.x, -popup.separatorBleed)
        compare(applicationsSeparator.width, popup.width + popup.separatorBleed * 2)
        const footerSeparatorPosition = footerSeparator.mapToItem(popup, 0, 0)
        compare(footerSeparatorPosition.x, -popup.separatorBleed)
        compare(footerSeparator.width, popup.width + popup.separatorBleed * 2)
    }

    function test_footer_defaults_to_tab_focus_hint() {
        const popup = createTemporaryObject(audioPopupComponent, root)
        verify(popup)

        const tabHint = findChild(popup, "tabKeyHint")
        verify(tabHint)
        compare(findChild(popup, "tabFocusHint").visible, true)
        compare(findChild(popup, "navigateHint").visible, false)
        verify(tabHint.leftPadding > 6)
        verify(tabHint.background.radius < tabHint.height / 2)
    }

    function test_footer_tracks_slider_device_and_summary_focus() {
        const popup = createTemporaryObject(audioPopupComponent, root)
        verify(popup)
        const slider = findChild(popup, "masterVolumeSlider")
        const summary = findChild(findChild(popup, "inputDeviceSection"), "audioCurrentDeviceRow")
        const deviceList = createTemporaryObject(deviceListComponent, root)
        verify(slider)
        verify(summary)
        tryCompare(deviceList, "count", 2)

        slider.forceActiveFocus()
        tryCompare(findChild(popup, "adjustHint"), "visible", true)
        compare(findChild(popup, "minMaxHint").visible, true)
        compare(findChild(popup, "muteHint").visible, true)

        const device = deviceList.itemAtIndex(0)
        device.forceActiveFocus()
        tryCompare(findChild(popup, "navigateHint"), "visible", true)
        compare(findChild(popup, "selectHint").visible, true)

        summary.forceActiveFocus()
        tryCompare(findChild(popup, "expandHint"), "visible", true)
        compare(popup.inputExpanded, false)
        keyClick(Qt.Key_Return)
        compare(popup.inputExpanded, true)
        compare(popup.outputExpanded, true)
    }

    function test_summary_rows_use_compact_radius_and_keep_default_badges() {
        const popup = createTemporaryObject(audioPopupComponent, root)
        verify(popup)

        const outputSection = findChild(popup, "outputDeviceSection")
        const inputSection = findChild(popup, "inputDeviceSection")
        const outputFrame = findChild(outputSection, "currentDeviceRowFrame")
        const inputFrame = findChild(inputSection, "currentDeviceRowFrame")
        const outputSummary = findChild(outputSection, "audioCurrentDeviceRow")
        const inputSummary = findChild(inputSection, "audioCurrentDeviceRow")
        const outputBadge = findChild(outputSection, "currentDeviceDefaultPill")
        const inputBadge = findChild(inputSection, "currentDeviceDefaultPill")
        compare(outputFrame.radius, 6)
        compare(inputFrame.radius, 6)
        compare(outputBadge.visible, true)
        compare(inputBadge.visible, true)

        compare(outputFrame.border.color, HoloniightPalette.borderPassive)
        outputSummary.forceActiveFocus()
        compare(outputFrame.border.color, HoloniightPalette.accentCyan)

        inputSummary.forceActiveFocus()
        compare(inputFrame.border.color, HoloniightPalette.accentViolet)
        compare(outputFrame.border.color, HoloniightPalette.borderPassive)
    }

    function test_master_mute_button_has_large_bordered_target() {
        const popup = createTemporaryObject(audioPopupComponent, root)
        verify(popup)

        const muteButton = findChild(popup, "masterMuteButton")
        const muteIcon = findChild(muteButton, "masterMuteButtonIcon")
        verify(muteButton)
        verify(muteIcon)
        compare(muteButton.width, 44)
        compare(muteButton.height, 44)
        compare(muteButton.sizeRole, HnControlSize.Large)
        compare(muteIcon.width, 22)
        compare(muteIcon.height, 22)
        compare(muteIcon.iconSize, 22)
        compare(muteIcon.tintColor, HoloniightPalette.textPrimary)
        compare(muteButton.background.border.color, HoloniightPalette.borderPassive)
        compare(muteButton.background.border.width, 1)
    }

    function test_master_and_device_sliders_use_cyan_accent() {
        const popup = createTemporaryObject(audioPopupComponent, root)
        verify(popup)
        const masterSlider = findChild(popup, "masterVolumeSlider")
        compare(masterSlider.accentColor, HoloniightPalette.accentCyan)

        const list = createTemporaryObject(deviceListComponent, root)
        verify(list)
        tryCompare(list, "count", 2)
        const selectedDelegate = list.itemAtIndex(0)
        const deviceSlider = findChild(selectedDelegate, "deviceVolumeSlider")
        compare(deviceSlider.accentColor, HoloniightPalette.accentCyan)
        verify(findChild(selectedDelegate, "currentDeviceDefaultPill") === null)
    }

    function test_device_icons_force_full_brightness_tint() {
        const list = createTemporaryObject(deviceListComponent, root)
        verify(list)
        tryCompare(list, "count", 2)
        const icon = findChild(list.itemAtIndex(0), "deviceIcon")
        const effect = findChild(icon, "audioIconTintEffect")
        verify(effect)
        compare(effect.brightness, 1)
        compare(effect.colorization, 1)
    }

    function test_summary_toggle_signals_preserve_other_section_state() {
        const popup = createTemporaryObject(audioPopupComponent, root)
        verify(popup)

        const outputSection = findChild(popup, "outputDeviceSection")
        const inputSection = findChild(popup, "inputDeviceSection")
        const outputList = findChild(outputSection, "audioDeviceList")
        const inputList = findChild(inputSection, "audioDeviceList")
        const inputRow = findChild(inputSection, "audioCurrentDeviceRow")
        verify(inputRow)

        inputRow.toggled()

        compare(outputList.visible, true)
        compare(inputList.visible, true)

        const outputRow = findChild(outputSection, "audioCurrentDeviceRow")
        outputRow.toggled()

        compare(outputList.visible, false)
        compare(inputList.visible, true)
    }

    function test_radio_circle_reflects_default_state() {
        const list = createTemporaryObject(deviceListComponent, root)
        verify(list)
        tryCompare(list, "count", 2)

        const firstDelegate = list.itemAtIndex(0)
        const secondDelegate = list.itemAtIndex(1)
        verify(firstDelegate)
        verify(secondDelegate)

        const firstRadio = findChild(firstDelegate, "deviceRadioIndicator")
        const secondRadio = findChild(secondDelegate, "deviceRadioIndicator")
        verify(firstRadio)
        verify(secondRadio)
        compare(firstRadio.color, HoloniightPalette.accentCyan)
        compare(secondRadio.color, Qt.color("transparent"))

        list.model.setProperty(0, "isDefault", false)
        list.model.setProperty(1, "isDefault", true)

        tryCompare(firstRadio, "color", Qt.color("transparent"))
        tryCompare(secondRadio, "color", HoloniightPalette.accentCyan)
    }

    function test_format_device_metadata_normal_device() {
        compare(AudioMetadataFormat.formatDeviceMetadata("Analog", 2, 48000, ""), "Analog • Stereo")
    }

    function test_format_device_metadata_omits_zero_channel_count() {
        compare(AudioMetadataFormat.formatDeviceMetadata("Analog", 0, 48000, ""), "Analog")
    }

    function test_format_device_metadata_omits_zero_sample_rate() {
        compare(AudioMetadataFormat.formatDeviceMetadata("Analog", 2, 0, ""), "Analog • Stereo")
    }

    function test_format_device_metadata_omits_sample_rate() {
        compare(AudioMetadataFormat.formatDeviceMetadata("Analog", 6, 44100, ""), "Analog • 6 channels")
    }

    function test_format_device_metadata_bluetooth_with_codec() {
        compare(AudioMetadataFormat.formatDeviceMetadata("Bluetooth", 2, 48000, "AAC"), "Bluetooth • AAC")
    }

    function test_format_device_metadata_omits_unknown_fields() {
        compare(AudioMetadataFormat.formatDeviceMetadata("Bluetooth", 2, 48000, ""), "Bluetooth")
        compare(AudioMetadataFormat.formatDeviceMetadata("Unknown", 0, 48000, ""), "")
    }

    function test_applications_section_caps_at_four_rows_and_show_all_expands() {
        const section = createTemporaryObject(applicationsSectionComponent, root)
        verify(section)

        const clipContainer = findChild(section, "audioApplicationsClipContainer")
        const streamList = findChild(section, "audioApplicationsStreamList")
        verify(clipContainer)
        verify(streamList)
        tryCompare(streamList, "count", 6)

        tryCompare(clipContainer, "height", clipContainer.collapsedHeight)

        section.showAll = true

        tryCompare(clipContainer, "height", streamList.contentHeight)
    }

    function test_show_all_is_hidden_at_four_streams() {
        const section = createTemporaryObject(fourApplicationsSectionComponent, root)
        verify(section)
        compare(findChild(section, "showAllToggle").visible, false)
    }

    function test_empty_sections_keep_space_for_empty_state() {
        const deviceSection = createTemporaryObject(inputDeviceSectionComponent, root)
        verify(deviceSection)
        const deviceList = findChild(deviceSection, "audioDeviceList")
        verify(deviceList)
        tryCompare(deviceList, "height", 72)

        const applicationsSection = createTemporaryObject(emptyApplicationsSectionComponent, root)
        verify(applicationsSection)
        const clipContainer = findChild(applicationsSection, "audioApplicationsClipContainer")
        verify(clipContainer)
        tryCompare(clipContainer, "height", 32)
        compare(findChild(applicationsSection, "audioApplicationsEmptyText").text,
                "No applications are playing audio")
    }

    function test_input_section_uses_input_accent_for_labels() {
        const section = createTemporaryObject(inputDeviceSectionComponent, root)
        verify(section)

        const currentLabel = findChild(section, "audioCurrentDeviceLabel")
        verify(currentLabel)
        compare(currentLabel.color, HoloniightPalette.accentViolet)

        const sectionLabel = findChild(section, "audioDeviceSectionLabel")
        verify(sectionLabel)
        compare(sectionLabel.color, HoloniightPalette.accentViolet)
    }

    function test_device_rows_keep_icons_and_values_neutral() {
        const inputList = createTemporaryObject(inputDeviceListComponent, root)
        verify(inputList)
        tryCompare(inputList, "count", 2)

        const selectedInput = inputList.itemAtIndex(0)
        const otherInput = inputList.itemAtIndex(1)
        const selectedSlider = findChild(selectedInput, "deviceVolumeSlider")
        const selectedIcon = findChild(selectedInput, "deviceIcon")
        const selectedVolume = findChild(selectedInput, "deviceVolumeText")
        const otherIcon = findChild(otherInput, "deviceIcon")
        const otherVolume = findChild(otherInput, "deviceVolumeText")

        compare(selectedSlider.accentColor, HoloniightPalette.accentViolet)
        compare(selectedIcon.tintColor, HoloniightPalette.textSecondary)
        compare(selectedVolume.color, HoloniightPalette.textMuted)
        compare(otherIcon.tintColor, HoloniightPalette.textSecondary)
        compare(otherVolume.color, HoloniightPalette.textMuted)

        const outputList = createTemporaryObject(deviceListComponent, root)
        verify(outputList)
        tryCompare(outputList, "count", 2)
        const selectedOutput = outputList.itemAtIndex(0)
        compare(findChild(selectedOutput, "deviceVolumeSlider").accentColor, HoloniightPalette.accentCyan)
        compare(findChild(selectedOutput, "deviceIcon").tintColor, HoloniightPalette.textSecondary)
        compare(findChild(selectedOutput, "deviceVolumeText").color, HoloniightPalette.textMuted)
    }

    function test_input_level_meter_uses_violet_section_accent() {
        const section = createTemporaryObject(inputDeviceSectionComponent, root)
        verify(section)
        compare(findChild(section, "inputDeviceLevelMeter").accentColor, HoloniightPalette.accentViolet)
    }

    function test_slider_keyboard_adjusts_clamps_and_requests_mute() {
        const slider = createTemporaryObject(volumeSliderComponent, root)
        verify(slider)
        const commitSpy = signalSpy.createObject(slider, { target: slider, signalName: "valueCommitted" })
        const muteSpy = signalSpy.createObject(slider, { target: slider, signalName: "muteRequested" })
        slider.forceActiveFocus()

        keyClick(Qt.Key_Right)
        compare(commitSpy.signalArguments[0][0], 55)
        keyClick(Qt.Key_Home)
        compare(commitSpy.signalArguments[1][0], 0)
        keyClick(Qt.Key_Left)
        compare(commitSpy.signalArguments[2][0], 0)
        keyClick(Qt.Key_End)
        compare(commitSpy.signalArguments[3][0], 100)
        keyClick(Qt.Key_M)
        compare(muteSpy.count, 1)
        compare(slider.Accessible.role, Accessible.Slider)
        compare(slider.Accessible.name, "Test player volume")
        compare(slider.accessibleValue, 100)
    }

    function test_device_rows_navigate_and_activate_from_keyboard() {
        const list = createTemporaryObject(deviceListComponent, root)
        verify(list)
        tryCompare(list, "count", 2)
        const first = list.itemAtIndex(0)
        const second = list.itemAtIndex(1)
        const clickedSpy = signalSpy.createObject(second, { target: second, signalName: "clicked" })
        first.forceActiveFocus()
        keyClick(Qt.Key_Down)
        tryCompare(second, "activeFocus", true)
        keyClick(Qt.Key_Space)
        compare(clickedSpy.count, 1)
        keyClick(Qt.Key_Up)
        tryCompare(first, "activeFocus", true)
        compare(first.Accessible.role, Accessible.RadioButton)
        compare(first.Accessible.checked, true)
    }

    function test_narrow_popup_rows_keep_volume_controls_in_bounds() {
        const deviceList = createTemporaryObject(narrowDeviceListComponent, root)
        verify(deviceList)
        tryCompare(deviceList, "count", 1)
        const deviceDelegate = deviceList.itemAtIndex(0)
        verify(deviceDelegate)
        const deviceSlider = findChild(deviceDelegate, "deviceVolumeSlider")
        verify(deviceSlider)
        const deviceSliderPosition = deviceSlider.mapToItem(deviceDelegate, 0, 0)
        verify(deviceSliderPosition.x >= 0)
        verify(deviceSliderPosition.x + deviceSlider.width <= deviceDelegate.width)

        const streamList = createTemporaryObject(narrowStreamListComponent, root)
        verify(streamList)
        tryCompare(streamList, "count", 1)
        const streamDelegate = streamList.itemAtIndex(0)
        verify(streamDelegate)
        const streamSlider = findChild(streamDelegate, "streamVolumeSlider")
        verify(streamSlider)
        const streamSliderPosition = streamSlider.mapToItem(streamDelegate, 0, 0)
        verify(streamSliderPosition.x >= 0)
        verify(streamSliderPosition.x + streamSlider.width <= streamDelegate.width)
    }

    function test_application_more_options_is_after_volume() {
        const streamList = createTemporaryObject(narrowStreamListComponent, root)
        verify(streamList)
        streamList.width = 640
        tryCompare(streamList, "count", 1)

        const delegate = streamList.itemAtIndex(0)
        const volumeText = findChild(delegate, "streamVolumeText")
        const moreButton = findChild(delegate, "streamMoreOptionsButton")
        verify(volumeText)
        verify(moreButton)
        compare(moreButton.visible, true)
        compare(moreButton.parent, volumeText.parent)
        verify(moreButton.parent.children.indexOf(moreButton) > moreButton.parent.children.indexOf(volumeText))
    }

    function test_settings_gear_is_accessible_and_opens_audio_page() {
        const header = createTemporaryObject(Qt.createComponent("../../apps/shell/qml/Popups/Audio/AudioPopupHeader.qml"), root)
        verify(header)
        const gear = findChild(header, "headerSettingsGear")
        verify(gear)
        compare(gear.enabled, true)
        compare(gear.activeFocusOnTab, true)
        compare(gear.Accessible.name, "Open Audio settings")
        verify(root.findDescendantOfType(gear, function(item) { return item instanceof MouseArea }) === null)

        const hideCount = StatusPopupSurface.hideCount
        gear.clicked()
        compare(SettingsNavigationService.lastOpenedPage, "audio")
        compare(StatusPopupSurface.hideCount, hideCount + 1)

        const list = createTemporaryObject(deviceListComponent, root)
        verify(list)

        const streamListComponentLocal = Qt.createComponent("../../apps/shell/qml/Popups/Audio/AudioStreamDelegate.qml")
        verify(streamListComponentLocal.status !== Component.Error, streamListComponentLocal.errorString())
    }

    function test_input_level_meter_renders_proportional_bars_without_error() {
        const meter = createTemporaryObject(inputMeterComponent, root)
        verify(meter)

        meter.level = 0
        const barsRow = meter.children[0]
        let bars = []
        for (let i = 0; i < barsRow.children.length; i++) {
            if (barsRow.children[i] instanceof Repeater)
                continue
            bars.push(findChild(barsRow.children[i], "inputLevelBar"))
        }
        compare(bars.length, 10)
        for (const bar of bars)
            compare(bar.color, HoloniightPalette.borderPassive)

        meter.level = 100
        for (const bar of bars)
            compare(bar.color, HoloniightPalette.accentCyan)

        meter.level = 45
        compare(bars[0].color, HoloniightPalette.accentCyan)
        compare(bars[9].color, HoloniightPalette.borderPassive)
    }
}

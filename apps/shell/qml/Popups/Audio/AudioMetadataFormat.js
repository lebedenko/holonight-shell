.pragma library

// Popup metadata intentionally stays terse. Detailed format and sample-rate information belongs
// in Settings.
function formatDeviceMetadata(busType, channelCount, sampleRate, codec) {
    const bus = busType && busType.length > 0 && busType.toLowerCase() !== "unknown" ? busType : ""
    const isBluetooth = bus.toLowerCase() === "bluetooth"
    const channelDescription = channelCount === 1 ? qsTr("Mono")
        : channelCount === 2 ? qsTr("Stereo")
        : channelCount > 2 ? qsTr("%1 channels").arg(channelCount) : ""
    const detail = isBluetooth ? (codec && codec.length > 0 ? codec : "") : channelDescription
    return [bus, detail].filter(part => part.length > 0).join(" • ")
}

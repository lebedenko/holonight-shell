.pragma library

// REQ-F-3003: "BusType • ChannelCount channels • SampleRate Hz" for standard devices;
// "BusType • Codec • SampleRate Hz" for Bluetooth devices. Empty parts are omitted.
function formatDeviceMetadata(busType, channelCount, sampleRate, codec) {
    const bus = busType && busType.length > 0 ? busType : qsTr("Unknown")
    const rateKhz = sampleRate / 1000
    const rate = sampleRate > 0
        ? qsTr("%1 kHz").arg(rateKhz.toLocaleString(Qt.locale(), "f", rateKhz % 1 === 0 ? 0 : 1)) : ""
    const middle = bus === "Bluetooth"
        ? (codec && codec.length > 0 ? codec : qsTr("Unknown"))
        : (channelCount > 0 ? qsTr("%1 channels").arg(channelCount) : "")
    return [bus, middle, rate].filter(part => part.length > 0).join(" • ")
}

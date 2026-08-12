# Audio Panel — Deferred Features

This document records audio-panel functionality that the design mockup
(`assets/dont-commit/01-audio-settings/`) implies but that the **first iteration of the
audio popup deliberately does not implement**. Each item is parked here for a future cycle
together with the reason it was deferred and a sketch of how it would be added.

The shipped first iteration is a focused mini-`pavucontrol`: three real tabs (Output Devices,
Input Devices, Applications), per-row mute + volume slider (0–100%), click-row-to-set-default
for devices, and an always-visible Master Volume bar. Everything below is **out of scope** for
that iteration.

---

## 1. Move stream to another device (the ⋮ row menu)

**Mockup:** each application row has a `⋮` overflow button that opens a menu to route that
stream to a different output (or input) device.

**Deferred because:** it needs a popdown menu surface, device-list enumeration inside the row,
and conflict handling when the target device disappears mid-menu. The backend plumbing already
exists (`AudioService.moveStreamToOutput` / `moveStreamToInput` →
`PulseAudioBackend::moveStreamToDevice` → `pa_context_move_sink_input_by_index`), so only the
QML menu and wiring are missing.

**How to add later:** add a `⋮` button to `AudioStreamDelegate`; on click show a small menu
(reuse the tray menu popup pattern) listing `AudioService.outputs`; on selection call
`AudioService.moveStreamToOutput(streamId, deviceId)`.

## 2. Device port & profile selection

**Mockup:** a device row can expose alternate **ports** (e.g. "Headphones" vs "Speakers" on the
same card) and **card profiles** (e.g. "Analog Stereo Duplex" vs "HDMI").

**Deferred because:** requires querying `pa_card_info` (profiles) and `pa_sink_info.ports` /
`active_port`, plus new model fields and a second-level UI (dropdowns per row). This is a
meaningful backend expansion (card enumeration is a separate PA object type not currently
tracked).

**How to add later:** add a `PulseAudioBackend` card subscription + `AudioCardModel`; expose
ports/profiles per device; render dropdowns in the device delegate; call
`pa_context_set_sink_port_by_index` / `pa_context_set_card_profile_by_index`.

## 3. Presets / modes ("Meeting Mode", "Headset Mode", etc.)

**Mockup:** quick-preset chips that apply a bundle of device/profile/volume settings at once.

**Deferred because:** presets are a policy layer on top of ports/profiles (item 2), and need a
persistence format (likely in `config.toml`) plus a definition of what each preset changes.
There is no backend or config support for this yet.

**How to add later:** depends on item 2 landing first; then add a `[audio.presets]` config
section and a preset bar in `AudioPopupContent`.

## 4. Over-amplification (volume above 100%)

**Mockup / pavucontrol parity:** sliders that allow boosting volume up to 150% (over-amplify).

**Deferred because:** the spec for this iteration clamps all volumes to **0–100%**
(`REQ-F-009`, `REQ-F-021`). The backend helper `volumeToPercent` already clamps to 150 and the
PA write path can express >100%, but the UI and the clamp policy intentionally cap at 100% to
keep the slider semantics simple and avoid accidental speaker damage.

**How to add later:** raise the slider max to 150, add a visual "over-amplified" zone past the
100% mark, and relax the clamp in `AudioVolumeSlider` / the service.

## 5. "+" add-device buttons

**Mockup:** a `+` affordance to add/connect devices.

**Deferred because:** "add device" maps to Bluetooth pairing / network-audio discovery, which is
outside the PulseAudio control surface entirely.

**How to add later:** define a dedicated pairing workflow and expose it as an action rather than a
device-list entry.

---

## Not deferred — already in this iteration

For clarity, these mockup elements **are** implemented and are *not* part of this deferral list:

- The header settings gear opens `holonight-settings` on page key `audio` through its
  `org.freedesktop.Application` D-Bus contract.
- Output / Input / Applications as real tabs (one section at a time)
- Per-row icon, name, description/media-title, mute toggle, volume slider, percentage
- DEFAULT badge + click-row-to-set-default (cyan for output, violet for input)
- Master Volume bar pinned to the bottom, bidirectionally synced with the default sink
- Live updates from external `pactl` changes (add/remove/volume/mute/default)
- Empty-state and "audio unavailable" messages
- Application icons via `image://icon/` with a generic audio fallback

# Bluetooth shell integration

Depends on the installed HoloNightSystem::Bluetooth component; no new daemon.
The executable owns presentation and agent orchestration. The bar hides without an
adapter; connected state includes a device count and optional battery tooltip.
The status popup groups Connected, Paired and Available devices and offers power,
connection, pairing, discovery and SettingsNavigationService.openPage("bluetooth").

Discovery begins on open, releases two seconds after close, and reopening cancels
the release. Explicit Stop persists until Start or reopening. Dismissal cancels
pairing. Incoming requests route to the focused monitor's registered widget anchor,
with top-center fallback. Only shell requests default agent status.

Pairing presents all Agent1 interactions inline; explicit successful pairing trusts
then connects, retaining partial successes. Adapter selection and all list operations
apply to one adapter. Audio routing remains owned by Audio.

Reference: [popup](../../mockups/bluetooth-popup.png),
[Settings](../../mockups/bluetooth-settings.png), and the provider Bluetooth SDD.
Acceptance includes offscreen UI tests, lint/qmltypes and live multi-monitor input.

## Popup layout and interaction

The content targets 480 logical pixels with a 72-pixel status header and 64-pixel
device rows. The shared surface frame owns the background, glow, monitor bounds,
anchoring and header/footer placement; the header and divided action footer remain pinned when
space permits. A circular blue Bluetooth emblem accompanies the selected adapter's
connected count and power switch. Multiple adapters retain a selector below it.

Connected and Paired headings disappear when empty. Available appears only while
scan is requested, with an animated indicator, scanning label and empty discovery
message. Stopping scanning hides results without cancelling inline pairing above
the sections. Unavailable, absent-adapter, powered-off and failure messages remain
explicit. Pending adapter operations disable power, scan and device controls;
pending device operations disable that device's actions and show Working status.

Rows elide names while exposing full names through tooltips and accessibility.
Connected rows show a battery only when reported (including 0%). Paired rows offer
Connect and discovered rows offer Pair with RSSI or Signal unknown. Connected and
paired rows or their chevrons toggle one inline action; action buttons do not
expand rows. Expansion clears when the device disappears or selection changes.
Device-ID reconciliation preserves delegates during live updates. Standard controls
provide keyboard activation and visible focus. Advanced management stays in Settings.

The header uses medium-weight type and the shared Hero switch size when supported
by the selected style. Device categories with known symbolic variants use those
monochrome icons; unknown provider icon names retain their normal lookup.
Section headings use accent blue. Hover feedback spans the full device row,
including its battery and actions. Footer actions use borderless tool buttons.

Each section owns a clipped scrolling viewport below its fixed heading, capped at
five normal rows. Available therefore shows no more than five rows before scrolling.
On shorter monitors, the section viewports share the available body height and
shrink independently. The outer popup body does not scroll in this mode; other
popups retain their existing whole-body scrolling. Keyboard focus reveals its row
inside the corresponding section viewport.

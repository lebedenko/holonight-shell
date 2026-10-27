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

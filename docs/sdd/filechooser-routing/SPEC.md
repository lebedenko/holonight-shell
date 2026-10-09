# FileChooser routing — I-003

Status: Accepted

Assigned repository: holonight-shell. Exact baseline:
25e1556bc5ffc06015a757d48e468f7c0d9c9ccb. Backend handoff: af1e9a4806f1b74aaffe392588c81a76d1275b2f, confirmed
on canonical origin/main and registered by umbrella checkpoint ae5ea86.

Add org.freedesktop.impl.portal.FileChooser=holonight-filechooser;gtk to the
HoloNight, Hyprland, Sway and labwc portal configurations. Preserve every existing
default and Settings preference, the Settings descriptor and bus identity, and
session environment propagation. Consume the installed backend descriptor named
holonight-filechooser.portal; do not implement FileChooser inside Shell.

Acceptance: source and staged configuration checks cover all four routes and
preserved defaults/Settings. Existing installation fixtures exercise relocated
prefixes. Run focused checks first, then required Shell tests and applicable
quality checks. Real-broker/manual ecosystem acceptance belongs to umbrella I-004.
No live installation or service restart is performed by this work package.

# Network Popup Information and Interaction Polish — Specification

**Project:** holonight-shell
**Version:** 1.0
**Date:** 2026-07-23
**Source review:** `/tmp/network-popup-review.md`

## Overview

This specification converts the network popup visual review into an incremental improvement pipeline. The existing
popup dimensions, overall hierarchy, section ordering, and theme are retained. The work focuses on removing duplicated
connection information, making the current-connection card more useful, replacing low-value idle transfer-rate tiles,
and tightening the interaction polish called out by the review.

The first delivery is deliberately limited to stable data available from NetworkManager: IPv4 address, Wi-Fi frequency
band, and device link bitrate. Connection duration, Wi-Fi generation naming, detailed security protocols, live traffic
graphs, keyboard-only help, and an expandable diagnostics panel are tracked as later stages because they require
additional data or interaction design.

## Goals

- Make the current-connection card the authoritative connection summary.
- Avoid repeating the same “Connected” state in the selected network row.
- Prefer stable, diagnostic connection facts over transfer-rate values that commonly read `0 Mbps`.
- Preserve the popup's current proportions and visual language.
- Deliver the work in small phases with automated and live-compositor verification.

## Non-Goals

- Changing the popup surface dimensions or section order.
- Replacing NetworkManager or the existing asynchronous backend query architecture.
- Adding a traffic graph or retaining download/upload telemetry in the first delivery.
- Inferring Wi-Fi generation from bitrate or frequency.
- Implementing gateway, DNS, MAC address, channel, or connection-duration reporting in the first delivery.
- Redesigning the Wi-Fi toggle, password dialog, settings action, or connection lifecycle.
- Introducing hardcoded colors or a new visual framework.

## Functional Requirements

### Current Connection Data

**REQ-F-001 — Active Wi-Fi frequency**
When the primary connection is Wi-Fi, the system SHALL expose the active access point frequency in MHz as a read-only
`NetworkService` property. When no active Wi-Fi access point is available, the value SHALL be `0`.

Acceptance:

- A 2.4 GHz access point reports its NetworkManager `Frequency` value through QML.
- A 5 GHz or 6 GHz access point reports its corresponding value through QML.
- Disconnecting or switching to wired clears the value to `0`.
- The NOTIFY signal is emitted only when the value changes.

**REQ-F-002 — Active link bitrate**
When the primary connection has a NetworkManager device, the system SHALL expose that device's current link bitrate in
megabits per second as a read-only `NetworkService` property. Unknown or unavailable bitrate SHALL be represented by
`0`.

Acceptance:

- For Wi-Fi, the backend converts `org.freedesktop.NetworkManager.Device.Wireless.Bitrate` from Kbit/s to Mbit/s.
- For wired devices, the backend uses the applicable NetworkManager speed property when available.
- The displayed value is not inferred from download/upload counters.
- Disconnecting clears the value to `0`.

**REQ-F-003 — Human-readable band label**
When an active Wi-Fi frequency is available, the popup SHALL display a localized band label using these ranges:

- `2400–2500 MHz` → `2.4 GHz`
- `4900–5900 MHz` → `5 GHz`
- `5925–7125 MHz` → `6 GHz`
- any other positive value → the frequency formatted in MHz
- `0` or unavailable → `Unavailable`

Acceptance: Unit or QML tests cover each named band, an unknown positive frequency, and the unavailable state.

### Current Connection Card

**REQ-F-004 — Stable summary metrics**
The current-connection card SHALL show three tiles in this order: `IP ADDRESS`, `LINK SPEED`, and `BAND`. The existing
`DOWNLOAD` and `UPLOAD` tiles SHALL be removed from the card.

Acceptance:

- IPv4 continues to show the current address or a localized unavailable/resolving state.
- Link speed shows `%1 Mbps` for a positive bitrate and `Unavailable` otherwise.
- Band follows REQ-F-003.
- The three tiles fit the current card and popup width without changing popup dimensions.

**REQ-F-005 — Rich connection subtitle**
For an active Wi-Fi connection, the card subtitle SHALL combine connection state and available band information without
claiming a security protocol that the backend has not identified. Wired, disconnected, VPN, and NetworkManager
unavailable states SHALL retain clear state-specific copy.

Acceptance:

- Active Wi-Fi with known frequency includes the band in the subtitle.
- Active Wi-Fi with unknown frequency still shows a valid connected state.
- VPN state remains visible and uses the existing violet emphasis.
- No subtitle claims WPA2, WPA3, Wi-Fi 5, Wi-Fi 6, or another unreported capability.

### Network List

**REQ-F-006 — Non-duplicated connected row**
The connected Wi-Fi delegate SHALL identify itself as `Current` instead of repeating `Connected`. The summary card
remains the authoritative detailed status.

Acceptance: With one active SSID, the current card presents the connection status and the selected list row subtitle
reads `Current`.

**REQ-F-007 — Signal group spacing**
Signal icons and percentage labels in both the current card and Wi-Fi delegates SHALL be visually grouped with a gap
approximately 4 px smaller than the current implementation, without overlap at `100%`.

Acceptance: At strengths `0`, `66`, and `100`, the icon and percentage remain legible and read as one group.

**REQ-F-008 — Signal quality emphasis**
Wi-Fi rows SHALL communicate signal quality through palette-driven emphasis. Strong signals (`>= 75`) use the primary
cyan emphasis, medium signals (`40–74`) use blue, and weak signals (`< 40`) use muted or urgent emphasis consistent with
the current HoloNight palette. Text contrast SHALL remain readable.

Acceptance: Representative values `25`, `55`, and `87` select three visually distinct palette-driven tiers.

**REQ-F-009 — Selected-row balance**
The selected Wi-Fi row border opacity SHALL be reduced by approximately 10–15% from its current value while preserving
the selected background and left rail.

Acceptance: The connected row remains unambiguous, but its border is visually subordinate to the background and rail.

### Secondary Controls

**REQ-F-010 — Rescan emphasis**
The Rescan control SHALL remain in the Wi-Fi section header and use lower resting emphasis than its hover or active
state. Scanning and disabled states SHALL remain clear.

Acceptance: At rest the control does not visually dominate the section title; hover, scanning, and disabled states are
distinguishable.

**REQ-F-011 — Action chevrons**
Action-row chevrons SHALL use a slightly brighter palette token or opacity than the current implementation while
remaining secondary to the action labels.

Acceptance: Chevrons are visible at normal viewing distance without becoming the primary accent.

## Non-Functional Requirements

**REQ-NF-001 — Incremental state updates**
New backend fields SHALL flow through `NetworkBackendState` and guarded `NetworkService` setters. Unchanged values SHALL
not emit redundant NOTIFY signals.

**REQ-NF-002 — Asynchronous polling preservation**
The implementation SHALL preserve the existing worker-query and queued-refresh behavior. No new synchronous D-Bus work
SHALL run on the QML/main thread.

**REQ-NF-003 — Graceful unavailable data**
Missing D-Bus properties, unsupported devices, disconnects, and NetworkManager loss SHALL produce neutral fallback
values and SHALL NOT leave stale link speed or frequency data visible.

**REQ-NF-004 — Theme and localization**
All colors SHALL come from `HoloniightPalette`, and all new user-visible QML strings SHALL use `qsTr()`.

**REQ-NF-005 — Accessibility and keyboard regression**
Existing pointer and keyboard interaction SHALL not regress. The selected network row, Rescan control, and action rows
SHALL preserve their current activation behavior.

**REQ-NF-006 — Testability**
Backend state propagation and formatting/tier behavior SHALL be covered by deterministic C++ or QML tests without
requiring a live NetworkManager instance.

## Constraints

- Keep QML resources under `qrc:/HolonightShell/`.
- Do not add dependencies or change generated Wayland files.
- Preserve existing project naming, formatting, and QML registration conventions.
- Keep `NetworkPopupContent.qml` compatible with the existing `StatusPopupSurface`.
- Do not alter popup geometry in this pipeline unless live verification proves content no longer fits; any such change
  requires a separate design decision.

## Deferred Pipeline

### Stage 2 — Keyboard-first help

Add focus-aware footer hints for arrow selection, Enter activation, `R` rescan, and Escape dismissal only after the
popup's actual focus and key-routing behavior is specified and testable.

### Stage 3 — Expandable diagnostics

Add a collapsed-by-default connection details panel for IP, gateway, DNS, MAC address, frequency, channel, link speed,
and security. This stage requires new backend fields, privacy-conscious presentation, arrow-key/click expansion, and a
clear relationship with the existing `Connection Information` action.

### Stage 4 — Optional traffic visualization

If live throughput remains valuable, reintroduce it as smoothed recent traffic with a small history visualization.
This stage must define sampling, smoothing, idle presentation, update cost, and behavior while the popup is closed.

## Completion Criteria

- Requirements REQ-F-001 through REQ-F-011 are implemented.
- Narrow C++ and QML tests pass.
- `task qml-lint` and `task qmltypes-check` pass for QML/CMake-facing changes.
- The popup is verified in a live Hyprland session for connected Wi-Fi, wired, disconnected, scanning, and unavailable
  states.
- No popup dimension change or unrelated network behavior regression is introduced.

# Network Popup Information and Interaction Polish — Design

## 1. Design Intent

The existing popup is visually mature. This design keeps its hierarchy and dimensions while making its information
density more intentional:

```text
Current connection card
  identity + state/band + signal
  IP address | link speed | band

Wi-Fi networks
  selected row: SSID + Current
  other rows: SSID + Secured/Open/Known

Actions
  existing rows with clearer chevrons
```

The card answers “what am I connected to?” and “what are the useful connection facts?” The list answers “what else can
I connect to?” The active row remains visible for orientation and disconnection, but does not repeat the card's full
status.

## 2. Current Architecture

```text
NetworkManager D-Bus
  ↓
QtNetworkManagerBackend::doQueryState()
  ↓ NetworkBackendState
NetworkService::onBackendStateChanged()
  ↓ guarded Q_PROPERTY setters
NetworkCurrentCard.qml / WifiNetworkDelegate.qml
```

Relevant files:

- `libs/holonight-services/src/network/NetworkManagerBackend.{h,cpp}`
- `libs/holonight-services/src/NetworkService.{h,cpp}`
- `apps/shell/qml/Popups/Network/NetworkCurrentCard.qml`
- `apps/shell/qml/Popups/Network/WifiNetworkDelegate.qml`
- `apps/shell/qml/Popups/Network/NetworkPopupContent.qml`
- `apps/shell/qml/Popups/Network/NetworkActionRow.qml`
- `tests/test_network_service.cpp`
- `tests/qml/tst_WifiNetworkDelegate.qml`

The model already exposes each visible access point's frequency. The service does not expose the active access point's
frequency or device bitrate directly, so the card cannot currently bind to stable diagnostics.

## 3. Backend Data Design

### 3.1 State fields

Extend `NetworkBackendState`:

```cpp
uint active_frequency_mhz{0};
uint active_link_speed_mbps{0};
```

Extend `NetworkService` with read-only properties:

```cpp
Q_PROPERTY(uint activeFrequencyMhz READ activeFrequencyMhz NOTIFY activeFrequencyMhzChanged)
Q_PROPERTY(uint activeLinkSpeedMbps READ activeLinkSpeedMbps NOTIFY activeLinkSpeedMbpsChanged)
```

Use unsigned integer values because both are non-negative NetworkManager measurements. `0` is the unavailable sentinel
and ensures disconnect/unavailable snapshots naturally clear stale UI.

### 3.2 Query flow

`updateActiveConnectionState()` already resolves the primary connection and its devices. Extend that flow:

1. Resolve `SpecificObject` from the active connection.
2. If it points to a Wi-Fi access point, read `Frequency` from the access point interface.
3. For each primary device:
   - if it is Wi-Fi, read `Bitrate` from `org.freedesktop.NetworkManager.Device.Wireless`;
   - if it is wired, read `Speed` from `org.freedesktop.NetworkManager.Device.Wired`;
   - keep `0` when the property is unavailable or invalid.
4. Store Mbps in the backend state. NetworkManager's Wi-Fi bitrate is Kbit/s, so convert with integer rounding suitable
   for display. Wired speed is already reported in Mb/s.
5. Continue collecting interface traffic counters internally for now; removing those service properties is outside
   this focused change and could break other consumers.

The implementation should reuse device paths already fetched for active state and statistics. It should not add a
second top-level scan.

### 3.3 Service propagation

`NetworkService::onBackendStateChanged()` applies both fields with private equality-guarded setters. The existing state
snapshot default values clear data after disconnect or NetworkManager loss.

The legacy `downloadSpeedText` and `uploadSpeedText` properties remain available during this pipeline even though the
card stops rendering them. Their eventual removal, if desired, is a separate cleanup after confirming no consumers
remain.

## 4. Presentation Design

### 4.1 Formatting helpers

Keep raw numeric properties in C++. Presentation remains in QML:

```text
bandText(0)    → Unavailable
bandText(2412) → 2.4 GHz
bandText(5180) → 5 GHz
bandText(6115) → 6 GHz
bandText(7300) → 7300 MHz

linkSpeedText(0)   → Unavailable
linkSpeedText(866) → 866 Mbps
```

The band thresholds are display classifications, not claims about Wi-Fi generation. They should be centralized in
`NetworkCurrentCard.qml` as pure functions so QML tests can exercise them.

### 4.2 Current card

Retain the 160 px preferred height unless live verification shows clipping. Replace only the tile contents and tighten
the existing signal group margin.

The subtitle priority is:

1. NetworkManager unavailable.
2. Wired connected, with VPN suffix when active.
3. Wi-Fi connected, with band when available and VPN state when active.
4. Existing connection status or disconnected fallback.

Do not display `secured` in the card unless security is sourced for the active connection. The list's boolean
`secured` role indicates that an access point advertises protection; it does not identify WPA version and should not
be promoted into a richer protocol claim.

### 4.3 Wi-Fi delegates

The active delegate keeps:

- selected background;
- cyan left rail;
- active font weight;
- click-to-disconnect behavior.

It changes:

- subtitle from `Connected` to `Current`;
- selected border alpha from `0.46` to approximately `0.40`;
- signal/percentage spacing from 6 px to approximately 2 px;
- signal emphasis from a binary selected/unselected choice to three palette-driven tiers.

Use a small readonly tier property or function local to the delegate. Avoid hardcoded color literals.

### 4.4 Rescan and actions

The Rescan control stays beside the `WI-FI NETWORKS` title. Lower its non-hover color/opacity, retain a clear cyan hover
state, and keep disabled/scanning contrast.

Brighten `NetworkActionRow` chevrons using an existing palette token or a modest opacity increase. Do not change row
height, click targets, labels, or signals.

## 5. Component and API Impact

```text
NetworkManagerBackend.h
  + active_frequency_mhz
  + active_link_speed_mbps

NetworkManagerBackend.cpp
  + active AP frequency query
  + device link-speed query/conversion

NetworkService.h/.cpp
  + two read-only QML properties
  + guarded setters and propagation

NetworkCurrentCard.qml
  ~ subtitle composition
  ~ IP / LINK SPEED / BAND tiles
  ~ signal spacing

WifiNetworkDelegate.qml
  ~ Current subtitle
  ~ tier emphasis, border alpha, spacing

NetworkPopupContent.qml
  ~ Rescan resting emphasis

NetworkActionRow.qml
  ~ chevron emphasis
```

No new QML components or CMake registrations are expected. If tests require a dedicated formatting component, prefer
testing exposed properties on `NetworkCurrentCard` rather than adding a production abstraction solely for tests.

## 6. Failure and Edge-State Behavior

| State | Frequency | Link speed | Card behavior |
|---|---:|---:|---|
| Connected Wi-Fi, full data | positive | positive | band and Mbps shown |
| Connected Wi-Fi, missing bitrate | positive | 0 | band shown; speed unavailable |
| Connected Wi-Fi, missing AP frequency | 0 | positive | speed shown; band unavailable |
| Connected wired | 0 | positive/0 | wired subtitle; band unavailable |
| Disconnected | 0 | 0 | disconnected state; metrics unavailable |
| NetworkManager unavailable | 0 | 0 | unavailable state; no stale metrics |

Property read failures are treated as missing data, not fatal backend errors. Existing logging conventions apply.

## 7. Testing Strategy

### C++

- Verify backend-state fixtures propagate frequency and link speed to `NetworkService`.
- Verify NOTIFY signals fire once on change and not for equal values.
- Verify disconnect/unavailable state clears both values.
- Add focused conversion/query helper tests if conversion logic is extracted into pure functions.

### QML

- Extend `tst_WifiNetworkDelegate.qml` to check `Current`, selected border treatment, and representative signal tiers.
- Add or extend a current-card test to check band/link-speed formatting and unavailable fallbacks.
- Preserve existing separator and icon tests.

### Static and build checks

- Run focused CTest targets first.
- Run `task qml-lint`.
- Run `task qmltypes-check` because C++ Q_PROPERTY additions affect generated QML types.
- Run `task format-check` for C++ changes.
- Run `task architecture-check` if include or target boundaries change.

### Live compositor verification

In a live Hyprland session, verify:

- connected 2.4/5/6 GHz Wi-Fi when available;
- selected row hierarchy and disconnect behavior;
- 0%, medium, and strong signal presentation;
- Rescan resting, hover, scanning, and disabled states;
- wired, disconnected, and NetworkManager-unavailable states;
- action chevron visibility;
- unchanged popup geometry and dismissal behavior.

## 8. Deferred Design Decisions

Keyboard hints require a defined focus owner, explicit key routing, and a policy for when hints appear. Expandable
diagnostics require additional D-Bus fields and a decision on whether the existing information action expands inline
or opens settings. Traffic graphs require sampling and lifecycle rules. Each should be promoted into its own scoped SDD
phase before implementation.

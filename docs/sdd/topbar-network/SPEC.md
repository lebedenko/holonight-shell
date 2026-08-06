# Network Widget Specification — EARS Format

Feature: `topbar-network` — Network status indicator for holonight-shell Wayland bar.

## Context

This specification defines the complete behavior of a network status widget in the top bar, composed of:
1. **C++ Service** (`NetworkService`): D-Bus interface to `org.freedesktop.NetworkManager`
2. **QML Display** (`NetworkSection`): Bar section rendering network state with icons, SSID, and signal strength

All QML colors derive from `HoloniightPalette` (no hardcoded hex). Icons load via the system icon theme. Service runs as a singleton; widget always visible (no hide logic).

---

## Requirements

### C++ Service — NetworkService

#### REQ-F-NS-001: D-Bus Service Registration
**The NetworkService shall initialize and connect to the system D-Bus instance when the application starts.**

- *Template*: Ubiquitous
- *Acceptance*: The service connects to `QDBusConnection::systemBus()` and is accessible to QML as a registered singleton before the UI renders.

#### REQ-F-NS-002: NetworkManager Availability
**The NetworkService shall expose a Q_PROPERTY `available` (bool, read-only, NOTIFY availableChanged) that indicates whether NetworkManager D-Bus service is reachable.**

- *Template*: Ubiquitous
- *Acceptance*: `available` becomes `true` when the NM D-Bus service is detected; becomes `false` when NM is unreachable or crashes.

#### REQ-F-NS-003: Network State Tracking
**The NetworkService shall expose a Q_PROPERTY `online` (bool, read-only, NOTIFY onlineChanged) that indicates active network connectivity.**

- *Template*: Ubiquitous
- *Acceptance*: `online` is `true` when NM state is `NM_STATE_CONNECTED_GLOBAL` or equivalent; otherwise `false`.

#### REQ-F-NS-004: Connection Type Classification
**The NetworkService shall expose a Q_PROPERTY `type` (int, read-only, NOTIFY typeChanged) as an enum mapping connection types to integer values: `None=0`, `WiFi=1`, `Wired=2`.**

- *Template*: Ubiquitous
- *Acceptance*: `type` reflects the primary active connection (or `0` if no connection); derived from the NM primary connection's device type.

#### REQ-F-NS-005: WiFi Network Identity
**The NetworkService shall expose a Q_PROPERTY `ssid` (QString, read-only, NOTIFY ssidChanged) containing the active WiFi network name when type is WiFi.**

- *Template*: Conditional
- *Condition*: Where the primary connection is WiFi
- *Acceptance*: `ssid` contains the UTF-8 SSID of the active WiFi network; is empty string for wired or offline states.

#### REQ-F-NS-006: WiFi Signal Strength
**The NetworkService shall expose a Q_PROPERTY `strength` (int, read-only, NOTIFY strengthChanged) representing WiFi signal strength as a percentage 0–100.**

- *Template*: Conditional
- *Condition*: Where the primary connection is WiFi
- *Acceptance*: `strength` is 0–100 (percent); is 0 when type is `Wired` or `None`.

#### REQ-F-NS-007: VPN Status Detection
**The NetworkService shall expose a Q_PROPERTY `vpnActive` (bool, read-only, NOTIFY vpnActiveChanged) that indicates whether a VPN connection is active alongside the primary connection.**

- *Template*: Ubiquitous
- *Acceptance*: `vpnActive` is `true` when an active VPN connection is present in NM's list of active connections; `false` otherwise.

#### REQ-F-NS-008: StateChanged Signal Subscription
**The NetworkService shall subscribe to the NetworkManager `StateChanged` D-Bus signal and update all properties (online, type, ssid, strength, vpnActive) when the signal fires.**

- *Template*: Event-driven
- *Trigger*: NetworkManager emits `StateChanged`
- *Response*: Fetch updated primary connection details from NM and emit corresponding NOTIFY signals
- *Acceptance*: Property updates occur within 100ms of NM state change; QML observers see changes immediately.

#### REQ-F-NS-009: Primary Connection Tracking
**The NetworkService shall query the NetworkManager `PrimaryConnection` property and traverse the D-Bus object tree to extract connection type, SSID (for WiFi), and active access point.**

- *Template*: Ubiquitous
- *Acceptance*: The service successfully fetches object introspection data from `/org/freedesktop/NetworkManager/Connection/<id>` and device paths without blocking the main thread.

#### REQ-F-NS-010: WiFi Signal Updates
**When the active WiFi connection's access point changes or its Strength property updates, the NetworkService shall fetch the new Strength value from the AP's D-Bus object.**

- *Template*: Event-driven
- *Trigger*: NM emits `PropertiesChanged` on the active access point's D-Bus path, or the primary connection property changes
- *Response*: Query the new active AP's `Strength` property and emit `strengthChanged`
- *Acceptance*: Signal strength updates appear in the UI within 200ms of NM change; strength values are 0–100 (not 0.0–1.0).

#### REQ-F-NS-011: Available False State Recovery
**If the NetworkManager D-Bus service is unavailable (available == false), the NetworkService shall attempt to reconnect by polling the D-Bus service every 2 seconds.**

- *Template*: Event-driven
- *Trigger*: `available` becomes `false` due to NM crash or unavailability
- *Response*: Start a 2-second polling timer to recheck NM reachability
- *Acceptance*: Upon NM restoration, `available` becomes `true` and all property subscriptions are re-established within one polling cycle.

#### REQ-NF-NS-012: Thread Safety
**The NetworkService shall not perform blocking D-Bus calls on the main Qt thread.**

- *Template*: Ubiquitous
- *Acceptance*: All D-Bus operations use async `QDBusInterface::asyncCall()` or delegate to a worker thread; no `QDBusInterface::call()` (synchronous) is used.

#### REQ-NF-NS-013: Signal Emission Guarantees
**The NetworkService shall emit NOTIFY signals only when property values actually change.**

- *Template*: Ubiquitous
- *Acceptance*: A property change signal is emitted once per unique value change; repeated identical updates do not trigger signals.

#### REQ-C-NS-014: System Bus Requirement
**The NetworkService shall use `QDBusConnection::systemBus()`, not the session bus.**

- *Template*: Ubiquitous
- *Acceptance*: The service connects exclusively to the system D-Bus instance where NetworkManager operates.

#### REQ-C-NS-015: NetworkManager Interface Version
**The implementation shall work with NetworkManager versions 1.20 and later (current stable and recent LTS distributions).**

- *Template*: Ubiquitous
- *Acceptance*: D-Bus paths and property names conform to NM 1.20+ API specification.

---

### QML Display — NetworkSection

#### REQ-F-QML-001: BarSection Extension
**NetworkSection shall be a QML type that extends BarSection and is always visible (never hidden, even when offline).**

- *Template*: Ubiquitous
- *Acceptance*: NetworkSection renders in the top bar on every frame; `visible: true` is hardcoded or implicit.

#### REQ-F-QML-002: Left-to-Right Layout
**The NetworkSection shall arrange its content left-to-right in the following order: [VPN icon if active] [primary icon] [SSID text if WiFi] [signal bars icon if WiFi] [strength% text if WiFi].**

- *Template*: Ubiquitous
- *Acceptance*: Items appear in the specified order; layout does not reflow when properties change (uses anchors or Row).

#### REQ-F-QML-003: Primary Icon Selection by Type
**The NetworkSection shall select and display a primary icon based on the current connection type:**
- **`type == None` (offline):** `network-offline-symbolic`
- **`type == Wired`:** `network-wired-symbolic`
- **`type == WiFi`:** `network-wireless-signal-{weak|ok|good|excellent}-symbolic` (chosen by strength quartile)

- *Template*: Conditional
- *Acceptance*: The correct icon name is constructed and passed to the image loader; icon appears immediately when type or strength changes.

#### REQ-F-QML-004: WiFi Strength Quartile Mapping
**When type is WiFi, the NetworkSection shall map the strength integer (0–100) to one of four signal icon names:**
- **1–25 %:** `weak`
- **26–50 %:** `ok`
- **51–75 %:** `good`
- **76–100 %:** `excellent`

- *Template*: Conditional
- *Condition*: Where `type == WiFi`
- *Acceptance*: A strength value of 50 yields the `ok` icon; 75 yields `good`; 100 yields `excellent`.

#### REQ-F-QML-005: VPN Icon Display
**When vpnActive is true, the NetworkSection shall display a `network-vpn-symbolic` icon before the primary icon.**

- *Template*: State-driven
- *State*: `vpnActive == true`
- *Behaviour*: The VPN icon is visible and positioned immediately before the primary connection icon
- *Acceptance*: The VPN icon appears when `vpnActive` changes from `false` to `true`; disappears when it returns to `false`.

#### REQ-F-QML-006: SSID Text Display
**When type is WiFi and ssid is non-empty, the NetworkSection shall display the SSID as text after the primary icon, using JetBrains Mono 13px.**

- *Template*: State-driven
- *State*: `type == WiFi && ssid != ""`
- *Behaviour*: SSID text is visible and positioned after the primary icon
- *Acceptance*: Text appears when SSID becomes available; disappears when switching to wired or offline.

#### REQ-F-QML-007: Signal Strength Percentage Display
**When type is WiFi, the NetworkSection shall display the signal strength as a percentage (format: "XX%") after the signal bars icon, using JetBrains Mono 13px.**

- *Template*: State-driven
- *State*: `type == WiFi`
- *Behaviour*: Strength percentage is visible and positioned after the signal icon
- *Acceptance*: Text shows "100%" for excellent signal, "0%" when offline; updates synchronously with strength property.

#### REQ-F-QML-008: Signal Bars Icon
**When type is WiFi, the NetworkSection shall display a signal-bars icon between the SSID text and strength percentage.**

- *Template*: Conditional
- *Condition*: Where `type == WiFi`
- *Acceptance*: A signal-bars icon (one of weak/ok/good/excellent) is visible between SSID and percentage when WiFi is active.

#### REQ-F-QML-009: Color — Online State
**When online is true (any active connection), all icons and text shall be colored with `HoloniightPalette.success` (green).**

- *Template*: State-driven
- *State*: `online == true`
- *Behaviour*: All visual elements use the success color
- *Acceptance*: Hex color of icons and text matches the design token for online state.

#### REQ-F-QML-010: Color — Offline State
**When online is false or available is false (no connection, or NM unreachable), all icons and text shall be colored with `HoloniightPalette.error` (red).**

- *Template*: State-driven
- *State*: `online == false || available == false`
- *Behaviour*: All visual elements use the error color
- *Acceptance*: Hex color of icons and text matches the design token for offline/error state.

#### REQ-F-QML-011: Offline Icon Display
**When available is false (NetworkManager D-Bus unreachable), the NetworkSection shall display the `network-offline-symbolic` icon in error color, regardless of the type property.**

- *Template*: State-driven
- *State*: `available == false`
- *Behaviour*: Only the offline icon is displayed; all other elements are hidden
- *Acceptance*: The offline icon appears and is colored red when NM becomes unreachable.

#### REQ-F-QML-012: Strength Percentage Animation
**The strength percentage text shall animate from its previous value to its new value over 200ms using an OutCubic easing curve (same pattern as AudioSection).**

- *Template*: Event-driven
- *Trigger*: `strength` property changes
- *Response*: Emit an intermediate animated value that eases from old to new over 200ms; update the displayed text in real-time
- *Acceptance*: Visual text transitions smoothly for 200ms when signal strength jumps; acceleration curve is OutCubic (not linear).

#### REQ-NF-QML-013: Text Alignment and Spacing
**SSID text and strength percentage shall use left-to-right alignment, and all elements shall be separated by consistent spacing (matching topbar design tokens).**

- *Template*: Ubiquitous
- *Acceptance*: Text is left-aligned; spacing between icons/text is visually consistent (verified against design assets).

#### REQ-C-QML-014: No Hardcoded Colors
**The NetworkSection shall not use hardcoded hex color values; all colors shall derive from `HoloniightPalette`.**

- *Template*: Ubiquitous
- *Acceptance*: Code review confirms zero hardcoded `#RRGGBB` values in NetworkSection.qml; all colors reference palette tokens.

#### REQ-C-QML-015: HoloniightPalette Import
**The NetworkSection shall import HoloniightPalette using the pattern `import Holonight` and reference colors as `HoloniightPalette.<token>`.**

- *Template*: Ubiquitous
- *Acceptance*: Import statement appears in NetworkSection.qml; no alternative color import patterns are used.

#### REQ-C-QML-016: System Icon Theme
**All icons (primary, VPN, signal bars) shall be loaded via `image://icon/<name>` and resolve to the system icon theme.**

- *Template*: Ubiquitous
- *Acceptance*: Icon sources use the `image://icon/` protocol; icons load from system theme or fallback gracefully if unavailable.

---

### Icons

#### REQ-F-ICON-001: Primary Offline Icon
**The `network-offline-symbolic` icon shall be available in the system icon theme and used when type is None or available is false.**

- *Template*: Ubiquitous
- *Acceptance*: Icon renders without error when requested; displays a recognizable offline/disconnected symbol.

#### REQ-F-ICON-002: Primary Wired Icon
**The `network-wired-symbolic` icon shall be available in the system icon theme and used when type is Wired and online is true.**

- *Template*: Ubiquitous
- *Acceptance*: Icon renders and displays a recognizable wired/Ethernet symbol.

#### REQ-F-ICON-003: WiFi Signal Icons
**Four WiFi signal strength icons shall be available in the system icon theme:**
- `network-wireless-signal-weak-symbolic`
- `network-wireless-signal-ok-symbolic`
- `network-wireless-signal-good-symbolic`
- `network-wireless-signal-excellent-symbolic`

- *Template*: Ubiquitous
- *Acceptance*: All four icons load and render; each displays a visually distinct number of signal bars (1, 2, 3, 4 respectively).

#### REQ-F-ICON-004: VPN Icon
**The `network-vpn-symbolic` icon shall be available in the system icon theme and displayed when vpnActive is true.**

- *Template*: Ubiquitous
- *Acceptance*: Icon renders when VPN is active; disappears when VPN is deactivated.

#### REQ-NF-ICON-005: Icon Size and DPI Scaling
**All icons shall be rendered at a consistent size (matching topbar icon standards) and scale correctly with system DPI.**

- *Template*: Ubiquitous
- *Acceptance*: Icons appear crisp at 100% and 150% DPI scaling; size is consistent with other topbar indicators (battery, audio, etc.).

---

### Animation

#### REQ-F-ANIM-001: Strength Percentage Interpolation
**The strength percentage text shall interpolate smoothly from its previous value to its new value over exactly 200ms when the strength property changes.**

- *Template*: Event-driven
- *Trigger*: `strength` property emits `strengthChanged`
- *Response*: Display an intermediate animated value that transitions from old to new
- *Acceptance*: Visual text updates continuously for 200ms; text shows correct old value at t=0ms and correct new value at t=200ms.

#### REQ-F-ANIM-002: Easing Curve
**The strength percentage animation shall use an OutCubic easing curve (not linear).**

- *Template*: Ubiquitous
- *Acceptance*: Easing type is `Easing.OutCubic`; animation slows toward the end (characteristic of OutCubic).

#### REQ-F-ANIM-003: Animation Property
**The animation shall be implemented using a QML Behavior on an intermediate integer property (following the AudioSection pattern).**

- *Template*: Ubiquitous
- *Acceptance*: Code uses `Behavior on <animatedProperty> { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }`.

#### REQ-F-ANIM-004: Icon Transitions
**When type or vpnActive changes, icons shall be swapped immediately (no cross-fade); text (SSID, strength) shall appear/disappear immediately.**

- *Template*: Event-driven
- *Trigger*: `type` or `vpnActive` property changes
- *Response*: Replace icon; show/hide text elements
- *Acceptance*: Icon changes within one frame (~16ms); no animation artifacts (e.g., fading, scaling).

---

### Error Handling

#### REQ-F-ERR-001: NM Service Unavailable Graceful Fallback
**If NetworkManager D-Bus service is unavailable, the service shall set available=false and the widget shall display the offline icon in error color.**

- *Template*: Unwanted
- *Condition*: If NetworkManager D-Bus service becomes unreachable
- *Response*: Set `available` to `false`; widget renders offline icon
- *Acceptance*: UI does not crash; offline state is visually distinct; user knows connectivity is unavailable.

#### REQ-F-ERR-002: Invalid SSID Handling
**If the SSID property contains invalid or non-UTF-8 data, the NetworkService shall sanitize or replace it with an empty string; the widget shall hide the SSID text.**

- *Template*: Unwanted
- *Condition*: If SSID data is malformed
- *Response*: Store empty string in `ssid` property; widget hides SSID text
- *Acceptance*: UI does not crash; SSID text area remains empty and does not display corrupted data.

#### REQ-F-ERR-003: Missing System Icons
**If a required icon (e.g., `network-wireless-signal-weak-symbolic`) is not available in the system icon theme, the widget shall display a fallback icon (e.g., a generic network icon).**

- *Template*: Unwanted
- *Condition*: If a system icon is not found
- *Response*: Load a fallback icon or placeholder
- *Acceptance*: UI does not show a broken/missing image; a recognizable icon (or placeholder) always renders.

#### REQ-F-ERR-004: D-Bus Connection Errors
**If a D-Bus call to NetworkManager fails (e.g., timeout, permission denied), the NetworkService shall log the error and retry after 2 seconds; it shall not crash the application.**

- *Template*: Unwanted
- *Condition*: If a D-Bus call fails
- *Response*: Log error; schedule a retry; continue operation with last-known state
- *Acceptance*: Application continues running; user sees stale data briefly until retry succeeds.

#### REQ-F-ERR-005: Strength Out-of-Range Clamping
**If NetworkManager provides a strength value outside 0–100, the NetworkService shall clamp it to [0, 100].**

- *Template*: Unwanted
- *Condition*: If `strength` is negative or > 100
- *Response*: Clamp to nearest boundary (0 or 100)
- *Acceptance*: UI always shows a valid 0–100 strength value; animation does not jump or behave unexpectedly.

#### REQ-NF-ERR-006: Error Logging
**All errors (D-Bus failures, missing icons, invalid data) shall be logged via Qt's logging system (qWarning, qCritical) with sufficient context to debug issues.**

- *Template*: Ubiquitous
- *Acceptance*: Logs include timestamp, function name, error details, and recovery action; can be reviewed in systemd journal or application log.

---

## Verification Strategy

Each requirement shall be verified by:
1. **Unit Tests**: C++ service D-Bus calls, property updates, signal emissions (GTest)
2. **Integration Tests**: Service lifecycle, property binding to QML
3. **Manual Visual Tests**: 
   - Launch in a Wayland session with NetworkManager running
   - Toggle network connectivity (e.g., disconnect WiFi, enable VPN, switch to wired)
   - Observe icon changes, SSID/strength updates, color transitions, animations
   - Verify layout remains stable and icons load from system theme
4. **Error Injection Tests**: Kill/restart NetworkManager; simulate missing icons; check graceful degradation

---

## Design Rationale

- **Always Visible**: Unlike audio (which hides in silent mode), network status is critical for troubleshooting; always showing it provides transparency.
- **System D-Bus**: NM operates on system bus; session bus access would require escalation and is not standard.
- **Strength Animation**: Smoothing avoids jarring jumps as signal fluctuates; OutCubic easing matches the design language of other animated indicators.
- **Color Coding**: Green (online) vs. red (offline) provides instant visual feedback; consistent with platform conventions.
- **Icon Theme**: Leverages system icons for consistency and respects user icon preferences (dark, light, custom themes).
- **VPN Indicator**: VPN active alongside primary connection is a distinct state worth showing (security-conscious users need this).

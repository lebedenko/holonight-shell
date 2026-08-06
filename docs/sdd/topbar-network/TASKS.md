# SDD Tasks — topbar-network

- [x] T-001: Add NetworkService sources to CMakeLists.txt
  - REQs: (CMake integration)
  - Check: CMakeLists.txt lists `src/NetworkService.h` and `src/NetworkService.cpp` in target_sources; NetworkSection.qml has QT_RESOURCE_ALIAS set to "Topbar/NetworkSection.qml".

- [x] T-002: Create NetworkService.h with class skeleton, Q_ENUMs, and Q_PROPERTY declarations
  - REQs: REQ-F-NS-001, REQ-F-NS-002, REQ-F-NS-003, REQ-F-NS-004, REQ-F-NS-005, REQ-F-NS-006, REQ-F-NS-007
  - Check: Header declares ConnectionType Q_ENUM (None=0, WiFi=1, Wired=2); all six read-only Q_PROPERTYs (available, online, type, ssid, strength, vpnActive) with NOTIFY signals; QML_ELEMENT and QML_SINGLETON macros present; private members and method signatures match design §4.1.

- [x] T-003: Implement NetworkService constructor with system bus check and poll timer initialization
  - REQs: REQ-F-NS-001, REQ-F-NS-002, REQ-C-NS-014, REQ-F-NS-011
  - Check: Constructor initializes all member variables; checks if NM service exists on system bus via introspect; if NM absent, sets available=false and starts poll_timer_ (2s interval); if NM present, calls queryNmState() and subscribes to StateChanged signal.

- [x] T-004: Implement NetworkService::queryNmState() → queryPrimaryConnection() async chain
  - REQs: REQ-F-NS-003, REQ-F-NS-008, REQ-NF-NS-012
  - Check: queryNmState uses asyncGet to fetch NM State property; callback extracts uint, sets online (true if == 70, else false); then calls queryPrimaryConnection (which also uses asyncGet and chains to queryConnectionType).

- [x] T-005: Implement NetworkService::queryConnectionType() with Wired/WiFi routing
  - REQs: REQ-F-NS-004, REQ-F-NS-005, REQ-F-NS-006, REQ-F-NS-009
  - Check: Fetches Type property from active connection path; classifies as "802-3-ethernet" (setType(Wired=2), call scanVpn()) or "802-11-wireless" (extract device path, call queryWifiDevice()); stores current_primary_path_ snapshot for stale-guard checks.

- [x] T-006: Implement NetworkService::queryWifiDevice() → queryAccessPoint() async chain
  - REQs: REQ-F-NS-005, REQ-F-NS-006, REQ-F-NS-010
  - Check: Fetches ActiveAccessPoint from wireless device; chains to queryAccessPoint(apPath) which fetches Ssid (as QByteArray, converted to QString via fromUtf8, checked for replacement chars and replaced with "" if found) and Strength (as uchar, clamped to [0,100]); calls scanVpn().

- [x] T-007: Implement NetworkService::onApPropertiesChanged() slot for strength-only updates
  - REQs: REQ-F-NS-006, REQ-F-NS-010
  - Check: Slot receives interface, changed QVariantMap, invalidated QStringList; if "Strength" in changed, extracts value as uchar, clamps to [0,100], calls setStrength(); does not trigger full re-query.

- [x] T-008: Implement NetworkService::scanVpn() to detect active VPN connections
  - REQs: REQ-F-NS-007
  - Check: Fetches ActiveConnections array from NM root; iterates each path, uses asyncGet to fetch Type property; if Type == "vpn", sets vpnActive=true; if no VPN found, sets vpnActive=false.

- [x] T-009: Implement NetworkService::onPollTimer() slot for NM recovery when available=false
  - REQs: REQ-F-NS-002, REQ-F-NS-011, REQ-F-ERR-004
  - Check: Timer fires every 2s when available=false; attempts to introspect NM; if successful, stops timer, sets available=true, calls queryNmState() and subscribes StateChanged; logs recoveries and errors.

- [x] T-010: Implement NetworkService property setter methods with change guards
  - REQs: REQ-NF-NS-013, REQ-F-NS-002, REQ-F-NS-003, REQ-F-NS-004, REQ-F-NS-005, REQ-F-NS-006, REQ-F-NS-007
  - Check: Each setter (setAvailable, setOnline, setType, setVpnActive, setSsid, setStrength) compares new value to member variable; only calls member assignment and emits NOTIFY signal if value differs; logging includes old/new values for debugging.

- [x] T-011: Implement NetworkService destructor and AP subscription management (unsubscribe on path change)
  - REQs: REQ-F-NS-010, REQ-NF-NS-012
  - Check: Destructor deletes poll_timer_ if exists; queryAccessPoint(newApPath) guards against duplicate subscriptions (early return if newApPath == current_ap_path_); unsubscribes PropertiesChanged from old AP path before subscribing to new; captured current_ap_path_ prevents stale responses.

- [x] T-012: Register NetworkService singleton in main.cpp alongside BatteryService and AudioService
  - REQs: REQ-F-NS-001
  - Check: main.cpp instantiates NetworkService(&app) and registers it via qmlRegisterSingletonInstance("HolonightShell", 1, 0, "NetworkService", <instance>); NetworkService is accessible to QML as NetworkService.available, .online, .type, etc.

- [x] T-013: Create NetworkSection.qml BarSection skeleton with computed properties (wifiTier, primaryIconName, iconColor, displayStrength)
  - REQs: REQ-F-QML-001, REQ-F-QML-003, REQ-F-QML-004, REQ-F-QML-009, REQ-F-QML-010
  - Check: NetworkSection extends BarSection (id: root); readonly properties compute wifiTier (strength <= 25 → "weak", <= 50 → "ok", <= 75 → "good", else "excellent"); primaryIconName (offline → "network-offline-symbolic", wired → "network-wired-symbolic", wifi → "network-wireless-signal-{tier}-symbolic"); iconColor (online && available → success, else error); property int displayStrength with Behavior NumberAnimation (200ms, OutCubic).

- [x] T-014: Implement NetworkSection.qml Row layout with all icon/text elements and visibility bindings
  - REQs: REQ-F-QML-002, REQ-F-QML-003, REQ-F-QML-005, REQ-F-QML-006, REQ-F-QML-007, REQ-F-QML-008, REQ-C-QML-016
  - Check: Row contains vpnIcon (visible: vpnActive, source: "image://icon/network-vpn-symbolic"); primaryIcon (always visible, source: computed primaryIconName); ssidLabel (visible: type==1 && ssid!="", text: ssid, font: JetBrains Mono 13px); signalIcon (visible: type==1, source: computed primaryIconName); strengthLabel (visible: type==1, text: displayStrength+"%", font: JetBrains Mono 13px); all icons colored via ColorOverlay(color: iconColor); all text elements use color: iconColor.

- [x] T-015: Implement NetworkSection.qml implicitWidth calculation and TextMetrics fixed-width for strength label
  - REQs: REQ-NF-QML-013
  - Check: implicitWidth computed as: primaryIcon.width + 8, plus (vpnActive ? vpnIcon.width + spacing : 0), plus (ssidLabel.visible ? ssidLabel.contentWidth + spacing : 0), plus (type==1 ? signalIcon.width + spacing + strengthLabel.contentWidth + spacing : 0); TextMetrics with text: "100%" pins strengthLabel.width to fixed value to prevent jitter.

- [x] T-016: Insert NetworkSection into TopBar.qml between AudioSection and BatterySection
  - REQs: REQ-F-QML-001
  - Check: TopBar.qml Row lists AudioSection, then NetworkSection (with Layout.alignment: Qt.AlignVCenter), then BatterySection; NetworkSection is instantiated and bound to root properties without errors.

- [x] T-017: Add HoloniightPalette import to NetworkSection.qml; verify no hardcoded color values
  - REQs: REQ-C-QML-014, REQ-C-QML-015
  - Check: NetworkSection.qml imports Holonight; all colors reference HoloniightPalette.success or HoloniightPalette.error; code review shows zero hardcoded #RRGGBB values.

- [x] T-018: Build project and verify qmllint passes with no errors
  - REQs: (Build verification)
  - Check: `task build` completes without CMake or linker errors; `task qml-lint` reports no diagnostics on src/qml/Topbar/NetworkSection.qml.

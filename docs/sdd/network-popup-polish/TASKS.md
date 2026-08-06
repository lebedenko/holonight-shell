# SDD Tasks — network-popup-polish

## Phase 1 — Backend Data

- [x] T-001: Add active frequency and link-speed fields to `NetworkBackendState`
  - REQs: REQ-F-001, REQ-F-002, REQ-NF-003
  - Check: fields default to `0`; disconnected and unavailable snapshots cannot retain stale values.

- [x] T-002: Query active access point frequency
  - REQs: REQ-F-001, REQ-NF-002
  - Check: `updateActiveConnectionState()` reads `Frequency` from the active Wi-Fi access point and leaves `0` for
    wired, missing, or invalid access-point paths.

- [x] T-003: Query and normalize active device link speed
  - REQs: REQ-F-002, REQ-NF-002, REQ-NF-003
  - Check: Wi-Fi `Bitrate` is converted from Kbit/s to Mbit/s; wired `Speed` is used when available; unsupported or
    invalid properties yield `0`.

- [x] T-004: Expose `activeFrequencyMhz` and `activeLinkSpeedMbps` on `NetworkService`
  - REQs: REQ-F-001, REQ-F-002, REQ-NF-001
  - Check: both are read-only Q_PROPERTY values with NOTIFY signals and equality-guarded private setters.

- [x] T-005: Propagate backend values and clear them with connection state
  - REQs: REQ-F-001, REQ-F-002, REQ-NF-001, REQ-NF-003
  - Check: `onBackendStateChanged()` applies both values; disconnect and NetworkManager loss update QML to `0`.

## Phase 2 — Backend Tests

- [x] T-006: Test active diagnostic state propagation
  - REQs: REQ-F-001, REQ-F-002, REQ-NF-006
  - Check: `tests/test_network_service.cpp` verifies positive values, changed values, and unavailable fallbacks without
    a live NetworkManager instance.

- [x] T-007: Test NOTIFY guards and stale-data clearing
  - REQs: REQ-NF-001, REQ-NF-003, REQ-NF-006
  - Check: QSignalSpy coverage confirms one emission per change, no emission for equal values, and emissions when
    positive values return to `0`.

## Phase 3 — Current Connection Card

- [x] T-008: Add band and link-speed formatting to `NetworkCurrentCard.qml`
  - REQs: REQ-F-003, REQ-F-004, REQ-NF-004
  - Check: 2.4, 5, and 6 GHz bands, unknown MHz, positive Mbps, and unavailable values format as specified.

- [x] T-009: Replace Download/Upload tiles with Link Speed/Band
  - REQs: REQ-F-004
  - Check: tile order is IP Address, Link Speed, Band; existing popup dimensions remain unchanged and no tile
    overflows at representative values.

- [x] T-010: Enrich the current connection subtitle without unsupported claims
  - REQs: REQ-F-005, REQ-NF-003, REQ-NF-004
  - Check: Wi-Fi includes the band when known; VPN, wired, disconnected, and unavailable copy remains correct; no
    inferred Wi-Fi generation or WPA version is displayed.

- [x] T-011: Tighten current-card signal grouping
  - REQs: REQ-F-007
  - Check: the icon-to-percentage gap is reduced by approximately 4 px and `100%` does not overlap or clip.

## Phase 4 — Network List and Secondary Polish

- [x] T-012: Replace the connected delegate subtitle with `Current`
  - REQs: REQ-F-006, REQ-NF-004
  - Check: the connected row reads `Current`; non-connected Known/Secured/Open copy and click behavior are unchanged.

- [x] T-013: Add three-tier signal quality emphasis
  - REQs: REQ-F-008, REQ-NF-004
  - Check: strengths `25`, `55`, and `87` resolve to weak, medium, and strong palette-driven treatments with readable
    text.

- [x] T-014: Rebalance selected-row border and signal spacing
  - REQs: REQ-F-007, REQ-F-009
  - Check: selected border alpha is reduced by 10–15%, the rail/background remain intact, and the signal group gap is
    reduced without overlap.

- [x] T-015: Reduce resting Rescan emphasis
  - REQs: REQ-F-010
  - Check: resting, hover, scanning, and disabled states are visually distinct; activation behavior is unchanged.

- [x] T-016: Brighten action-row chevrons
  - REQs: REQ-F-011
  - Check: both chevrons are easier to see while remaining subordinate to their labels; action signals and hit targets
    are unchanged.

## Phase 5 — QML Tests

- [x] T-017: Extend `tst_WifiNetworkDelegate.qml`
  - REQs: REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009, REQ-NF-006
  - Check: tests cover connected subtitle, representative signal tiers, selection treatment, and retain separator
    coverage.

- [x] T-018: Add current-card formatting tests
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005, REQ-NF-006
  - Check: an offscreen QML test covers band thresholds, link-speed formatting, unavailable fallbacks, and subtitle
    behavior.

- [x] T-019: Add secondary-control regression checks where practical
  - REQs: REQ-F-010, REQ-F-011, REQ-NF-005
  - Check: QML object-state tests confirm Rescan disabled/scanning behavior and action activation signals remain intact.

## Phase 6 — Verification

- [x] T-020: Run narrow network service and QML tests
  - REQs: REQ-NF-006
  - Check: the network GTest target/filter and `test_holonight_qml_harness` pass offscreen.

- [x] T-021: Run QML and generated-type checks
  - REQs: REQ-NF-004
  - Check: `task qml-lint` and `task qmltypes-check` pass.

- [x] T-022: Run formatting and architecture checks
  - REQs: REQ-NF-001, REQ-NF-002
  - Check: `task format-check` passes; `task architecture-check` passes if target-boundary files changed.

- [ ] T-023: Verify live popup states in Hyprland
  - REQs: REQ-F-001..011, REQ-NF-003, REQ-NF-005
  - Check: connected Wi-Fi, wired, disconnected, scanning, and unavailable states render correctly; selected-row
    disconnect, Rescan, actions, dismissal, and popup geometry do not regress.

## Deferred Backlog — Separate Promotion Required

- [ ] D-001: Specify and implement focus-aware keyboard shortcut hints.
- [ ] D-002: Specify and implement expandable connection diagnostics.
- [ ] D-003: Evaluate a smoothed traffic-history visualization before retaining or removing traffic telemetry APIs.
- [ ] D-004: Evaluate connection duration, detailed security protocol, channel, and Wi-Fi generation only with reliable
  backend sources.

## Ordering Rationale

1. T-001 through T-005 establish stable data and QML-facing contracts.
2. T-006 and T-007 lock backend behavior before presentation changes.
3. T-008 through T-011 make the summary card authoritative.
4. T-012 through T-016 apply the remaining focused review polish.
5. T-017 through T-019 protect QML behavior.
6. T-020 through T-023 complete automated and live verification.

Each phase is intended to remain reviewable and buildable on its own.

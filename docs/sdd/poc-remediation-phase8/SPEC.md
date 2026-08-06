# Phase 8 — Correctness Remediation

**Status**: Specification phase (Stage 1 of SDD)

## Objective

Fix the first bounded, high-value correctness tranche seeded by the Phase 7
triage. This phase addresses six Confirmed items with deterministic failure
paths and small implementation scope. It does not attempt the remaining Phase
7 backlog.

| Source | Phase 8 item | Rationale |
|---|---|---|
| U-02 I-08 | Control socket framing and input bound | Local control input may be fragmented or unbounded. |
| U-03 I-01 | Clamp battery percentage | Invalid firmware/UPower values reach notifications. |
| U-07 I-02 | Provider-qualified calendar sync state | Same-named CalDAV and ICS accounts collide. |
| U-02 I-07 | Tooltip screen-local anchor geometry | Secondary-monitor tooltip placement uses a global coordinate. |
| U-11 I-C1 | Settings model range validation | Non-QML callers can persist invalid numeric settings. |
| U-11 I-Q3 | Dirty-state gating for save actions | No-op saves remain enabled despite no pending changes. |

The Phase 7 triage remains the authority for confidence and prioritization:
`docs/sdd/poc-remediation-phase7/REPORT.md` §3.

## Functional Requirements

### REQ-F-01 — Bounded, complete control commands

`ControlServer` shall accumulate each local-socket request until the peer
disconnects, then decode at most one command from the complete payload.

- A fragmented valid command shall produce exactly the same signal as an
  unfragmented command.
- A request larger than a named maximum (4 KiB) shall be rejected without
  dispatching a command and the socket shall be disconnected.
- The existing command syntax and `decodeCommand()` behavior shall remain
  compatible with `holonight-shell --toggle-*` callers.

**Acceptance**: app tests cover a fragmented valid command and an oversized
request; neither can produce duplicate dispatch.

### REQ-F-02 — Valid battery percentage invariant

`BatteryService` shall expose and signal only an integer battery percentage in
the inclusive range 0–100.

- Clamp at the `setPercent()` ingress point so DBus updates and test/direct
  callers share the invariant.
- A clamped value equal to the current value shall not emit `percentChanged`.
- Low-battery notification text shall consequently never contain a negative or
  above-100 percentage.

**Acceptance**: services tests assert -1 becomes 0, 101 becomes 100, and the
low-battery state machine observes the clamped value.

### REQ-F-03 — Provider-qualified calendar transient state

`CalendarSyncManager` shall key `in_progress_` and `backoff_` by the pair
`(provider_type, account_name)`, not by account name alone.

- CalDAV and ICS accounts with the same account name shall sync and back off
  independently.
- Completion and account removal shall modify only the matching provider/account
  transient state.
- Persistent cache keys remain unchanged because they already carry provider
  type separately.

**Acceptance**: a calendar integration test with same-named providers proves a
failure/backoff or removal in one type does not suppress the other.

### REQ-F-04 — Screen-local tooltip positioning

`TooltipSurface` shall convert a global anchor x-coordinate to the selected
screen's local coordinate system before centering and clamping its surface.

- Primary-screen positioning remains unchanged.
- On a screen whose origin x is non-zero, the tooltip is centered relative to
  that screen and bounded by `kScreenEdgeMargin`.

**Acceptance**: a pure geometry test covers primary and offset-screen anchors,
including left and right clamp boundaries.

### REQ-F-05 — Settings edit-model numeric bounds

`SettingsEditModel` shall enforce the same valid ranges as the settings UI at
every numeric setter boundary:

| Property | Allowed range |
|---|---:|
| `uiFontSize` | 8–24 |
| `fixedFontSize` | 8–24 |
| `workspaceCount` | 3–10 |
| `trayMaxItems` | 2–5 |

- Out-of-range inputs shall clamp before becoming current state or being
  serialized.
- In-range inputs retain existing behavior and dirty-state signaling.

**Acceptance**: settings tests call each setter below and above its range and
assert the stored `ParsedConfig` value is bounded.

### REQ-F-06 — Save actions represent pending work

Both **Apply** and **Save & Apply** shall be enabled only when
`editModel.isDirty && !fileService.isSaving`.

Their shared persistence action remains intentional per
`docs/sdd/holonight-settings/SPEC.md` REQ-F-017; this phase shall not invent a
second save/apply behavior.

**Acceptance**: QML inspection/smoke coverage confirms both buttons use the
same dirty-and-saving gate and still invoke `fileService.save()`.

## Constraints and Verification

- Keep changes localized; do not add dependencies or alter public command
  syntax/config-file schema.
- Add or extend deterministic unit/integration tests for every behavioral
  requirement.
- Run `task format-check`, `task tidy`, and `task test`; run `task qml-lint`
  and `task qmltypes-check` because `FooterBar.qml` changes.
- Run `task architecture-check` because `TooltipSurface`/surface test wiring
  changes may touch a surfaces target boundary.
- A live compositor check is required for the secondary-monitor tooltip after
  automated geometry tests pass.

## Out of Scope

All other Phase 7 Confirmed findings, including speculative micro-optimizations
and broader refactors, remain queued for later remediation tranches.

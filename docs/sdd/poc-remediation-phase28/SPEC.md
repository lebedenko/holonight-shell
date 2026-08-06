# Phase 28 — Per-Monitor View Lookup

**Status**: Complete — implementation, automated validation, and user verification passed.

## Objective

Remediate U-02 I-09: avoid scanning every live monitor surface to find the
view that receives an occupancy-driven widget visibility update.

| Source | Phase 28 item | Impact |
|---|---|---|
| U-02 I-09 | Indexed per-monitor view lookup | Widget occupancy updates locate their target surface directly. |

## Functional Requirements

### REQ-F-01 — Resolve live views by monitor name directly

`PerMonitorLayerManager` shall maintain a monitor-name index for its live
surfaces. `WidgetManager` shall use that shared lookup for occupancy updates.

- A lookup for a live monitor shall return that monitor's existing view.
- A lookup for an absent monitor shall return `nullptr` as before.
- The index shall be populated only after a surface is live and removed before
  its corresponding `QScreen` is torn down.

**Acceptance**: build and surfaces tests preserve widget behavior, while the
lookup no longer iterates the full per-monitor surface map on occupancy events.

## Constraints and Verification

- Keep ownership and Wayland-safe destruction order unchanged.
- Keep the optimization in the shared per-monitor lifecycle, without adding a
  second widget-specific cache.
- Run focused surfaces tests, `task test`, `task format-check`, and
  `git diff --check`.

## Out of Scope

- Widget layout, occupancy policy, timer behavior, and monitor naming policy.
- The remaining queued Phase 7 Low-severity candidates.

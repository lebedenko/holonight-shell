# Phase 41 — QML Delegate Contract Hardening

**Status**: Complete — automated checks and user verification passed.

## Objective

Bundle the three remaining U-09 delegate-contract findings from the Phase 7
backlog. This tranche makes network and sidebar delegates declare their model
inputs and opts the affected files into bound component behavior.

| Source | Phase 41 item | Impact |
|---|---|---|
| U-09 I-002 | Declare Wi-Fi and notification delegate inputs | Model-role renames and omissions become QML compile-time contract failures. |
| U-09 I-006 | Align notification preview with notification rules | Both notification surfaces use explicit delegate inputs. |
| U-09 I-007 | Bind Overview and tab delegates lexically | Delegate references to owning components use compiled bound-component semantics. |

## Functional Requirements

### REQ-F-01 — Wi-Fi delegate roles are explicit

`WifiNetworkDelegate` shall declare every model role it consumes as a required
property and shall use bound component behavior. Network row appearance,
selection indexes, password requests, connection actions, and lock repainting
shall remain unchanged.

### REQ-F-02 — Notification preview inputs are explicit

The Overview notification delegate shall require its array element and index.
Its existing `notif` convenience property may remain as a read-only alias of
the required model value. Content, separators, counts, and tab navigation shall
remain unchanged.

### REQ-F-03 — Sidebar delegate components are bound

The Overview calendar, notification, and upcoming-event files plus
`SidebarTabBar` shall opt into `pragma ComponentBehavior: Bound`. Their delegates
shall explicitly declare all consumed model inputs, including calendar-event
roles, JavaScript-array values, and indexes.

## Constraints and Verification

- Do not change model role names, C++ model APIs, displayed content, layout, or
  user interaction.
- Keep the phase within the affected QML delegate files, focused tests, and SDD
  documentation; add no dependencies or new components.
- Run focused sidebar/network QML coverage, `task qml-lint`,
  `task qmltypes-check`, `task test`, `task architecture-check`, and
  `git diff --check`.
- Perform a live Hyprland check of Wi-Fi rows, Overview notifications/calendar,
  upcoming events, and sidebar tabs before closing the phase.

## Out of Scope

- Popup glow ordering, brightness write cadence, weather popup overflow, and
  all U-10/U-11 backlog candidates.

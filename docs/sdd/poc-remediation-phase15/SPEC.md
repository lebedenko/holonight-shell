# Phase 15 — Active Window State Hygiene

**Status**: Complete — implementation and live acceptance passed.

## Objective

Remediate three related, Small Low-severity items from the Phase 7 backlog in
`ActiveWindowService`. The tranche keeps the service's per-monitor state in
sync with Hyprland snapshots, removes an accidental duplicate public API, and
centralizes its duplicated desktop-entry parsing.

| Source | Phase 15 item | Impact |
|---|---|---|
| U-04 I-02 | Prune missing monitor state | A disconnected monitor must not leave stale window/workspace data behind. |
| U-04 I-03 | Remove duplicate focused-monitor getter | The QML-facing API has one canonical focused-monitor accessor. |
| U-04 I-06 | Share desktop-entry parsing | Category resolution and desktop-file matching use one consistent parser. |

## Functional Requirements

### REQ-F-01 — Snapshots reconcile per-monitor state

After applying a complete Hyprland monitor snapshot, `ActiveWindowState` shall
contain window and workspace entries only for monitors represented in that
snapshot.

- Removing a monitor shall emit the existing per-monitor change signals so
  consumers clear stale content.
- The focused monitor shall clear when it is no longer present.
- An incomplete or failed snapshot shall not prune existing state.

**Acceptance**: deterministic state-transition tests add two monitors, apply a
one-monitor snapshot, and verify the removed monitor has no remaining window
or workspace state and its observers are notified.

### REQ-F-02 — Focused-monitor API has one canonical accessor

`focusedMonitorName()` shall remain the public QML/invokable accessor. The
duplicate `focusedMonitor()` method shall be removed or made an internal
compatibility alias only if a repository caller requires it.

**Acceptance**: repository callers compile against the canonical accessor;
the focused-monitor property and its notifications retain current behavior.

### REQ-F-03 — Desktop-entry parsing is shared and behavior-preserving

The service shall use one internal parser for the `[Desktop Entry]` state
machine shared by category lookup and desktop-file matching.

- Preserve first-section handling and first-`=` splitting behavior.
- Preserve current category mapping and `Name`/`Exec` matching semantics.
- Do not add a general-purpose desktop-file framework for this focused change.

**Acceptance**: focused tests cover category extraction, matching a desktop
entry by name/exec, and malformed or irrelevant files.

## Constraints and Verification

- Keep existing QML property names and per-monitor notification contracts
  unless repository usage proves an unused duplicate can be removed safely.
- Do not broaden this phase to monitor-surface ownership or generic desktop
  entry infrastructure.
- Add deterministic C++ tests before live compositor validation.
- Run focused tests, `task test`, `task architecture-check`,
  `task format-check`, and `git diff --check`.
- In a live Hyprland session, disconnect/reconnect a monitor and verify window
  title, category, and workspace consumers do not retain stale monitor data.

## Out of Scope

- The other 58 unscheduled Low-severity Phase 7 candidates.
- NetworkManager polling/error-message work and audio channel-map handling.
- Changes to Hyprland IPC protocol parsing outside the monitor-state and
  desktop-entry paths listed above.

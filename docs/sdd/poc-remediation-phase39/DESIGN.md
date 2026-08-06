# Phase 39 — Notification Rule Model Hardening: Design

## 1. Rule-bound policy

The model owns a compile-time limit of 256 entries. Existing entries are
looked up before the capacity check, so their `last_seen_ms`, desktop-entry ID,
and icon still refresh at capacity. A new name at capacity is ignored. This
keeps established user preferences stable under arbitrary new D-Bus app names
instead of turning the bound into an attacker-controlled eviction policy.

On load, normal desktop-entry pruning runs first. If the result remains
oversized, the model repeatedly removes the smallest `last_seen_ms` value.
Removing in place preserves the relative order of retained rows. The existing
size-change persistence path writes the bounded snapshot without a storage
format migration.

## 2. Role enums

Both notification model role enums become `: std::uint16_t`. Their first value
remains `Qt::UserRole + 1`, so all numeric role values stay identical. This
matches `WorkspaceModel`, `TrayModel`, `CalendarEventModel`, and other Qt model
contracts already used by the project.

## 3. Risks and controls

- A user with 256 established apps cannot auto-discover a 257th. The high
  ceiling makes this exceptional, and preserving existing preferences is safer
  than silent eviction.
- Equal timestamps use deterministic first-oldest removal; retained ordering
  is otherwise stable.
- Tests cover both runtime insertion and startup recovery boundaries.

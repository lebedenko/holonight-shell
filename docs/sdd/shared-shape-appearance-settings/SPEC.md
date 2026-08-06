# Shared Shape Appearance Settings Specification

> **Live-apply amendment:** after a successful settings save, the settings window explicitly reloads both
> `HoloniightPalette` and `HnAppearance`. Color and shape changes therefore become visible in the settings process
> together, without waiting for file-watcher timing.

**Feature:** Edit the shared HoloNight shape appearance from HoloNight Settings
**Status:** Implemented
**Date:** 2026-07-27

## Overview

HoloNight Settings shall become the user-facing owner of
`$XDG_CONFIG_HOME/holonight/appearance.json`. The file is a shared configuration consumed by the
`HnAppearance` singleton from `holonight-qt`, so a saved change applies consistently to
`holonight-shell`, `holonight-ai`, the `holonight-qt` demo, and other HoloNight applications.

The file remains optional. Applications shall continue to use semantic shape defaults when it is
absent. HoloNight Settings shall create or replace it only as part of an explicit successful save;
merely launching Settings or another HoloNight application shall not create it.

## Existing Contracts

- `holonight-qt` owns the JSON schema, validation rules, semantic defaults, file watching, and
  runtime shape resolution.
- HoloNight Settings already edits `$XDG_CONFIG_HOME/holonight/config.toml` and
  `$XDG_CONFIG_HOME/holonight/theme.conf`.
- This feature shall not combine shape configuration with either existing file.
- The shared JSON schema is version 1:

```json
{
  "version": 1,
  "cornerStyle": "inherit",
  "shapeScale": 1.0,
  "baseRadius": null,
  "baseChamfer": null
}
```

## Functional Requirements

### REQ-F-001: Resolve the shared appearance path

HoloNight Settings shall resolve the file through the canonical `holonight-qt` appearance
configuration API rather than duplicating its XDG path logic.

**Acceptance criteria:**

- With no override, the path is
  `$XDG_CONFIG_HOME/holonight/appearance.json`, falling back to
  `~/.config/holonight/appearance.json`.
- `HOLONIGHT_APPEARANCE_FILE` is honored for tests and controlled launches.
- Settings and `HnAppearance` resolve the same path.

### REQ-F-002: Load optional shape preferences

On startup and Discard/reload, Settings shall load the shared file into its edit model.

**Acceptance criteria:**

- A missing file loads `cornerStyle=inherit`, `shapeScale=1.0`, and unset base overrides without
  warning or error UI.
- Valid values are reflected in the controls.
- Invalid or unsupported content uses the normalized safe values produced by the shared
  `holonight-qt` parser.
- Loading does not create or modify the file.

### REQ-F-003: Edit the global corner style

The Appearance page shall expose a shape-style selector for `inherit`, `hybrid`, `rounded`, and
`chamfered`.

**Acceptance criteria:**

- The current normalized value is visibly selected.
- Changing the selection updates only the in-memory edit model until Save & Apply.
- The labels explain that the setting affects all HoloNight applications.

### REQ-F-004: Edit the global shape scale

The Appearance page shall expose `shapeScale` over the canonical inclusive range `0.25` to `4.0`.

**Acceptance criteria:**

- The control displays the current numeric value.
- Values are clamped or rejected consistently with `holonight-qt`.
- A value of `1.0` represents the semantic default token scale.

### REQ-F-005: Edit optional advanced base overrides

The Appearance page shall expose optional `baseRadius` and `baseChamfer` controls as advanced
settings.

**Acceptance criteria:**

- Each override can independently be enabled or unset.
- Enabled values accept the canonical inclusive range `0` to `128`.
- Unset values serialize as absent or JSON `null` and are represented in C++ without an arbitrary
  sentinel value.
- The UI explains that an enabled base override flattens the corresponding semantic size family.

### REQ-F-006: Track shape edits in the existing dirty-state workflow

Shared shape values shall participate in the same snapshot and dirty-state behavior as the other
Appearance settings.

**Acceptance criteria:**

- Changing any shape value makes `SettingsEditModel::isDirty` true.
- Restoring all values to their snapshot clears dirty state.
- Discard reloads all three configuration sources and restores their persisted/default values.
- A successful save updates all snapshots and clears dirty state.
- A failed save preserves dirty state.

### REQ-F-007: Persist atomically on explicit save

Save & Apply shall persist the normalized shape configuration using an atomic replacement.

**Acceptance criteria:**

- The parent directory is created when needed.
- JSON is valid UTF-8, has `version: 1`, and is accepted by `Holonight::AppearanceConfig::load()`.
- Existing files are replaced atomically so watchers never observe partial JSON.
- A missing file is created only during an explicit save.
- Write, flush, or commit failure produces a user-visible save error and does not mark the model
  saved.

### REQ-F-008: Preserve cross-file save consistency

Saving `config.toml`, `theme.conf`, and `appearance.json` shall report success only when all required
writes succeed.

**Acceptance criteria:**

- `saveFinished(true)` is emitted only after all three files are successfully committed.
- Failure identifies the path that could not be written.
- The design stage shall explicitly choose and document the rollback or partial-write policy for
  failures occurring after an earlier file has committed.

### REQ-F-009: Apply changes without application-specific signaling

After a successful write, running HoloNight applications shall observe the change through
`HnAppearance` file/directory watching.

**Acceptance criteria:**

- No new D-Bus service or application-specific IPC is introduced.
- Atomic replacement triggers a reload in consumers already running.
- Settings itself reflects the saved normalized values.

### REQ-F-010: Test observable behavior

Automated tests shall cover the shared file lifecycle and model integration.

**Acceptance criteria:**

- Tests use temporary paths through `HOLONIGHT_APPEARANCE_FILE` and do not touch the user's real
  configuration.
- Coverage includes missing-file load without creation, valid load, dirty tracking, range handling,
  optional override handling, successful atomic save, malformed input fallback, and write failure.
- A round-trip test writes through Settings and reads through
  `Holonight::AppearanceConfig::load()`.
- QML coverage verifies the controls bind to the edit model and can change each exposed value.

## Non-Functional Requirements

### REQ-NF-001: Single schema authority

Parsing, validation limits, enums, and defaults shall remain owned by `holonight-qt`. Settings may
add a writer adapter, but it shall use the public `holonight-qt` value/API and shall not introduce a
second independent appearance schema.

### REQ-NF-002: Backward compatibility

Existing `config.toml` and `theme.conf` behavior shall remain unchanged. Users without
`appearance.json` shall retain current rendering defaults.

### REQ-NF-003: Focused architecture

The implementation shall extend the existing `SettingsEditModel`, `ConfigFileService`, and
Appearance page. It shall not introduce a new framework, settings daemon, or broad configuration
abstraction.

### REQ-NF-004: Verification

Implementation verification shall include focused Settings C++ tests, Settings QML tests,
`task qml-lint`, `task qmltypes-check`, and the relevant `holonight-qt` appearance tests.

## Out of Scope

- Editing color schemes or accents in `appearance.json`; those remain in `theme.conf`.
- Editing font settings in `appearance.json`; those remain in `config.toml`.
- Per-application shape overrides or profiles.
- Automatic file creation at startup.
- D-Bus appearance-change notifications.
- Migrating or deleting legacy palette radius properties.
- Changing the version-1 JSON schema.

## Open Design Decisions

The design stage shall resolve:

1. Whether serialization belongs in a new public `holonight-qt` writer API or a Settings-owned
   adapter built on the public `AppearanceConfig` value.
2. How the edit model represents optional numeric overrides in a QML-friendly way without losing
   the distinction between zero and unset.
3. The multi-file failure policy required by REQ-F-008.
4. Whether saving all-default shape values should write an explicit file or remove an existing
   override file; deletion shall not be selected without explicit product justification because it
   changes recovery and failure semantics.

## Pipeline Status

- Requirements: complete
- Design: complete
- Task breakdown: complete
- Implementation: complete

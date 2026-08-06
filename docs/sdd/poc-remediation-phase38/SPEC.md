# Phase 38 — Desktop Entry Field Mapping Consistency

**Status**: Complete — automated checks and user verification passed.

## Objective

Remediate U-06 I-09 from the Phase 7 backlog by making the scalar text-field
mapping of `DesktopEntry` a single maintained contract. The scanner and JSON
cache serializer currently enumerate the same fields independently; the
`startup_wm_class` addition demonstrated the resulting multi-site update risk.

| Source | Phase 38 item | Impact |
|---|---|---|
| U-06 I-09 | Consolidate `DesktopEntry` scalar field mappings | Adding or renaming a supported scalar desktop-entry field has one authoritative mapping definition, while parse and cache behavior remain unchanged. |

## Functional Requirements

### REQ-F-01 — Define one scalar-field mapping contract

The launcher shall define the supported scalar `QString` fields of
`DesktopEntry` and their desktop-file and JSON names in one authoritative,
compile-time-readable mapping.

- The mapping covers the fields currently handled in all three paths:
  `name`, `generic_name`, `comment`, `exec`, `icon`, `categories`, `path`,
  `desktop_file`, and `startup_wm_class`.
- A field that has no desktop-file key (currently `desktop_file`, supplied by
  the scanned file path) remains represented without inventing an XDG key.
- `Type`, `Hidden`, `NoDisplay`, `Terminal`, actions, and MIME types retain
  their dedicated parsing/serialization rules and are not folded into the
  scalar-text mapping.

**Acceptance**: scanner assignment and both JSON directions derive their
scalar-text field handling from the same mapping; adding a mapped field does
not require parallel hand-maintained lists in those paths.

### REQ-F-02 — Preserve scanner and cache semantics

The consolidation shall not change observable desktop-entry parsing or cache
serialization behavior.

- Existing XDG keys continue to populate the same `DesktopEntry` members.
- JSON keys, mandatory-field validation (`name`, `exec`, and `desktop_file`),
  default handling, compact cache payloads, and cache schema version remain
  unchanged.
- The special scanner assignment of `desktop_file` from the input path remains
  intact.
- Existing action and MIME list filtering, boolean parsing, and launchability
  checks remain intact.

**Acceptance**: current scanner and serializer tests retain their observed
values; a complete scalar-field round trip and a cache reopen retain every
mapped value.

## Constraints and Verification

- Keep changes in the launcher desktop-entry representation, scanner,
  serializer, and focused launcher tests. Do not alter launcher ranking,
  selection, QML roles, cache SQL schema, or cache recovery policy.
- Prefer a small compile-time descriptor/list over runtime reflection,
  metatype registration, or a new dependency.
- Preserve the existing cache schema version because the JSON key/value shape
  is unchanged.
- Run focused launcher tests, `task test`, `task architecture-check`,
  `task format-check`, changed-file `clang-format --dry-run --Werror`, and
  `git diff --check`.

## Out of Scope

- Notification-rule limits and role-enum underlying types (U-06 I-05/I-06).
- Launcher selection, search/ranking, cache SQL/query lifecycle, or schema
  migration behavior.
- Weather, portal, tray, QML, and every other queued Phase 7 candidate.

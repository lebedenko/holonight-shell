# Phase 33 — Portal Color-Scheme Protocol Constants

**Status**: Complete — automated checks and user verification passed.

## Objective

Remediate U-05 I-08: replace bare freedesktop Settings portal `color-scheme`
protocol integers with names that communicate the dark and light values while
preserving the D-Bus contract.

| Source | Phase 33 item | Impact |
|---|---|---|
| U-05 I-08 | Name Settings portal color-scheme values | Theme-to-portal mapping and its default state are understandable without relying on unexplained `1` and `2` literals. |

## Functional Requirements

### REQ-F-01 — Name the protocol values at their definition site

`SettingsPortalBackend` shall define named values for the freedesktop portal
color-scheme dark (`1`) and light (`2`) enumerators.

- The default `Values::color_scheme` value and every result from
  `colorSchemeForThemeConfig()` use those names rather than bare protocol
  integers.
- The values emitted from `Read`, `ReadAll`, and `SettingChanged` remain the
  same unsigned D-Bus values as before.
- Theme scheme resolution and the fallback `appearance/mode` behavior remain
  unchanged.

**Acceptance**: a reader can identify the protocol meaning of each
color-scheme value from the source, and dark/light configuration continues to
publish `1`/`2` respectively.

## Constraints and Verification

- Keep the protocol constants local to `SettingsPortalBackend`; do not change
  the QML-facing `PortalService::colorScheme` API or introduce a new shared
  public enum.
- Do not alter portal service registration, D-Bus signatures, theme resolution,
  or accent-color behavior.
- Extend or retain focused Settings portal backend coverage for both published
  color-scheme values.
- Run focused portal tests, `task test`, `task architecture-check`,
  `task format-check`, and `git diff --check`.

## Out of Scope

- Reinterpreting the freedesktop portal specification or adding an automatic
  color-scheme state.
- Portal client-adapter naming, D-Bus timeout, MIME-cache, or other queued
  Phase 7 remediations.
- The remaining queued Phase 7 Low-severity candidates.

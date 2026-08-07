# SPEC: Appearance Configuration and Shell Config Ownership

**Initiative:** ACF-006
**Date:** 2026-08-07
**Status:** In Progress
**Repository baseline:** `4fe75168632c43476ed862c70c63b95f63e4f292`
**Umbrella contract:** `623129188f7deeebf943857f0c4b3691c92d63c0`
**Appearance provider:** `holonight-config` `81b01d3ae8433f3a4b017db2feb588a1ee62b714`
**Qt provider:** `holonight-qt` `6f591cbdfb8c8e887e87e43a1c4e0c48c1f5f39d`

## Goal

Make Shell own its product configuration and consume global appearance as a separate read-only domain. Remove the
current dependency on a Shell schema exported from the Settings repository, eliminate appearance fields from
`config.toml`, and project only validated in-memory canonical appearance into Shell QML and the Settings portal.

This package must precede Settings adoption: Settings edits Shell product configuration, so it consumes a schema
published by Shell rather than defining Shell's schema itself.

## Shell product configuration ownership

- Move the schema, defaults, validation, TOML parsing/writing, and test helpers for
  `$XDG_CONFIG_HOME/holonight/config.toml` into this repository.
- Provide a narrowly scoped, independently buildable and installable library under
  `libs/holonight-shell-config/`. It must be buildable and testable without Wayland, compositor, QML, or the Shell
  executable.
- Export it as package `HoloNightShellConfig` and target `HoloNightShellConfig::Config`, with public headers under
  `holonight_shell_config/`.
- Qt Core and toml++ are acceptable implementation/public dependencies where required by the existing product schema;
  no Shell service or UI dependency is allowed.
- `ConfigService` consumes this in-repository target. HoloNight Settings consumes the installed package after ACF-006
  is published.

The product schema retains workspace, tray, background, notification/history, widget, calendar, weather, logo, OSD,
and other Shell-owned behavior. It contains no global appearance or theme selection.

Remove from the product schema, parser, missing-default tracking, writer, tests, examples, and documentation:

- the `[appearance]` table and its typography, transparency, and blur fields;
- any legacy `[theme]` table or derived theme mode;
- any API named `AppearanceConfig` that actually describes Shell TOML content.

Existing legacy tables remain untouched in user files but are inert. A product-config write may omit them in its
new canonical output; no appearance migration or fallback is provided.

## Canonical appearance consumption

- Consume `HoloNight::Config` from `holonight-config` for path resolution, defaults, parsing, validation, and
  diagnostics.
- Shell is read-only. It never calls the provider's write operation in production.
- Missing startup appearance uses shared defaults without creating a file.
- Present invalid startup appearance uses shared defaults and emits a redacted diagnostic.
- Watch both the canonical file and parent directory. Parse and validate a complete candidate before publication.
- Invalid live replacement retains last-known-good appearance and emits no semantic change signals.
- Atomic replacement, late creation, watcher rearming, and semantically unchanged reloads follow the provider/Qt
  contracts.
- `HOLONIGHT_APPEARANCE_FILE` is the only supported appearance override.

`AppearanceService` becomes the single Shell-owned canonical appearance state and watcher. It exposes canonical UI,
monospace, title, and display families/sizes, icon/fallback/cursor choices, layout scale, shape choices, scheme,
accent, derived color mode, and a revision as needed by Shell QML.

`ConfigService` no longer exposes `appearance()`, emits `appearanceChanged`, or watches appearance through
`config.toml`. Debug overlays are Shell diagnostic behavior and must be sourced from an explicitly named Shell option
or test/developer environment flag, not smuggled into global appearance.

## Theme and portal projection

- Remove `ThemeService`'s independent `theme.conf` path resolver, parser, and watcher.
- Consolidate its reload coordination into `AppearanceService`, or reduce it to a projection subscribed to that
  service; there must be only one canonical appearance reader in the Shell process.
- `SettingsPortalBackend` accepts resolved values from `AppearanceService`. It never reads any configuration file.
- `org.freedesktop.appearance color-scheme` is derived from the selected catalog scheme; dark/light is not persisted.
- `org.freedesktop.appearance accent-color` is derived through the Qt theme catalog/resolver at the exact pinned Qt
  provider contract.
- Portal `SettingChanged` is emitted only when the corresponding resolved system-facing value changes, and before
  Shell publishes the related in-process appearance revision.
- Existing QML palette-reload behavior is driven by the accepted appearance revision rather than a legacy file event.

## Transaction and security boundary

Global appearance and Shell product configuration are separate documents, stores, dirty domains, and failure
results. An appearance-only action in Settings must never parse or write `config.toml`; Shell never couples their
reloads.

`config.toml` currently includes credential and private-URL fields. Moving those values to a secret store affects
service provisioning, user migration, and Settings UX, so it is assigned to the separate Shell-owned
[Shell Credential Storage](../shell-credential-storage/README.md) initiative rather than expanded into ACF. Until
that initiative lands:

- ACF documentation and tests use redacted fixtures only.
- Diagnostics never include credential values or full authenticated URLs.
- Appearance APIs never accept or expose product configuration values.
- Product-config writes occur only for explicit Shell-settings changes, never as a side effect of appearance changes.

## Clean-break requirements

After ACF-006, Shell does not depend on `HolonightConfig::Config` from the Settings repository and does not read,
write, watch, document, or test appearance from `config.toml` or `theme.conf`. No copied schema, dual reader,
field-level environment override, KDE selection fallback, or portal-side parser remains.

## Verification

- Build and test `libs/holonight-shell-config` standalone; install it and compile a minimal external consumer.
- Test that the Shell product schema contains every retained field and no appearance/theme fields.
- Test product-config parsing/writing with redacted fixtures and verify unknown legacy appearance tables are inert.
- Test canonical appearance defaults, valid startup, invalid startup, live replacement, invalid-reload rollback, late
  creation, watcher rearming, and unchanged reload behavior.
- Test precise AppearanceService signals and QML typography/icon/layout/shape projections.
- Test portal initial values and change ordering from injected in-memory appearance without filesystem access.
- Search production, tests, docs, and CMake for the removed Settings package, `theme.conf`, appearance-in-product
  fields, legacy watchers, and duplicate path/parser logic.
- Run format, tidy, architecture, QML lint/types, full CTest, and manual Hyprland appearance reload checks.

## Non-goals

- Writing global appearance or implementing Settings UI.
- Moving global appearance into Shell product configuration.
- Cross-toolkit adapters.
- Credential/keyring migration itself; that is a separately tracked Shell security initiative.

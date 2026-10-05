# Configuration flow

Shell product behavior lives in `$XDG_CONFIG_HOME/holonight/config.toml`; global appearance lives in the separate
`appearance.toml`. Both fall back below `~/.config`. Shell owns the exported `HoloNightShellConfig::Config` package
under `libs/holonight-shell-config/`. It declares ProductConfig, sparse defaults, metadata and validation. Settings
is an independent consumer of that installed package. Toolkit-neutral documents/editing live in HoloNight Config,
and reusable Qt watching lives in HoloNight Qt; neither requires Shell, Settings or a running daemon.

ConfigService reads on startup and through the Qt DocumentWatcher with a 200 ms debounce. The watcher follows file
replacement, deletion and recreation by watching the nearest existing parent. Reads do not create directories or
files and never backfill defaults. Missing overrides use typed defaults. Known invalid values, wrong section types
and invalid dynamic collections reject the document. Invalid reloads retain the last valid configuration; startup
errors use defaults with diagnostics. Missing documents use defaults without diagnostics. Effective-value signals
are emitted only when their values change. Logo changes retain their existing restart requirement, recorded in
the schema metadata. Unknown fields, including legacy theme/appearance fields, are inert.

Removing an assignment resets an override. `[[widget]]` remains an opt-in dynamic collection; an absent collection
means no desktop widgets. Disabled countdowns can retain incomplete title/deadline drafts. Known field types,
positions, ranges and widget types are validated instead of silently skipping or repairing entries.

The public Result-based `decodeDocument()` reports validation diagnostics. The legacy `parseConfigTable()` API
throws `std::invalid_argument` for invalid known values. Explicit full serialization and missing-default migration
APIs remain available, but are not called by readers and are unsuitable for preservation-oriented interactive saves.

The accepted [configuration architecture SDD](sdd/configuration-architecture/README.md) replaces earlier self-healing
and whole-file editing designs. Its Settings work package adopts baseline/pending patches, preserving saves,
per-value conflicts and guarded appearance rollback. That dependent migration is coordinated by the umbrella
initiative; publishing the Shell schema alone does not claim that Settings has adopted it. Additional application
settings surfaces, including a Files page, are outside this phase.

Window/page activation is separate: Shell may call `org.freedesktop.Application.ActivateAction` on
`org.holonight.Settings`; configuration values are not carried over D-Bus. Files and Viewer remain standalone
applications with their own preferences and no Shell or Settings runtime dependency.

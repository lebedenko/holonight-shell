# Config flow: holonight-shell ↔ holonight-settings

`holonight-shell` and `holonight-settings` are separate applications, in separate git
repositories, with separate build/install steps. Configuration exchange uses one shared artifact:
`$XDG_CONFIG_HOME/holonight/config.toml` (default `~/.config/holonight/config.toml`). Window/page
activation is separate from configuration exchange: the shell asynchronously calls
`org.freedesktop.Application.ActivateAction("audio", [], {})` on `org.holonight.Settings` at
`/org/holonight/Settings` when the audio popup's settings button is activated. No configuration
values are carried over D-Bus.

Both link the same `holonight_config` static library for the TOML schema
(`config_structs.h`/`ParsedConfig`), the parser (`ConfigParsers.cpp`), and the writer
(`ConfigWriter.cpp`). That library's canonical source lives in the `holonight-settings` repo, at
`libs/holonight-config/`, and is installed system-/user-wide via `find_package(HolonightConfig
CONFIG REQUIRED)` (installed under `/usr/include`, `/usr/lib/cmake/HolonightConfig`,
`~/.local/include`, `~/.local/lib/cmake/HolonightConfig` depending on install prefix).
`holonight-shell` has no local copy of the schema and cannot change it without editing and
reinstalling that sibling repo first.

## holonight-shell: reader, with narrow self-healing writes

`ConfigService` (`libs/holonight-core/src/ConfigService.cpp`):

- On construction, if `config.toml` doesn't exist at all, it bootstraps one by writing a fully
  default-valued `ParsedConfig{}` via `ConfigWriter::write`.
- Parses the file on startup, and again on every change reported by a `QFileSystemWatcher`
  (200 ms debounce).
- After parsing, any *scalar* key absent from the file (tracked field-by-field in the
  `MissingDefaults` struct — e.g. `appearance.clock_font`) is written back into the file with its
  default value via `writeMissingDefaults`. This is how new scalar settings "appear" in existing
  users' files after an upgrade.
- Invalid *present* values are corrected in memory only (never rewritten to disk): type errors and
  out-of-range values fall back to defaults or get clamped, logged through `holonight.config`.
- Never touches the file for any other reason. It does not run on `holonight-settings`' behalf and
  has no way to signal it.

## holonight-settings: whole-file writer, no re-read on save

`ConfigFileService` (`holonight-settings/apps/settings/src/ConfigFileService.cpp`):

- `load()` parses `config.toml` (if present) into a `ParsedConfig`, then copies it wholesale into
  `SettingsEditModel::current_` (a `ParsedConfig` member) — this happens once, typically at app
  start.
- `save()` calls `model_->toParsedConfig()` — which mutates a copy of `current_` with only the
  fields the UI actually edited (theme, fonts, workspace count, etc.) — and writes the *entire*
  result back with `ConfigWriter::write`. It does **not** re-read the file from disk first.
- Sections the settings UI has no editor for (see below) ride along unmodified in `current_` and
  are re-serialized as-is. This is why editing, say, Appearance and saving does not delete an
  existing `[[widget]]` block — confirmed by
  `holonight-settings/tests/test_settings_app.cpp::SavingUnrelatedSettingsPreservesDisabledTimeToEventWidget`.
  It is a side effect of copying the whole struct, not an explicit preserve/merge step.

## Widgets specifically: opt-in, not defaulted

`[[widget]]` entries (`WidgetsConfig.definitions`) are parsed as a TOML array-of-tables
(`parseWidgets` in `ConfigParsers.cpp`), not as scalar keys. They are **not** covered by
`MissingDefaults`/the self-healing backfill described above. Consequently:

- A `config.toml` with no `[widgets]`/`[[widget]]` section at all parses to an **empty**
  `WidgetsConfig.definitions` list — no widget exists, not "a widget with default settings."
- The only way a widget entry comes into existence is if something writes the
  `[[widget]] type = "..."` block: today, that means hand-editing the file. Both existing widget
  types (`clock`, `time-to-event`) work this way already; there is no special-casing for a new
  type.
- **`holonight-settings` has no UI for widgets at all.** Its nav panel
  (`apps/settings/qml/NavPanel.qml`) has no "Widgets"/"Desktop" entry, and no `*.qml` page in that
  app references `Widget` in any form. The only place `Widget` appears in that repo outside the
  config library itself is the one preserve-on-save regression test above. The config library's
  read/write support for widgets is fully implemented; the GUI layer to author or edit a widget
  entry simply was never built.

## Known gaps (out of scope for this cycle — tracked here for a future one)

1. **Inconsistent self-healing.** Scalar config gets auto-backfilled with defaults on upgrade;
   array-of-table sections (`[[widget]]`) do not. A user who wants a widget must always hand-edit
   TOML, with no discoverability path (no default example is (re-)injected, no settings-UI
   affordance, no first-run prompt).
2. **No widget authoring UI**, despite full backend (schema + parse + write + round-trip
   preservation) already existing in `holonight-config`. Adding a new widget *type* to the schema
   does not, by itself, make it reachable from the GUI — someone has to build a NavPanel page for
   it separately.
3. **Last-write-wins with no re-read/merge in `holonight-settings`.** `ConfigFileService::save()`
   writes from a `current_` snapshot taken at `load()` time and never refreshed before saving. If
   `holonight-shell` performs a self-healing backfill write (or another process edits the file)
   while the settings window is open, that change is silently overwritten by the stale in-memory
   snapshot on the next Save — including sections the settings UI never touched, like `[[widget]]`
   blocks, if they were the specific thing that changed externally in that window.
4. **No cross-repo schema version check.** `holonight-shell`'s build resolves whatever
   `HolonightConfig` package is currently installed via `find_package`; nothing enforces that it
   was built from a compatible/expected version of the `holonight-settings` schema. A stale
   installed package silently exposes an outdated struct shape to `holonight-shell` with no build
   or runtime warning.

# Configuration architecture: holonight-shell

Work package: CA-003. Upstream baseline: `589ddc4023766ade9d95bdbc04bdfca502c0cdd0`.

## Scope

Shell-owned schema and validated sparse reads.

Follow the accepted [shared contract](../../../../docs/initiatives/configuration-architecture/README.md).
Keep this repository independently buildable; do not modify another repository in its implementation commit.

## Design and acceptance

Each application owns its configuration schema, file, settings UI and behavior. Global appearance is shared;
application preferences are separate. Files and Viewer must remain usable without Shell or Settings. AI and Packages
retain their own configuration. Infrastructure requires neither Shell, Settings nor a running daemon.

Snapshots retain original bytes, parsed typed values, override presence, source spans and content revisions.
Paths are vectors of key segments, including quoted keys containing dots. Schemas declare typed defaults,
constraints, descriptions and reload policy, with domain validators for related values and dynamic collections.
Edit batches carry set/remove operations and baseline values/presence. Save outcomes distinguish success,
per-key conflicts, invalid documents/edits, unsupported patches, pre-replacement storage failures and
post-replacement durability failures.

Use toml++ and a TOML-aware lexical editor; never serialize an existing document wholesale or substitute by regex.
Preserve unrelated bytes, comments, ordering, whitespace and unknown fields. Reset removes an assignment and retains
comments/sections. Arrays and arrays of tables are whole conflict values. Unsupported safe patches fail unchanged.
Reparse and validate every candidate. Establish preservation fixtures before consumer adoption.

Merge baseline, pending and current values: unrelated edits merge, identical edits converge, different changes to the
same value conflict. Lock a stable sibling file for cooperating writers; read/patch under the lock and recheck the
revision immediately before replacement. Arbitrary editors do not participate in the lock: a race remains between
the final check and rename. Follow existing symlinks, abort on retargeting, preserve existing permissions, create new
files as 0600, sync a same-directory temporary file, rename, and sync the directory.

Appearance v2 uses sparse defaults, rejects invalid known fields and warns about preserved unknown fields. Retain the
v1 decoder and explicit v1 serialization APIs. Document version is metadata, separate from the effective appearance
model. First successful Settings save upgrades valid v1 by changing only version and requested values. New editing
documents use v2; unsupported versions are read-only. Enable GUI v2 writes only after readers/adapters pass compatibility.

Shell owns defaults/validation in its exported configuration package. Preserve paths and meanings. Reads never create
files or write defaults. Missing overrides use defaults; reset removes the override. Reject invalid known values.

Settings retains Save/Discard, tracks baseline/pending edits, refreshes untouched controls on external changes and
retains pending edits. Expose baseline/disk/pending values and per-value keep-pending/accept-external resolution;
recheck on save. Show default/override status and diagnostics. Discard loads latest disk. Invalid external documents
block saves and running consumers retain last valid values; startup errors use defaults with diagnostics. Missing
files use defaults without writes. Watch files and nearest existing parents through replacement/deletion/recreation;
publish only differing effective values. Domain saves have independent outcomes. Rollback is conditional on the staged
revision still being current; concurrent changes survive and must not be reported successfully applied.

- [ ] Preservation fixtures cover comments, unknown fields, quoted/dotted keys, inline tables, multiline strings, Unicode, CRLF, arrays/AoT, insertion/reset and rejected patches.
- [ ] Merge, convergence, conflicts/reset, cooperating locks and revision-change aborts pass.
- [ ] Unreadable files, permissions, interrupted writes, replacement failures, symlink retargeting and durability outcomes pass.
- [ ] v1 behavior remains compatible; sparse v2/reset and surgical first-save upgrades pass; unsupported versions cannot be overwritten.
- [ ] Runtime invalid/startup/missing/delete/recreate and unchanged-signal scenarios pass.
- [ ] Settings Save/Discard, external updates, per-value resolution, partial saves and adapter/rollback concurrency pass.
- [ ] Each repository passes required clean acceptance and installed-package checks at accepted provider revisions.
- [ ] Files and Viewer pass standalone checks without Shell or Settings installed.
- [ ] Every participating submodule is clean and pinned to a canonical published implementation commit.
- [ ] Dependency-order integration and user-operated concurrent-edit/appearance checks are recorded with dates and revisions.

## CA-003 implementation

The exported ShellConfig package declares `SettingMetadata` with key-segment paths, typed defaults, numeric bounds,
choices, descriptions and reload policy. `documentSchema()` uses those declarations for neutral edit validation;
`decodeDocument()` preserves the existing ProductConfig model. Known values and section types are validated before
parsing. Dynamic tray rules require an icon and selector; calendar accounts require their existing identifying
fields; widget types, field types, positions and ranges are validated. Disabled countdowns may retain incomplete
title/deadline drafts, preserving that existing product meaning. Unknown fields, including legacy appearance/theme
sections, remain inert and preserved.

`parseConfigTable()` now throws `std::invalid_argument` on invalid known values instead of silently repairing them.
The Result-based document API returns diagnostics. Legacy full serialization and explicit missing-default migration
remain available for existing explicit callers, but no Shell read path creates directories/files or writes defaults.
ConfigService uses the published Qt DocumentWatcher, publishes diagnostics, keeps the last valid configuration after
invalid reloads, and uses typed defaults for missing documents. Existing effective-value signals remain conditional
on changes. ShellConfig can be installed independently of the Shell executable and Settings.

The clean acceptance lane uses Config `7e83cacde8911452741420a29abbfe4465b7d52b`, Qt
`1067d0a717f0b24eaa399693a855d687d30e1f71` and System Services `398804a7cce5a57f9f6870c4e7ec99e9b1f3ddaa`.
System Services builds before Qt because Qt requires its compositor package. The isolated installed ShellConfig
consumer now exercises document decoding and metadata rather than only instantiating a struct.

Verification (2026-10-05 UTC / 2026-10-06 local): 59 focused tests passed; required `task test` passed all
1,152 CTest registrations with four general-run private-bus cases skipped and the dedicated private-bus test passing.
The initial sandbox run could not bind its private D-Bus socket; the approved isolated rerun passed. Format,
architecture boundaries and focused source/test clang-tidy passed. Clean build-test passed the same suite and
qmllint; complete build log scanned with no actionable warnings. Full clean static/licensing evidence follows.

The additional Config staging API at published `b705ccb2bc7ab87c3bf9706ec42bb5c97dd4a5eb` is additive and preserves
existing result layouts/read symbols. The workspace provider was rebuilt/installed at that exact revision, with
Qt `1067d0a717f0b24eaa399693a855d687d30e1f71` and System Services `398804a7cce5a57f9f6870c4e7ec99e9b1f3ddaa`;
provider-state metadata confirms clean sources. The already built Shell consumer then passed all 59 focused reader
and schema tests against the corrected shared library, checking binary compatibility as well as behavior.

### Acceptance prerequisites

Full host analysis and clean static acceptance exposed existing diagnostics in launcher execution, native-launch callers, popup geometry and test fixtures. Correct them in a separate Shell commit, without disabling checks: name internal members, initialize D-Bus aggregates, retain exact backend behavior in smaller helpers, add braces and explicit precedence, and align fixtures with repository analysis rules. Retest affected native/private-bus launch and popup paths. The failed static snapshot is `build/ci/20261005T212130Z-9xhe39wx/` (2787 complete log lines reviewed). Final acceptance uses Config `733781607124fc9bec0820c880e7467d08b34a50` and Qt `98803bca05e16ae0d0784a6cb43b0ace561385de`.

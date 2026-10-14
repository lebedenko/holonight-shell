# External dependency management — holonight-shell

Work package: EDM-012
Baseline: `27567a918aae67ad43dba0e813b3f40ed14e539b`
Umbrella audit: `37dd9fe` (Ready checkpoint `8f6ff94`; canonical tidy correction `d7c8104`)
Status: Done; verified local implementation handoff.

## Requirements

- Configuration and normal build commands shall never acquire dependencies or prepare source providers.
- `task deps` shall explicitly prepare supplied local providers and check installed external prerequisites.
- Internal providers shall resolve only from approved prefixes; external packages shall resolve from an explicit
  external prefix or normal system locations.
- Enabled dependency-dependent features shall require their dependencies; minimal profiles shall disable them explicitly.
- Where production components use Qt, discovery shall require >=6.11; private Qt components shall match
  the selected public version exactly.
- Where tests use GTest, consumers shall require installed GTest >=1.14 CONFIG and any GMock targets they use.
- Existing public APIs, component switches, install-tree consumers and acceptance coverage shall be preserved.

## Design

Adopt canonical `tooling/modules/holonight-shell.json` and `.cmake`, then synchronize bundle 2.0.0 into this repository.
The copied metadata is owned by this standalone checkout. CMake performs authoritative discovery; shared probes
provide prerequisite diagnostics and record resolved versions/paths. Incompatible caches and local provider install
trees are archived before reuse. Package acquisition occurs only in explicit environment preparation. CI switches
build/runtime/release lanes together to qualified immutable images and runs acceptance offline after preparation.

Remove the GTest download fallback; require GMock. Require WaylandClient and matching WaylandClientPrivate directly. Preserve configuration-package exports consumed by Settings and use the qualified full Arch image for release acceptance; Debian qualification is superseded.

## Implementation and verification tasks

- [x] Confirm the exact baseline and canonical handoff before implementation.
- [x] Adopt declarations, prefix helpers, explicit preparation and compatibility signatures.
- [x] Apply the repository-specific discovery changes above.
- [x] Update Task, CI preparation/entrypoints, Dockerfiles, release lanes and contributor instructions.
- [x] Run declaration/CMake/profile consistency checks and offline missing-package regressions.
- [x] Run narrow regressions, clean acceptance, installed-package consumers and required quality checks.
- [x] Inspect complete logs; record commands, versions, features, warnings and manual checks.
- [x] Make a repository-local commit only after local verification; publish only when authorized.

Local automated acceptance passed. Exact revisions, options and log hashes are recorded in VERIFICATION.json.

## Accepted handoff and implementation decisions

The Ready handoff is umbrella `8f6ff94`, from the accepted `37dd9fe` integration state. Canonical
corrections `d7c8104` and `a704462` preserve Shell's existing explicit/scoped tidy contexts and the
empty-brace policy workaround. These corrections are separate umbrella commits; no gitlink changes.

Consumer preparation order is Config, System Services, then Qt. Exact provider revisions remain
those published at the accepted umbrella pins. Providers use Debug, tests disabled; Qt Wayland and
System Services compositor Wayland enabled, with default Audio/Storage/Compositor components.

The Portal launcher pattern was adapted while retaining Shell's privileged build-test container.
Preparation alone may acquire exact provider sources/images. Every acceptance container uses
`--pull=never --network=none`. Consumers mount only the installed prefix and revision evidence;
provider sources and provider build trees are unavailable. Manifest identity and content/mode hashes
reject stale or tampered preparation; incompatible preparation is archived, with compatible provider
artifacts reused only after source/toolchain/options validation by the canonical workflow.

The standalone config library uses approved internal provider discovery and Qt >=6.11; its installed
package has the same minimum. `HoloNightShellConfig::Config`, headers, version and package layout
are preserved. Debug and static lanes build/install/consume the standalone export separately.
Release staging validates the shell executable and configuration-package payload under a disposable DESTDIR.
Tag release keeps source-tarball publication unchanged, gated by the same four offline lanes.

Quality cleanup is separate from adoption: export authentication model metatypes from the owning
target and list its QObject headers explicitly; require model properties in generated metadata;
avoid feeding app metatypes twice into the shell registrar; use ABI-equivalent `quint16` for notification
roles so the Qt registrar recognizes the enum type. Enum values and application logic are unchanged.

## Focused verification — 2026-10-14

- `python3 -m unittest discover -s tooling -p 'test_*.py'`: 32 canonical fixtures pass, including
  provider content/toolchain/options compatibility and incompatible artifact archival.
- `python3 scripts/ci/test_launcher.py`: 12 pass, including input snapshots, exact revisions,
  missing/stale/tampered preparation, permissions/symlinks, archival, failures, offline argv and hidden
  sources; licensing remains independent of preparation.
- `python3 scripts/ci/test_tooling_tidy.py`: both explicit compiler-context/scoped coverage regressions pass.
- `python3 scripts/ci/test_dependencies.py` in the qualified image: all 16 real-CMake regressions pass.
  Covers missing/old Qt and GTest, private Wayland Qt mismatch, missing GMock, Test prerequisites,
  internal/external/system/cached prefix isolation, provider paths with spaces, missing pkg-config/ XML,
  standalone Config restriction, compatible reuse and changed external-prefix cache archival.
  Configure/build/test failures never create dependency/preparation trees or download sources.
- Canonical bundle drift, exact module/dependency copies, generated installer-profile drift, Python/shell
  syntax, architecture boundaries (including eight fixtures), QML import policy and whitespace checks pass.
- Focused model metadata generation passes with complete AuthenticationPromptModel properties and
  exports; initial empty metadata reproduced and retained. Logs: `build/ci/edm012-focused/`.

## Earlier attempts and retained evidence

Preparation `build/ci/20261014T074830Z-tlkx_mg2` passed. Later identity changes archived preparation
and reused verified compatible providers (`20261014T075739Z-ia96v9wc`,
`20261014T081523Z-0vh7_4n7`, `20261014T084010Z-frhe_aqj`). Full preparation logs were inspected.
Only Qt private-module compatibility notices remain in provider builds.

`build/ci/20261014T080218Z-1wcg1_51` passed Release, full strict static checks, standalone consumer
and licensing. Its Debug build passed but namespace startup failed because the adapted launcher initially
omitted Shell's existing privileged option. Restoring it and adding an early namespace preflight fixed
this. `build/ci/20261014T084504Z-0vopzmy7` then passed all 1,157 CTest registrations and standalone
package acceptance. Four directly discovered private-bus tests retain skips; the dedicated private-bus
registration runs those same four cases successfully. The Controls launch registration passed 45 real
frontend/style combinations plus three missing-module checks, producing 58 retained log files.

These runs exposed registrar warnings addressed by the separate metadata cleanup. Final acceptance
below supersedes their application build results; their failure/correction evidence is retained.

## Final clean acceptance — 2026-10-14

`JOBS=8 python3 scripts/ci/run.py` passes all four lanes in
`build/ci/20261014T085921Z-205uwj2o`. Complete logs were inspected, and implementation hashes
were compared with the final working tree. Documentation-only closure is followed by REUSE,
bundle/declaration/profile drift and whitespace checks; application acceptance is not repeated.

The qualified full Arch digest is
`ghcr.io/lebedenko/holonight-deps@sha256:37ca051c39d1688d6413c47d94b8c0f221152dac6981ec67d155f2c43ea17b40`,
image ID `sha256:51bb1aed43c0ad4e9336ed49962862ec78293f4380fd05e73b954e88c6c479c7`.
REUSE uses `docker.io/fsfe/reuse@sha256:85462a75c0f8efda09ddd190b92816b70e7662577c8427429e11e1b9f25a992e`.
Toolchain: GCC 16.2.1, Qt 6.11.2, GTest 1.18.0, CMake 4.4.3, Ninja 1.13.2,
LLVM format/tidy 22.1.8, Python 3.14.7, Task 3.53.1 and Wayland scanner 1.26.0.
Full package identities and compiler contexts are retained in each lane's artifacts.

Debug: 1,157 CTest registrations, zero failures. Four duplicate standalone private-bus registrations
are skipped because they lack the explicit disposable-bus opt-in; the dedicated registration passes
all four cases. Controls real launch acceptance passes 45 build/relocated frontend/style combinations
and three missing-module cases; 58 log files are retained. QML metadata/packaging guards pass,
including the authentication model's properties. Standalone config build/install/consumer passes with
provider sources/build trees hidden.

Release: clean optimized build and staged system-layout installation pass, including the executable
and preserved HoloNightShellConfig package. Static: canonical format and strict tidy across all 244
owned translation units pass, plus the package consumer in its own compiler database. QML lint,
canonical policy, interface/types checks, architecture and all focused regressions pass. REUSE passes.
The registrar warnings have been resolved; expected Qt private-module compatibility notices and
negative-fixture diagnostics remain, along with suppressed external-header/existing NOLINT tidy counts.

See [machine-readable verification](VERIFICATION.json) for exact provider options, source hashes,
container/toolchain identities and complete log hashes. Real Docker acceptance is verified; Podman
argument mapping is covered by fixtures, without claiming a real Podman run.
No system deployment, live Shell restart, publication or gitlink update occurred. Settings remains
Planned; the umbrella initiative remains Accepted. Fresh user-operated desktop review belongs to
final integration.

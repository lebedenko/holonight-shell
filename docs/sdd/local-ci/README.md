# Local CI rehearsal

Baseline ad351ab06cf58b9eb81ff932634180a94c460f9b. Umbrella CI-014.

Preserve independent build/test, static and REUSE results and push/PR triggers.
Use shared scripts with the immutable build image and checksum-pinned supplements;
REUSE 6.2.0 has its matching immutable environment. Config fe69a59e6b73167fd5349223a4d265d75386c139,
Qt 8d11e3e91fea5ad0d20a34f2ed27e5e5f485124a and System Services
3e2928eb55bbc3de2b1e877e29aa57d47077c05d are provider contracts. Qt was corrected from 863af418: existing icon QML
uses rendering and normalColor; the older revision lacks rendering. Initial
native runtime failure logs retain this evidence. Use system Python explicitly
to preserve its availability under filesystem isolation.
Fresh Release providers and Debug BUILD_TESTS=ON consumers; remove provider
source trees before configuring consumers. Architecture, qmltypes, full CTest
with private D-Bus and required bubblewrap filesystem/network namespaces,
headless Sway Controls startup evidence, QML lint and full format/tidy remain
required. No live desktop automation or production activation.

Read-only input snapshot contains tracked edits and non-ignored new files,
including symlinks/modes; new inputs are reported. Disposable work/build trees
and host-owned logs, source state, versions, image references and lane results
live under ignored build/ci. Every unavailable/failed required check fails.
Build/test alone uses a privileged disposable container for required namespaces;
static/licensing do not. Local hosts receive no AppArmor/sysctl changes. GitHub
retains the existing user-namespace sysctl prerequisite on its disposable Ubuntu
runner. No publication or pin changes are included.
Native owned diagnostics must be fixed before acceptance. Scope compiler header
analysis to owned apps/libs/integrations/tests and default to two analysis workers.
Retain check families; only single-line comma policy works around clang 23's
empty-argument delimiter bug.

## Implementation

Shared scripts live in scripts/ci; Task and the independent GitHub jobs invoke
the same launcher. JSON manifests pin image digests and all 38 supplements.
Fresh builds default to two compilation/analysis workers. The standalone
installed-package consumer has its own compiler database and static check;
the primary tidy target covers all 250 selected owned translation units.
Generated protocol dependencies now follow the current integration targets.

Owned clang-tidy 23 diagnostics were corrected with explicit initialization,
braces, attributes, local names and small smoke-test helpers. Public model roles,
plugin ABI enums, QML methods and fixture assertions remain intact. Product,
protocol and fixture string values/counts were compared with the baseline.
The smoke printf-to-println conversion retains equivalent output.

Installed Audio headers supply metadata only, without compiling duplicate
provider code. Anonymous model registrations and canonical provider property
types resolve inherited audio properties in QML tools. The metadata guard rejects
the previous incomplete output and requires anonymous model semantics; QML lint
now completes without the seven inherited-audio warnings.

## Acceptance — 2026-10-04

`task ci`, invoked by the ignored isolation verifier
`python3 build/ci/acceptance.py`, exits zero. All build/test, static and REUSE
lanes pass in build/ci/20261004T085924Z-hxoqxewi. Full logs were inspected:
1,212 CTest checks, architecture, QML types/packaging/lint, private D-Bus,
headless Sway and installed frontend startup checks pass. Full format/tidy and
the installed-package consumer check pass. REUSE 6.2.0 passes. Only expected
Qt private-header compatibility notices remain; providers and consumers use
the same Qt build. All 60 evidence files are host-owned. Source/development
isolation passes for 6,917 files, including content, modes, timestamps and links.

The initial run, build/ci/20261003T234240Z-ps4souks, passed build/test and
licensing but failed static checks on obsolete generated-header dependencies.
Its failure and complete logs remain available; 6,916-file isolation passed.
The corrected acceptance starts each lane from fresh providers and build trees.

An exact-provider fresh native Debug build in build/ci/native-final passes all
1,212 CTest checks. After the final audio metadata correction, the affected
36 QML/runtime/startup checks pass again. Native full required tidy (clang 23),
separate installed-consumer build/tidy/run, format, architecture, QML types/lint
and licensing pass. Logs are build/ci/native-final-*.log and
build/ci/native-consumer*.log. Provider provenance is recorded beneath
build/ci/native-providers-corrected; compatible Release provider artifacts were
reused only after checking their revisions, compiler, Qt and CMake options.

Four launcher regressions and additional individual-lane fault injection pass:
edits/deletions/new/ignored/space-containing inputs, executable bits, symlinks,
rootless Podman argv mapping, missing runtimes and required failures are covered.
Every individual failed lane fails the task even if the remaining lanes pass.
Python/shell syntax and final diff checks pass. Real Podman is unavailable and
has not been claimed as verified. Existing live compositor/Labwc manual tasks
remain available; no live desktop interaction was automated. No push or pin update.

Developer-entry-point follow-up: the default merged database selected a legacy
build-tests tree missing generated Labwc headers. Its failure log is retained as
build/ci/native-task-tidy-src-final.log; this is a stale compiler context, not a
new source failure. HOLONIGHT_TIDY_DATABASE now selects a fresh database explicitly,
scope coverage considers only the requested source/test files, and files with
multiple compiler contexts are invoked once. Both regression tests pass on the
host and in the pinned container (build/ci/pinned-tooling-context-tests.log).
Fresh-snapshot pinned licensing passes in build/ci/20261004T103241Z-oww9jpq4.
`HOLONIGHT_TIDY_DATABASE=build/ci/native-final/compile_commands.json task tidy-src`
passes with clang-tidy 23; its complete log is
build/ci/native-task-tidy-src-explicit.log. Application source/build checks are
unaffected by this tooling-only follow-up and are not repeated.

# Shell Architecture Alignment

Status: Draft

Review recorded: 2026-09-24. Reviewed baseline: `f0fb057`.
Current shell HEAD verified when writing: `f0fb0574beae8401970806c25e8242d964e70774`;
the working tree was clean before this documentation change.

This local SDD records eight findings and proposed follow-up work. The original review was documentation
only. Implementation was subsequently requested; progress and remaining acceptance work are recorded in
[IMPLEMENTATION.md](IMPLEMENTATION.md). No umbrella initiative state changes are implied.

## Documents

- [SPEC.md](SPEC.md): numbered requirements and acceptance criteria.
- [DESIGN.md](DESIGN.md): proposed approaches, ownership, compatibility, and investigations.
- [TASKS.md](TASKS.md): ordered work packages, dependencies, and verification expectations.
- [IMPLEMENTATION.md](IMPLEMENTATION.md): code changes, checks, and outstanding acceptance work.
- [TARGET-BOUNDARIES.md](TARGET-BOUNDARIES.md): allowed target edges and public contract rationale.

## Findings and priorities

Priorities are proposed sequencing guidance: P1 addresses verification gaps and interaction semantics;
P2 addresses simplification and maintainability. Confirmed observations below come from source inspection
at the baseline; recommendations are not claims of measured runtime failures.

| Finding | Priority | Confirmed observation and source | Recommendation |
|---|---|---|---|
| F-01 Coverage | P1 | The coverage library list in [root CMake](../../../CMakeLists.txt) omits `holonight_compositor` despite its inclusion in report filters. [Test CMake](../../../tests/CMakeLists.txt) directly invokes selected binaries and omits shell-config/authentication paths from report scope. | Reconcile all current targets, instrumentation, test execution, and report inclusion; document exclusions and use the isolated test environment. |
| F-02 QML metadata | P1 | [ShellApplication](../../../apps/shell/app/ShellApplication.cpp) registers `SettingsNavigationService` at runtime; its [header](../../../apps/shell/app/SettingsNavigationService.h) lacks QML export annotations, and [shell CMake](../../../apps/shell/CMakeLists.txt) does not merge app metatypes. [The checker](../../../scripts/check-qmltypes.sh) checks only five C++ type names. Shell CMake uses private Qt registration helpers. | Cover runtime-exposed interfaces and strengthen checks. Investigate public registration APIs separately. |
| F-03 Appearance | P2 | [AppearanceService](../../../libs/holonight-services/src/AppearanceService.cpp) owns a reader and emits revision changes; [AppearanceReloadBridge](../../../apps/shell/qml/Utility/AppearanceReloadBridge.qml) explicitly reloads the palette. [ThemeService](../../../libs/holonight-services/src/ThemeService.cpp) projects portal settings and emits a reload signal with no consumer found in the source audit. | Map reload ownership, then remove only proven redundancy while retaining live updates, cursor integration, portal behavior, and per-engine initialization. |
| F-04 Controls | P1 | [NetworkToggleRow](../../../apps/shell/qml/Popups/Network/NetworkToggleRow.qml) and [ProfileButton](../../../apps/shell/qml/Popups/Battery/ProfileButton.qml) implement pointer activation on Items without explicit keyboard, focus, or accessibility semantics; profile availability uses a separate `isEnabled` property. | Use semantic controls with the intended custom presentation and consistent disabled behavior. |
| F-05 Typography | P2 | Ordinary labels in [network controls](../../../apps/shell/qml/Popups/Network/NetworkToggleRow.qml) and [battery content](../../../apps/shell/qml/Popups/Battery/BatteryPopupContent.qml) specify fixed point sizes. | Map ordinary text to shared semantic roles; inventory justified fixed-size exceptions rather than mechanically replacing every number. |
| F-06 Audio cleanup | P2 | At the reviewed baseline, `libs/holonight-core/src/AudioState.{h,cpp}` was compiled and tested only by `tests/test_audio_state.cpp`; source references showed no production caller. [Service linkage](../../../libs/holonight-services/CMakeLists.txt) uses `HoloNightSystem::Audio`. | Confirm provider-backed behavior and coverage before removing unused shell conversions and their obsolete tests. |
| F-07 Tooling | P1 | Debug, tests, and coverage inherit one binary directory in [CMakePresets](../../../CMakePresets.json) and share `BUILD_DIR` in [Taskfile](../../../Taskfile.yml). `tidy-container` prepares only the Qt dependency, unlike local configuration's full dependency preparation. | Separate configurations and align local/container dependency preparation; leave image migration with its umbrella owner. |
| F-08 Boundaries | P2 | [Services](../../../libs/holonight-services/CMakeLists.txt) and [surfaces](../../../libs/holonight-surfaces/CMakeLists.txt) expose broad PUBLIC links/include paths. [Architecture checks](../../../scripts/check-architecture-boundaries.sh) enforce selected include and authentication rules, not a complete target dependency policy. | Document allowed edges and public contracts, tighten exposure where valid, and extend checks with negative fixtures. Splitting libraries requires demonstrated benefit. |

## Evidence and acceptance limits

The supplied review reports **24 passing focused tests in an existing build** at the reviewed baseline.
This is inherited existing-build evidence, not a new test run or clean acceptance. The supplied handoff
does not include exact commands, test names, or logs; this SDD does not invent them or assign that result
to individual work packages. Source findings were rechecked while writing these documents.

Implementation needs focused verification per [TASKS.md](TASKS.md), followed by the applicable
clean acceptance checks with exact revision, provider revisions, commands, results, and environment
recorded. The original documentation delivery checked links, requirement/task traceability, and
whitespace only; subsequent implementation checks are in [IMPLEMENTATION.md](IMPLEMENTATION.md).

## Related work and ownership

- [Local maintainability SDD](../ecosystem-maintainability-standardization/README.md) and its
  [tasks](../ecosystem-maintainability-standardization/TASKS.md): F-04/F-05 overlap
  `HOLONIGHT_SHELL-02`; F-01/F-02/F-07/F-08 support `HOLONIGHT_SHELL-03`. Their broader
  audit and mandatory CI acceptance obligations remain intact.
- [Ecosystem maintainability initiative](../../../../docs/initiatives/ecosystem-maintainability-standardization/README.md).
- [Shared CI build infrastructure](../../../../docs/initiatives/shared-ci-build-infrastructure/README.md):
  owns shared image migration; this SDD addresses shell workflow consistency only.
- [Appearance foundation](../../../../docs/initiatives/appearance-configuration-foundation/README.md),
  [unified controls](../../../../docs/initiatives/unified-qtquick-controls/README.md), and
  [shared system services](../../../../docs/initiatives/shared-system-services/README.md): preserve existing contracts.
- [Shared image architecture](../../../../docs/initiatives/shared-image-architecture/README.md):
  context for investigation-only MPRIS decoding reuse. Adoption requires a separately settled dependency contract.

Umbrella links resolve in the coordinated sibling checkout; they do not imply edits or status changes there.

# Shell Architecture Alignment Design

Status: Draft. Requirements: [SPEC.md](SPEC.md). Source evidence: [README.md](README.md).

## Coverage and workflow foundations (REQ-001, REQ-007)

First separate build directories and align provider preparation, then reconcile the coverage target list
against actual CMake targets. Keep a reviewable inventory of included targets and exclusions rather than
assuming the older hard-coded list is complete. Prefer the isolated CTest path used by `task test`, with
the coverage selection made explicit, over direct test-binary calls that bypass environment setup.
Preserve failure propagation before gcovr runs and prove production object instrumentation from build
commands. Provider and generated sources need explicit treatment in report scope.

Use distinct debug, test, and coverage directories consistently in presets, Task commands, qmltypes/lint
paths, and documentation. Define which configuration supplies `compile_commands.json`; do not let a
quality check unexpectedly reconfigure a developer's build. Align container preparation with the local
config → Qt → shell dependency order and include the system-services provider before shell configuration.
Mounting a host-built prefix alone is not compatibility evidence. Record toolchain/Qt/options/revisions
when reusing artifacts, otherwise prepare them in the compatible environment. Do not migrate the shared
CI image in this SDD.

## Runtime registration and metadata (REQ-002)

Inventory registrations in `ShellApplication` and QML-used interfaces, including app-owned objects absent
from the current merged metatypes. Add appropriate metadata declarations and app metatype inputs while
preserving the runtime factory and ownership model. Check generated export names, singleton flags and
signatures against that inventory. Keep real-engine import checks as well as metadata checks: either
side can be correct while the other is incomplete. Include deliberate omissions with rationale.

The existing private `_qt_internal_*` CMake calls are a maintenance risk, not proof that a safe public
replacement exists. A separate feasibility step should compare public Qt APIs on supported versions,
prototype metadata generation, and verify duplicate registration, engine identity, ownership, and resource
layout. Retaining the existing mechanism with a documented constraint is a valid investigation outcome.

## Appearance responsibilities (REQ-003)

Map the shared reader/watchers, shell `AppearanceService`, per-engine palette/shape singletons,
`AppearanceReloadBridge`, and `ThemeService` signal consumers before removing wiring. Eliminate unused
signals and duplicate reload requests only where focused tests show that shared observation covers both
existing and newly created engines. A bridge may remain if it performs necessary synchronization.

Shared providers retain configuration parsing, defaults, validation, and appearance primitives. The shell
retains portal projection through `ThemeService`/`SettingsPortalBackend` and cursor session integration
through `ShellApplication`. Test replacement and invalid configuration with temporary paths and a private
session bus; do not mutate the running user's appearance to obtain automated evidence. Changes to provider
contracts require separately coordinated work, not a shell-local workaround.

## Semantic controls and typography (REQ-004, REQ-005)

Prefer existing shared controls or standard Controls bases with custom backgrounds/content for the
network toggle and profile button. Preserve glow, disc, glyph, checked-state, and animation intent while
letting the control handle keyboard activation, focus, enabled state, and accessibility. Keep existing
service calls and public component properties compatible, or migrate all callers in the same work package.
Review other network/profile interactions against the same inventory; do not replace already-semantic
controls merely for uniformity.

Map ordinary labels to existing shared typography roles. Produce a file/component-level exception ledger
for display values, clocks, numeric readouts, or other fixed-size uses that have a concrete visual need;
these are candidates for review, not automatically approved exceptions. Validate layout under changed
fonts and fractional scaling. Avoid adding new provider roles unless a missing shared contract is settled.

Run deterministic offscreen keyboard/state/accessibility tests under HoloNight and Fusion. Where actual
assistive technology or compositor visuals require manual verification, record user-performed checks.
Do not automate pointer movement, window focus, or other focus-dependent desktop interaction.

## Audio cleanup (REQ-006)

Search every helper and type reference before deleting `AudioState` and its build/test entries. Compare
existing shell test assertions with the shared system-services audio controller/backend/model coverage,
including percent rounding/clamping, channels, and mute state. Record the provider revision and exact
evidence. Keep provider behavior in the provider and shell projection behavior in shell tests. Missing
provider guarantees block the affected deletion until a separate contract is resolved; this SDD does not
authorize provider edits.

## Target policy (REQ-008)

Start from existing libraries and constrain dependency direction without splitting them. The proposed
internal policy below is an audit starting point; enumerate actual CMake edges and justified exceptions
before enforcement. External Qt/provider usage must be recorded separately per public-header need.

| Owner | Allowed internal dependency direction / responsibility |
|---|---|
| `holonight_platform` | Platform foundations; no core, services, surfaces, or app dependency. |
| `holonight_shell_config` | Shell configuration over its shared provider; no upper shell layers. |
| `holonight_core` | Shell config and platform foundations; no services, surfaces, or app dependency. |
| `holonight_compositor` | Platform integration; no services, surfaces, or app dependency. |
| `holonight_services` | Core, compositor, platform and declared providers; no surfaces or app dependency. |
| `holonight_surfaces` | Core, compositor, platform; service links only for reviewed presentation orchestration. |
| `holonight_app` / executable | Shell composition and wiring of lower layers. |
| Authentication targets | Authentication core/QML and declared shared providers; independent of shell composition, services, and surfaces. |

Preserve the current notification/MPRIS surface include exceptions until their orchestration needs have
been reviewed. Tighten PUBLIC exposure based on headers and transitive consumer requirements, not by
blindly converting every link to PRIVATE. Checks should inspect declared target edges and include policy
with temporary negative fixtures, without corrupting the real source tree. Validate metadata generation
and legitimate consumers after narrowing visibility. A library split is deferred unless measured build
cost or a specific ownership/testability problem justifies it.

## Deferred shared decoding investigation

The shell's [MprisArtworkCache](../../../libs/holonight-services/src/mpris/MprisArtworkCache.cpp)
uses `QImageReader`, size limits, worker decoding, and cached output. Assess whether `holonight-images`
can replace only the decoding mechanics. Shell downloading, scheduling/deduplication, cache keys and
storage policy, callbacks, and presentation remain shell-owned.

Compare format support, local/data/remote inputs, decode/resource limits, orientation/scaling, thread
safety, cancellation/error behavior, and output compatibility. Record expected benefit and dependency cost.
The outcome may be defer or retain. Adoption requires a separate settled package/API/version contract and
verification plan through the [shared image initiative](../../../../docs/initiatives/shared-image-architecture/README.md);
this draft neither adds the dependency nor changes that initiative's acceptance state.

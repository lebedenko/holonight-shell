# Shell Architecture Alignment Tasks

Status: Implementation in progress by user request; acceptance remains open. Requirements: [SPEC.md](SPEC.md). Approaches: [DESIGN.md](DESIGN.md).

Dependencies below establish execution order after scope acceptance. Before a task becomes `Ready`, record
its exact shell baseline and relevant provider revisions and resolve any shared contract it needs. All
implementation ownership is `holonight-shell`; provider changes and umbrella coordination need separate work.

| ID | Work package / output | Requirements | Depends on | State | Verification expected |
|---|---|---|---|---|---|
| AA-01 | Inventory current targets, runtime QML interfaces, appearance observers, custom controls, fixed text sizes, and public dependency edges; record exclusions and exceptions. | REQ-001–REQ-008 | — | In Progress | Compare inventories to source/CMake and baseline; distinguish confirmed gaps from recommendations. |
| AA-02 | Separate debug/test/coverage configurations and align local/container provider preparation and documented paths. | REQ-007 | AA-01 | In Progress | Configure each independently; inspect caches for isolation; demonstrate fresh local/container preparation and record provider/toolchain compatibility. |
| AA-03 | Reconcile coverage instrumentation, test selection, isolated execution, filters, and exclusions. | REQ-001 | AA-02 | In Progress | Inspect production compile/link flags and report inclusion; run isolated coverage, confirm test failure propagation and thresholds; record exclusions/denominator. |
| AA-04 | Complete runtime singleton metadata, including SettingsNavigationService, and strengthen interface checks. | REQ-002 | AA-02 | In Progress | Compare generated metadata with real registrations and signatures; run focused runtime imports, `task qmltypes-check`, and `task qml-lint`; malformed/missing metadata fixtures must fail. |
| AA-05 | Investigate supported public Qt registration APIs; record feasibility and compatibility constraints. | REQ-002 | AA-04 | In Progress | Prototype against supported Qt versions and multi-engine ownership; document retain/defer/replace recommendation. No production replacement required. |
| AA-06 | Simplify proven redundant appearance wiring while retaining shell projections. | REQ-003 | AA-04 | In Progress | Test atomic replacement, invalid/missing input and recovery across two engines plus a later engine; check portal projection, cursor integration and live palette/font/shape updates in isolation. |
| AA-07 | Restore network/profile control semantics with existing visual presentation. | REQ-004 | AA-04 | In Progress | Offscreen keyboard activation, focus, disabled-state and accessibility assertions under HoloNight and Fusion; QML lint and user-performed visual/assistive checks where needed. |
| AA-08 | Adopt semantic text roles and finish the fixed-size exception ledger. | REQ-005 | AA-06, AA-07 | In Progress | Verify font family/size updates, long text and geometry at 1.0/1.25/1.5 scales; QML lint and representative visual evidence. |
| AA-09 | Confirm shared-provider audio coverage, then remove unused AudioState sources and obsolete tests. | REQ-006 | AA-03 | In Progress | Search all callers; map old assertions to exact provider tests/revision; verify provider-backed shell service/UI behavior and absence of stale build references. |
| AA-10 | Document/enforce allowed target edges and narrow public implementation exposure. | REQ-008 | AA-04, AA-06, AA-09 | In Progress | `task architecture-check` with negative fixtures for forbidden edges/includes/authentication dependencies; valid exceptions pass; build affected consumers and run qmltypes checks. |
| AA-11 | Investigate shared MPRIS decoding mechanics and record a decision without adoption. | Investigation only | AA-01 | In Progress | Compare API/formats/limits/threading/errors/output and ownership; describe separate dependency contract and tests needed before any adoption. |
| AA-12 | Record final local acceptance and links to overlapping maintainability obligations. | REQ-001–REQ-008 | AA-03, AA-05, AA-06, AA-07, AA-08, AA-09, AA-10, AA-11 | Planned | Clean acceptance build, required tests and applicable checks at exact revisions; inspect full logs, record manual checks and outstanding limits. No automatic umbrella status change. |

## Verification discipline

Use the narrowest relevant checks while editing. Future behavior changes require `task test`; QML changes
require `task qml-lint`, registration changes require `task qmltypes-check`, and boundary changes require
`task architecture-check`. Adapt build-directory arguments to the accepted AA-02 workflow. Compositor-facing
changes also require the repository's compositor smoke checklist and relevant user-performed checks.
Do not automate desktop pointer/focus interaction.

Record exact commands, shell/provider revisions, Qt/toolchain/build options, results, and dates per package
before marking it `Done`. Reuse compatible provider artifacts only after checking that evidence. Run clean
acceptance once implementation is ready; rerun checks invalidated by corrections. Do not use CI as the local
development loop or claim an unrun manual check passed.

The inherited **24 passing focused tests** are existing-build review evidence only, with the provenance
limits in [README.md](README.md). They do not satisfy any future package or clean acceptance requirement.

## Original documentation delivery

The initial review verified local Markdown links, REQ-001 through REQ-008 mappings, initial `Planned`
states, and whitespace. Application builds/tests were not rerun for that documentation-only
change. Implementation evidence is recorded in [IMPLEMENTATION.md](IMPLEMENTATION.md).

`HOLONIGHT_SHELL-02` retains its broader controls audit and exception-recording obligations;
`HOLONIGHT_SHELL-03` retains mandatory CI qmllint, qmltypes, resource-alias, architecture, and installed-package
checks. Completing overlapping tasks here does not automatically close either
[maintainability task](../ecosystem-maintainability-standardization/TASKS.md).

Allowed states: `Planned`, `Ready`, `In Progress`, `Done`, `Blocked`, `Superseded`.

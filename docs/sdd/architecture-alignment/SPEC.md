# Shell Architecture Alignment Specification

Status: Draft. Baseline and evidence limits: [README.md](README.md).
All requirements describe future work; task states are in [TASKS.md](TASKS.md).

## REQ-001: Coverage matches the target graph

Reconcile instrumentation, executed tests, and report filters with current shell targets.

Acceptance criteria:

- Inventory production and test targets, including compositor, shell configuration, and authentication;
  each is included or has an explicit exclusion with rationale. Generated and provider code exclusions
  are documented without hiding shell-owned behavior.
- Instrument all included production objects and their required test/link targets. Inspect compile/link
  commands and report entries to prove inclusion; test-binary instrumentation alone is insufficient.
- Run coverage tests through `scripts/run-isolated-test.py --hide-host`, preserving required offscreen,
  temporary configuration, and private session-bus setup. Report generation fails when tests fail.
- Preserve existing coverage thresholds unless a separately justified change is accepted; record changed
  report denominators and exclusions so percentages remain interpretable.

## REQ-002: Runtime QML interfaces have usable metadata

Generated metadata shall represent all runtime-exposed shell singleton interfaces, including
`SettingsNavigationService.openPage(string)`.

Acceptance criteria:

- Compare an inventory of runtime registrations against generated exports, singleton status, module/version,
  and QML-used properties, signals, and invokables; identify deliberate non-singleton exports separately.
- Extend metadata inputs/annotations and checks to detect missing types and missing or wrong interface
  signatures. A populated file or a matching C++ type name alone does not establish correctness.
- Verify real runtime import/registration alongside generated metadata, resource aliases, and qmllint;
  preserve application ownership, object lifetimes, and multiple-engine behavior.
- Record public-API registration feasibility separately. No replacement of private Qt machinery is
  required unless supported Qt versions and current registration semantics can be preserved.

## REQ-003: Appearance has a clear reload path

Simplify redundant shell reload paths only after identifying the authority for each projection.

Acceptance criteria:

- Preserve shared-provider parsing, fallback, and watching; shell code does not introduce a second schema.
- Preserve portal settings projection and session cursor integration, including initial values and live changes.
- In temporary configuration, verify atomic replacement, malformed/invalid configuration, missing files,
  and subsequent recovery across at least two live QML engines and an engine created after a change.
- Palette, typography, and shape updates reach affected engines without stale bindings, duplicate shell
  reload wiring, or dependence on a removed signal. Record why retained explicit reload paths are necessary.

## REQ-004: Custom controls retain interaction semantics

Network and profile controls shall preserve their visual intent while exposing standard interaction behavior.

Acceptance criteria:

- Keyboard users can reach, identify focus on, and activate enabled controls with the appropriate keys;
  activation occurs once and preserves checked/selected state and existing service requests.
- Disabled/unavailable controls reject pointer and keyboard activation and expose consistent enabled state.
- Accessible names, roles, checked/selected state, and disabled state match the action; captions remain
  available beyond hover-only presentation where needed for keyboard or accessibility use.
- Focus, keyboard, state, and accessible behavior pass under both HoloNight and Fusion, with existing
  network switch and profile disc/glyph presentation retained. Use `QtQuick.Controls as Controls`.

## REQ-005: Ordinary text follows semantic typography

Adopt shared typography roles for ordinary labels without obscuring intentional display typography.

Acceptance criteria:

- Inventory fixed `font.pointSize`/`font.pixelSize` assignments across shell QML and map ordinary labels to
  existing shared roles. Record each retained exception by component, purpose, and sizing rationale.
- Verify changes to shared font family/size propagate to ordinary text. Test long labels and changed
  font metrics for wrapping, elision, clipping, and popup geometry.
- Check representative surfaces at scale 1.0 and fractional scales 1.25 and 1.5; record geometry and visual
  results for each changed family of components. Exceptions retain deliberate scaling behavior.

## REQ-006: Remove obsolete shell audio conversions

Remove unused shell-local conversion code and tests only after verifying shared-provider coverage.

Acceptance criteria:

- Confirm no production consumer of `AudioState` helpers remains; compare rounding, volume limits,
  channel handling, and mute behavior with the provider's contract and tests at an exact revision.
- Remove unused sources, declarations, build entries, and obsolete tests together. If a necessary
  provider behavior lacks evidence, record the gap and resolve the contract before deletion.
- Verify shell behavior through its provider-backed audio service/UI boundary after cleanup. Retain
  meaningful shell adapter and presentation tests; do not reproduce provider implementation tests locally.

## REQ-007: Build configurations and dependencies are consistent

Local and container workflows shall use independent configurations and compatible prepared dependencies.

Acceptance criteria:

- Debug, test, and coverage configurations use distinct binary directories in presets and Task workflows;
  invoking one does not silently change another's cache. Keep release/staging separation and document
  overrides, artifact paths, report paths, and compile-command selection.
- Local and container preparation accounts for config, Qt, and system-services providers in dependency
  order, with exact revisions and explicit package paths. Fresh configuration must not rely on incidental
  host packages or a previous local build.
- Validate compiler, Qt, ABI/build options, and provider revisions before reusing artifacts in a container;
  build compatible provider artifacts when reuse is unsuitable.
- Verify configurations independently and demonstrate local/container dependency preparation. Shared CI
  image migration remains owned by its existing umbrella initiative.

## REQ-008: Target boundaries are explicit and enforceable

Document allowed target dependencies and narrow public exposure without speculative library restructuring.

Acceptance criteria:

- Record current and allowed internal target edges, provider edges, include surfaces, and exceptions with
  rationale. Lower layers cannot acquire application composition dependencies; authentication remains independent.
- Audit PUBLIC links and include directories against actual public-header needs; narrow implementation-only
  exposure without breaking legitimate consumers, generated metadata, or installed-package contracts.
- Extend architecture checks beyond selected includes. Negative fixtures demonstrate rejection of forbidden
  target edges, unauthorized service includes, and authentication-to-shell dependencies; valid exceptions pass.
- Require a concrete ownership, dependency, testability, or build-cost benefit before any library split.
  Broad target size alone is insufficient justification.

## Investigation boundary

Shared MPRIS image decoding is investigation-only, not a ninth adoption requirement. Assess
`holonight-images` compatibility while retaining shell-owned downloading, scheduling, caching, and
presentation. No new dependency or migration is authorized until a separate contract settles package/API,
revision, formats, limits, threading, errors, and verification obligations; see [DESIGN.md](DESIGN.md).

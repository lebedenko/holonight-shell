# Canonical HoloNight Module Adoption Specification

**Project:** holonight-shell
**Version:** 1.0
**Date:** 2026-07-27
**Status:** Completed
**Source brief:** `/tmp/downstream-adoption.md`
**Upstream baseline:** `holonight-qt` commit `25939dd`

## Overview

`holonight-qt` now exposes its shared primitives and rich controls through the canonical
`Holonight.Core` and `Holonight.Controls` QML modules. This pipeline shall migrate HoloNight Shell
and HoloNight Settings to those ownership boundaries while preserving their current behavior,
appearance, packaging, and launchability.

This is an import and dependency adoption cycle. It shall not remove upstream compatibility
surfaces, redesign application UI, or change any migrated type's public contract.

## Existing Contracts

The canonical upstream interfaces are:

- `Holonight.Core`: `HoloniightPalette`, `HolonightTheme`, `HnAppearance`,
  `HnShapeProfile`, `HnSurfaceRole`, `HnCornerStyle`, `HnShapeKind`, `HnCornerMask`,
  `HnIconProvider`, and `HnIcon`.
- `Holonight.Controls`: `HnSurfaceFrame` and `HnApplicationWindow`.
- `Holonight`: standard HoloNight Qt Quick Controls style components and a compatibility surface
  for the migrated types.
- `holonight`: a lowercase compatibility surface. There are no lowercase
  `holonight.core` or `holonight.controls` modules.
- Installed CMake component targets remain `HolonightQt::Core` and
  `HolonightQt::Controls`.

Canonical and compatibility imports resolve the same Core singleton instances. This application
shall consume those upstream registrations rather than duplicate migrated types locally.

## Scope

This pipeline includes:

- all application QML under `apps/shell/qml/` and `apps/settings/qml/`;
- QML tests and C++ QML test-harness registration or import-path support under `tests/`;
- dynamically constructed QML source, module names, import paths, and runtime loaders;
- CMake target linkage, QML import metadata, build-tree runtime paths, install rules, packaging,
  and CI dependency setup where canonical module discovery requires changes;
- focused canonical-import, component-instantiation, singleton-behavior, and packaging coverage;
- headless verification and supported live launches at scale factors 1.0 and 1.25.

## Functional Requirements

### REQ-F-001 — Inventory every affected import and use

Before changing application imports, the implementation SHALL inspect all QML and runtime-loading
paths for `import Holonight`, `import holonight`, and every migrated type named in Existing
Contracts.

**Acceptance criteria:**

- The inventory covers shell QML, Settings QML, QML tests, C++-constructed QML, generated test
  module metadata, CMake QML import declarations, and runtime import paths.
- Each affected QML file is classified by the canonical types and styled controls it consumes.
- Alias-qualified imports and potential unqualified-name collisions are identified before editing.
- Dynamic QML strings do not retain an accidental compatibility dependency.

### REQ-F-002 — Import Core-owned types canonically

Application and test QML that uses a Core-owned migrated type SHALL import `Holonight.Core`.

**Acceptance criteria:**

- Palette, theme, appearance, shape, enum, icon-provider, and `HnIcon` references resolve through
  `Holonight.Core`.
- Canonical module spelling and capitalization are exact.
- No `holonight.core` import is introduced.
- Existing aliases are preserved unless an explicit alias is required to resolve an ambiguity.

### REQ-F-003 — Import Controls-owned types canonically

Application and test QML that uses `HnSurfaceFrame` or `HnApplicationWindow` SHALL import
`Holonight.Controls`.

**Acceptance criteria:**

- Every migrated use of `HnSurfaceFrame` resolves through `Holonight.Controls`.
- Any current or newly discovered `HnApplicationWindow` use resolves through
  `Holonight.Controls`.
- No `holonight.controls` import is introduced.
- Shell-local composites such as `HudFrame` and `BarFrame` remain application-owned and are not
  duplicated or moved upstream by this pipeline.

### REQ-F-004 — Retain the style module only where required

Where a QML file uses standard HoloNight-styled Qt Quick Controls, it SHALL retain
`import Holonight`.

**Acceptance criteria:**

- Styled `Button`, `TextField`, `ComboBox`, `CheckBox`, `Switch`, `Slider`, popup, dialog, menu,
  window, and other standard-control uses continue selecting the HoloNight style as before.
- A file that uses both styled controls and canonical migrated types imports both required
  modules.
- `import Holonight` is removed only when the file has no remaining style-owned or intentionally
  compatible type use.
- `Holonight.Components` imports are not changed unless repository inspection proves they are
  directly affected by the upstream canonical-module migration.

### REQ-F-005 — Preserve runtime singleton identity and behavior

The migration SHALL preserve the observable behavior of the palette, theme, appearance, shape,
and icon services.

**Acceptance criteria:**

- `HoloniightPalette`, `HolonightTheme`, `HnAppearance`, `HnShapeProfile`, and
  `HnIconProvider` remain process-wide singletons.
- Theme and appearance changes continue to propagate to existing bindings without restart or
  duplicate singleton state.
- Enum values, method calls, signal behavior, and property values used by the application remain
  unchanged.
- The test harness does not register a conflicting compatibility singleton when exercising the
  installed canonical module.

### REQ-F-006 — Preserve component behavior and rendering

Changing import ownership SHALL NOT alter component APIs, layout, geometry, colors, shapes, icons,
focus behavior, interaction, or animations.

**Acceptance criteria:**

- Important shell surfaces and Settings pages still instantiate.
- Existing `HnIcon` state and color behavior remains intact.
- Existing `HnSurfaceFrame` content ownership, surface roles, corner styles, and shape resolution
  remain intact.
- No visual constants or application behavior are changed merely to facilitate the import
  migration.

### REQ-F-007 — Link canonical CMake components where required

Targets that consume canonical QML modules SHALL use the existing
`HolonightQt::Core` and `HolonightQt::Controls` CMake targets where necessary for plugin discovery,
linkage, or deployment.

**Acceptance criteria:**

- No HoloNight package name, component name, namespace, or version is changed.
- The shell executable, Settings executable, and relevant test targets receive only the
  additional canonical dependencies they require.
- Existing `HolonightQt::Config` and `HolonightQt::Theme` dependencies retain their current
  behavior.
- No copied plugin, duplicated QML source, or private upstream implementation is added.

### REQ-F-008 — Preserve build-tree and installed execution

Canonical Core and Controls QML plugins SHALL be discoverable in both development and installed
execution modes.

**Acceptance criteria:**

- The configured build can resolve canonical imports from the installed development prefix.
- Installed/package execution includes or depends on the required Core and Controls plugin,
  `qmldir`, type metadata, and owned QML artifacts.
- CI obtains an upstream revision containing commit `25939dd` or later before configuring this
  repository.
- Existing application executable names, desktop files, install destinations, and package version
  remain unchanged.

### REQ-F-009 — Prove canonical imports with focused tests

Automated tests SHALL directly exercise the canonical module boundary.

**Acceptance criteria:**

- A focused test imports `Holonight.Core` and resolves representative singleton, enum, shape, and
  icon APIs.
- A focused test imports `Holonight.Controls` and instantiates `HnSurfaceFrame`.
- Important downstream shell components and Settings pages instantiate with their migrated
  imports.
- Theme, palette, appearance, and singleton use remain covered without relying exclusively on
  `import Holonight`.
- A migration guard detects newly migrated application QML that depends unnecessarily on the
  compatibility import.

### REQ-F-010 — Verify package contents and runtime loading

Automated or scripted verification SHALL confirm that deployment contains everything required by
the canonical imports.

**Acceptance criteria:**

- Install/package checks verify the Core and Controls runtime artifacts needed by both
  executables.
- A clean installed-context smoke test resolves the canonical imports without relying on source
  tree paths.
- Missing canonical plugins or metadata fail verification with an actionable error.
- Intentional retained compatibility imports remain loadable during this adoption cycle.

### REQ-F-011 — Validate supported launch modes and scale factors

After headless verification succeeds, the implementation SHALL launch each supported application
or representative UI entry point at scale factors 1.0 and 1.25 where the environment supports it.

**Acceptance criteria:**

- Runtime logs contain no new QML import, binding, singleton-registration, or component-creation
  errors.
- Shell launch is checked in the available live Wayland/Hyprland session.
- HoloNight Settings is launched at both requested scale factors.
- If a launch mode is unavailable, the completion report records the exact environmental blocker
  and the closest verification performed.

### REQ-F-012 — Record intentional compatibility use

The completed pipeline SHALL document every remaining intentional `import Holonight` or
`import holonight` use relevant to migrated application code.

**Acceptance criteria:**

- Each retained `Holonight` import is justified by a style-owned control or an explicitly
  documented compatibility need.
- Any retained lowercase import is explicitly listed and justified.
- No compatibility import is removed from upstream `holonight-qt`.

## Non-Functional Requirements

### REQ-NF-001 — Minimal, reviewable change

Changes SHALL be limited to imports, directly required dependency/package metadata, focused tests,
and pipeline documentation. Unrelated formatting, cleanup, abstraction, and refactoring are
prohibited.

### REQ-NF-002 — Compatibility

The migration SHALL preserve public behavior, visuals, package identity, install locations, and
existing user configuration. It SHALL target `holonight-qt` commit `25939dd` or later without
removing compatibility with the upstream migration's supported legacy surfaces.

### REQ-NF-003 — Deterministic verification

Canonical import and component-instantiation coverage SHALL run headlessly and without a live
network. Live compositor checks supplement rather than replace deterministic tests.

### REQ-NF-004 — QML tooling integrity

QML changes SHALL pass the repository's QML lint and generated-type checks. CMake/QML registration
changes SHALL not leave an empty or incomplete `HolonightShell` qmltypes artifact.

### REQ-NF-005 — Preserve local work

The implementation SHALL preserve pre-existing user changes and SHALL not edit generated Wayland
sources, secrets, credentials, environment files, or generated dependency lockfiles.

## Verification Requirements

Verification shall run from narrowest to broadest:

1. focused canonical Core/Controls import and instantiation tests;
2. affected QML component and Settings tests;
3. `task qml-lint`;
4. `task qmltypes-check` when QML/CMake registration is touched;
5. relevant install/package verification;
6. `task test` for the complete headless suite;
7. live launches at scale factors 1.0 and 1.25;
8. final diff review and `git diff --check`.

Failures known to predate this pipeline shall be separated from regressions in files changed by
this pipeline.

## Out of Scope

- Removing, deprecating, or redesigning `Holonight` or `holonight` compatibility surfaces.
- Adding lowercase Core or Controls modules.
- Migrating other downstream repositories.
- Public API, enum, singleton, package, component, or version changes in `holonight-qt`.
- Visual redesign, density work, semantic-size refactoring, or control replacement.
- Moving shell-specific composites into `holonight-qt`.
- Unrelated QML modernization, lint cleanup, formatting, or architecture refactoring.
- Committing or publishing the change.

## Open Design Decisions

The design stage shall resolve:

1. The exact per-file import matrix, including files that must retain `Holonight` for styled
   controls while adding Core or Controls.
2. Whether canonical CMake targets must be linked directly to each executable/test target or can
   be propagated through an existing target without obscuring runtime ownership.
3. How `FakeQmlServices` should support canonical singleton tests without conflicting with the
   real installed Core plugin.
4. The smallest robust migration guard that distinguishes legitimate styled-control imports from
   unnecessary compatibility imports.
5. Which existing install/package test is the correct owner for canonical plugin and metadata
   assertions.
6. The exact launch commands and log filters for scale-factor validation in the current
   Wayland/Hyprland environment.

## Completion Criteria

The pipeline is complete when:

- all affected application imports are classified and migrated to their canonical owners;
- intentional style and compatibility imports are documented;
- build-tree and installed execution resolve canonical modules;
- focused, full headless, packaging, and supported live-scale verification pass;
- the final diff contains no unrelated behavior or visual changes;
- the user reviews the results and explicitly approves closing the pipeline.

## Pipeline Status

- Requirements: approved
- Design: approved
- Task breakdown: approved
- Implementation: completed
- Pipeline closure: approved on 2026-07-28

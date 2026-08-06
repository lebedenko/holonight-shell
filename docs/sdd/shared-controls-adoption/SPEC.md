# Shared Controls Adoption Specification

**Project:** holonight-shell
**Version:** 1.0
**Date:** 2026-07-28
**Status:** Complete
**Shell implementation-start baseline:** `a7f0c822c3121018620366f6354700f413d7428a`
**Controls implementation-start baseline:** `00b1b55d88a9d20f527559b8551ad664760c2a7b`

## 1. Purpose

HoloNight Shell and HoloNight Settings contain application-owned visual compositions that now
have matching primitives in `Holonight.Controls`. This pipeline adopts those controls
incrementally while preserving application behavior and public QML contracts.

The pipeline is deliberately sequential. One named visual region, or one inseparable
parent/delegate pair, is changed at a time and presented for visual review. No later checkpoint
may begin until the current checkpoint is explicitly approved, skipped by user decision, or
otherwise disposed after an upstream block.

## 2. Scope

The adoption covers the Settings application, launcher, network and audio popups, and right
sidebar components listed in `TASKS.md`. It also covers focused behavioral tests, QML
instantiation checks, live compositor review, temporary before/after screenshots, and durable
checkpoint/upstream ledgers.

The shared-control module is consumed through:

```qml
import Holonight.Core
import Holonight.Controls
```

Existing standard HoloNight-styled controls continue to use `import Holonight` where required.

## 3. Functional Requirements

### REQ-F-001 — Preserve public contracts

Each migrated component SHALL preserve its filename, externally consumed properties, required
properties, signals, functions, model roles, service calls, routing, commands, and C++ interfaces.
The Settings window SHALL remain a C++-owned `QQuickView`.

### REQ-F-002 — Preserve observable behavior

Each checkpoint SHALL preserve pointer activation, keyboard activation and navigation, focus
transfer, selection, disabled behavior, empty/loading/error behavior, and model synchronization
that apply to the component. A visual-control adoption SHALL NOT change a domain action.

### REQ-F-003 — Delegate only shared visual contracts

Shared controls SHALL own their documented selection, surface, semantic sizing, focus, and
feedback presentation. The application SHALL retain routing, commands, service operations,
model interpretation, responsive surface policy, and domain-specific content. Application-owned
icons, mute buttons, volume controls, or other specialized content MAY be supplied through shared
control slots.

Downstream code SHALL NOT calculate shared-control selection colors.

### REQ-F-004 — Migrate one reviewable region at a time

Only the component named by the active checkpoint may be migrated. A parent and delegate may be
changed together only when they form an inseparable model/selection contract and the checkpoint
explicitly names both.

### REQ-F-005 — Enforce the checkpoint gate

Every visual checkpoint SHALL follow this sequence:

1. Record behavior, geometry, relevant states, and a baseline screenshot.
2. Add or update focused behavioral tests.
3. Migrate only the named component.
4. Run the focused test, `task qml-lint`, an applicable component-instantiation check, and
   `git diff --check`.
5. Exercise the applicable pointer, keyboard, focus, selected, disabled, empty, loading, and
   error states.
6. Capture a post-change screenshot and present the comparison.
7. Record the evidence and stop for explicit user approval.

The next checkpoint SHALL NOT start during the same autonomous implementation run.

### REQ-F-006 — Correct downstream defects in place

If review finds a downstream defect, the active checkpoint SHALL become `Needs correction`.
The correction SHALL remain scoped to that checkpoint, repeat its verification and screenshots,
and return to review. A failed review does not authorize starting another component.

### REQ-F-007 — Escalate upstream deficiencies without visual workarounds

If the matching shared control cannot preserve the required contract, the component SHALL be
restored to its last approved state, the checkpoint SHALL become `Blocked upstream`, and an
actionable entry SHALL be added to `UPSTREAM-NOTES.md`. A downstream visual workaround SHALL NOT
be added by default. Work stops until the user chooses an upstream disposition.

### REQ-F-008 — Use explicit checkpoint states

Every checkpoint SHALL have exactly one of these states: `Pending`, `In progress`,
`Needs correction`, `Blocked upstream`, `Skipped by decision`, or `Approved`. Only explicit user
review may produce `Approved` or `Skipped by decision`.

### REQ-F-009 — Revalidate dependency discovery

The first adoption checkpoint SHALL verify that `Holonight.Controls` is found through the
existing installed QML prefix and dynamic plugin loading. `HolonightQt::Controls` SHALL be linked
only if configure/build/runtime evidence demonstrates a need. The successful canonical-module
pipeline is the current evidence that extra linkage is unnecessary.

### REQ-F-010 — Keep evidence durable but screenshots temporary

`REVIEW-CHECKPOINTS.md` SHALL retain commands, results, screenshot identifiers or temporary
paths, corrections, and decisions. Screenshots are temporary review evidence and SHALL NOT be
committed unless the user explicitly changes that policy.

### REQ-F-011 — Maintain an actionable upstream ledger

Each upstream finding SHALL identify the checkpoint, reproduction, expected and actual behavior,
affected control/API, evidence, suggested correction, and disposition. New public upstream APIs
requested by this pipeline are documentation-only downstream.

## 4. Non-Functional Requirements

- Changes SHALL be small, readable, and consistent with existing QML conventions.
- No new framework, library, copied upstream control, or parallel design system may be added.
- Shared semantic sizes and slots are preferred over new fixed visual constants.
- User-visible strings remain translatable.
- Relevant behavior remains deterministic under QtQuickTest.
- Generated Wayland files, secrets, credentials, dependency lockfiles, and unrelated code remain
  untouched.

## 5. Exclusions

The following remain application-owned:

- HUD decoration, topbar widgets, and OSD renderers;
- weather visualizations;
- tray protocol menus and notification toasts;
- power-profile icon buttons;
- routing, commands, model/service operations, and compositor surface policy.

`HnApplicationWindow`, `HnTextArea`, and `HnCardDelegate` are excluded unless implementation-time
inspection discovers a new, exact contract match and the user explicitly approves adding a
checkpoint. The Settings window is not converted from its C++-owned `QQuickView`.

## 6. Milestone and Final Acceptance

After all checkpoints for a surface are approved:

- exercise dark and light themes at scale factors 1.0 and 1.25;
- run `task qmltypes-check`, `task test`, and the relevant live application workflow;
- capture milestone screenshots and record results.

Launcher acceptance includes its keyboard workflow. Network, audio, and sidebar acceptance
includes live compositor interaction. Final acceptance additionally requires:

- `task format-check`;
- a full diff review and `git diff --check`;
- a reconciled `UPSTREAM-NOTES.md`;
- every candidate marked `Approved`, `Skipped by decision`, or explicitly deferred by user
  decision;
- explicit user approval to close the pipeline.

## 7. Completion Criteria

The pipeline is complete only when all milestone and final acceptance checks pass, no checkpoint
is `Pending`, `In progress`, `Needs correction`, or `Blocked upstream`, public and behavioral
contracts are preserved, and the user approves closure. Creating these SDD artifacts does not
approve or begin the first implementation checkpoint.

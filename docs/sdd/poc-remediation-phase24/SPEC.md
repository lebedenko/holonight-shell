# Phase 24 — Precise Workspace Model Role Notifications

**Status**: Complete — implementation, automated validation, and user verification passed.

## Objective

Remediate U-01 I-01: make `WorkspaceModel` include the roles it changes in
each `dataChanged` notification, so QML does not need to reconsider unrelated
bindings for state-only workspace updates.

| Source | Phase 24 item | Impact |
|---|---|---|
| U-01 I-01 | Precise `dataChanged` role lists | Focus, occupancy, and urgency updates invalidate only the state role; full snapshot replacements remain fully described. |

## Functional Requirements

### REQ-F-01 — Emit the exact changed roles

`WorkspaceModel` shall provide a non-empty role list with every non-reset
`dataChanged` signal it emits.

- Focus, occupancy, and urgency changes shall report only `WorkspaceStateRole`.
- Same-row-count batch updates shall report all four model roles because every
  field in the incoming snapshot can differ.
- The existing affected row range, revision signals, reset behavior, and QML
  role names shall remain unchanged.

**Acceptance**: focused model tests observe `WorkspaceStateRole` for a
state-only update and all four roles for a same-count batch replacement.

## Constraints and Verification

- Keep the change local to `WorkspaceModel`; do not alter workspace-state
  calculation, Hyprland IPC, or QML delegates.
- Test the public `dataChanged` signal through `QSignalSpy`.
- Run focused workspace-model tests, `task test`, `task format-check`, and
  `git diff --check`.

## Out of Scope

- The other 46 queued Low-severity candidates after this planned tranche.
- Per-row diffing or changes to model-reset policy.
- QML delegate layout, animation, or visibility behavior.

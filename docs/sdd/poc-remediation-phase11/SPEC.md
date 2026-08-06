# Phase 11 — Empty Workspace Pill Activation

**Status**: Accepted

## Objective

Fix the user-visible failure where an empty numbered workspace pill receives
pointer feedback but does not switch the Hyprland workspace. Occupied pills
continue to switch correctly, and Hyprland keybindings can switch to the same
empty workspace.

The shell must make empty and occupied numbered pills equivalent activation
targets. An empty workspace has no ext-workspace handle, so it must reliably
use the Hyprland IPC dispatch path.

## Functional Requirements

### REQ-F-01 — Empty pill click dispatches its absolute ID

Clicking any visible numbered `WorkspacePill` with a positive `wsId`, including
one whose state is `WorkspaceModel.Empty`, shall emit exactly one workspace
activation request for that same absolute ID.

**Acceptance**: deterministic QML coverage clicks an empty pill and observes
one `activateWorkspaceRequested(id)` signal.

### REQ-F-02 — Empty-workspace dispatch is observable and reliable

`HyprlandWorkspaceService` shall submit `dispatch workspace <id>` for an
activation request even when the requested ID has no ext-workspace handle.

- A queued dispatch retains the most recent requested ID and runs after the
  current command completes.
- Failure to submit or complete a dispatch shall be logged with the requested
  ID and command outcome; it shall not fail silently.
- Existing activation of ext-workspace-backed (occupied) IDs remains intact.

**Acceptance**: service coverage drives an activation request through an
injectable IPC transport and asserts the dispatch command; live logs identify
the command outcome for an empty-pill click.

### REQ-F-03 — Live empty pill switches workspaces

Clicking an empty visible pill shall switch to that workspace on the bar's
monitor, update the active styling, and preserve normal click/hover feedback.

**Acceptance**: live Hyprland check verifies empty and occupied workspace
pills both switch successfully on each monitor.

## Constraints and Verification

- Do not change workspace number, occupancy, or keyboard-binding semantics.
- Keep the existing model signal as the single QML activation contract.
- Do not add a shell-out `hyprctl` fallback or a second IPC protocol.
- Add deterministic QML and service-level coverage before live verification.
- Run targeted core/QML tests, `task qml-lint`, `task qmltypes-check`, and
  `task test`.

## Out of Scope

- General workspace navigation redesign.
- Phase 7 backlog candidates unrelated to workspace activation.
- Changing the bounded-strip behavior completed in Phase 10.

## Acceptance Record

On 2026-07-21, live Hyprland verification confirmed that clicking empty
workspace pills 4, 3, and 2 activates each requested workspace. Hyprland
rejected the legacy dispatch command and the service retried the documented
Lua focus dispatcher successfully for each activation. The user also confirmed
the visible behavior works as expected.

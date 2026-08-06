# Phase 11 — Empty Workspace Pill Activation: Tasks

**Status**: Accepted

## Pre-flight

- [x] T-104: Revalidate the empty-pill click and activation-signal path.
  - Check: reproduce the reported empty-pill failure and confirm pointer
    feedback, emitted model signal, and service command boundaries separately.
  - Result: the screenshot confirms pointer feedback; static tracing confirms
    every positive pill ID reaches the model signal, while only empty IDs rely
    solely on Hyprland IPC rather than an ext-workspace handle.

## Implementation and Tests

- [x] T-105: Add deterministic empty-pill click coverage.
  - REQs: REQ-F-01
  - Files: `tests/qml/tst_WorkspacePill.qml` and only the necessary fixture
    seam.
  - Check: an empty pill click emits its positive absolute ID once.
  - Result: the QML test invokes the actual pill `MouseArea` and observes one
    activation request for empty workspace ID 4.

- [x] T-106: Add a transport-backed activation dispatch test and outcome logs.
  - REQs: REQ-F-02
  - Files: `libs/holonight-core/src/HyprlandWorkspaceService.cpp`,
    `tests/test_hyprland_workspace_service.cpp`
  - Check: an empty/generated ID submits `dispatch workspace <id>`; queued and
    failed dispatches are observable.
  - Result: transport-backed tests cover direct and queued empty-ID dispatch.
    The live service now logs submission, completion, and failed submission or
    completion with the requested workspace ID. A legacy `error:` response
    retries the documented Lua focus dispatcher once for the same ID.

## Validation and Handoff

- [x] T-107: Run targeted and project validation.
  - Check: relevant core/QML tests, `task qml-lint`, `task qmltypes-check`, and
    `task test` pass.
  - Result: focused core/QML tests, `task test`, `task qml-lint`,
    `task qmltypes-check`, and `git diff --check` passed.

- [x] T-108: Perform live Hyprland empty-pill activation verification.
  - Steps:
    1. Keep the `task run` terminal visible.
    2. Click a visible empty workspace pill, then an occupied pill.
    3. Confirm both switch workspaces and record the matching dispatch log.
    4. Repeat on another monitor if available.
  - Result: live verification activated empty workspaces 4, 3, and 2. Each
    legacy response was rejected, the Lua dispatcher retry completed, and the
    user confirmed the expected behavior.

- [x] T-109: Update the Phase 11 handoff after acceptance passes.
  - Result: Phase 11 is accepted. The launcher hide logs observed during
    workspace changes are unrelated lifecycle no-ops when no launcher view is
    present; they are recorded as a follow-up logging-noise candidate.

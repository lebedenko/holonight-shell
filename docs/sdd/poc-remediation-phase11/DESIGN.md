# Phase 11 — Empty Workspace Pill Activation: Design

**Input**: `poc-remediation-phase11/SPEC.md`
**Baseline**: Phase 10 accepted locally, with its implementation uncommitted.

## 1. Activation Path

```
WorkspacePill MouseArea
  -> WorkspaceModel.activateWorkspace(wsId)
  -> activateWorkspaceRequested(wsId)
  -> ExtWorkspaceManager (existing handles only)
  -> HyprlandWorkspaceService (all positive IDs)
  -> Hyprland IPC: dispatch workspace <id>
```

The screenshot confirms press feedback reaches the pill. The phase therefore
tests the QML signal boundary and the service-to-transport boundary separately,
then uses a focused log at the latter boundary during live verification.

## 2. Design Decisions

### 2.1 Preserve one QML contract

`WorkspacePill` continues to call `WorkspaceModel.activateWorkspace(wsId)`.
The model signal is the shared contract for fixed, generated, occupied, and
empty numbered IDs; do not introduce state-specific QML command paths.

### 2.2 Make dispatch testable at the transport boundary

Extend the existing `HyprlandWorkspaceService` test fixture with a minimal
fake `HyprlandIpcTransport`. It records submitted commands and can complete a
command explicitly. The test verifies that a model activation for an ID absent
from the ext-workspace model still submits `dispatch workspace <id>`.

### 2.3 Log only activation failures and outcomes

Use the existing Hyprland IPC logging category. Record a warning if a dispatch
cannot be submitted or finishes unsuccessfully, including the workspace ID.
Record a low-volume info result for a successful user-requested dispatch. This
makes a live failure attributable to QML, service queueing, socket submission,
or compositor response without adding a new user-facing state.

### 2.4 Retry only a rejected legacy dispatcher

Hyprland's current Lua configuration rejects the legacy socket command
`dispatch workspace <id>` while retaining the same control socket. A socket
completion alone is therefore insufficient evidence of activation. When the
legacy response begins with `error:`, retry once with the documented Lua form:

```
dispatch hl.dsp.focus({ workspace = <id> })
```

An `ok` legacy response remains the compatibility path for older Hyprland
configurations. A failed Lua retry is reported and not retried again.

## 3. Test Strategy

| Layer | Scenario | Assertion |
|---|---|---|
| QML | Click empty pill | exactly one model activation signal carries that pill's ID |
| Core service | Activate an ID not represented by a protocol handle | fake transport receives `dispatch workspace <id>` |
| Core service | A dispatch waits behind an active command | latest dispatch is issued when the active command completes |
| Core service | Legacy dispatcher returns `error:` | Lua focus command is retried once for the same ID |
| Live Hyprland | Click empty and occupied pills | both change workspace; log has a successful dispatch outcome |

## 4. Risks

- The static code path already appears to dispatch all positive IDs; tests may
  prove that the unresolved failure is runtime transport/compositor-specific.
  The structured outcome log is therefore required before changing retry or
  fallback behavior.
- Ext-workspace activation and Hyprland dispatch intentionally both observe the
  model signal today. This phase must not remove the ext-workspace path that
  makes occupied workspaces work.

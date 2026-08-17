# Agent Window Activation — Requirements Specification

**Initiative:** `agent-notification-window-activation`
**Work package:** `ANWA-101`
**Baseline:** `b48574d39a83943cb7c6fa012ed0066bb8120399`
**Status:** Accepted for implementation

## Scope

HoloNight Shell shall provide a compositor-neutral application API that resolves a terminal window from bounded
process-lineage metadata and asks a supported compositor backend to activate that existing window. Hyprland and Sway
are supported. The generic backend rejects requests cleanly.

## Functional requirements

### SH-ACT-001: Session-bus interface

The shell shall register `org.holonight.Shell.WindowActivation1` on the shell's existing session-bus service and export:

```text
RequestWindowActivation(au processLineage, s titleHint) -> b accepted
```

The adapter shall expose no activation-completed signal and no compositor-specific types.

### SH-ACT-002: Accepted-result semantics

The reply shall be `true` only when all of the following are true at request time:

1. the input is valid and within bounds;
2. the selected backend supports window activation and is connected;
3. current backend inventory resolves exactly one target; and
4. the backend accepts the target's activation command immediately or into its bounded command queue.

`true` means accepted for delivery, not that the compositor has synchronously confirmed focus. A later IPC failure is
diagnostic state. Every rejection before command acceptance returns `false`.

### SH-ACT-003: Candidate resolution

- `processLineage` is ordered from the registered agent process to its ancestors.
- Zero is invalid. Duplicate PIDs are ignored after their first occurrence.
- A window is a PID candidate when its compositor-reported PID occurs in the lineage.
- Prefer candidates at the earliest lineage position. Candidates at later positions are considered only if no window
  exists at an earlier position.
- If exactly one candidate exists at the preferred position, select it.
- If multiple candidates exist there, a non-empty `titleHint` shall retain only candidates whose complete current title
  is exactly equal, including case, to the hint. Select only if that leaves exactly one candidate.
- A title never selects a window outside the process lineage. Empty hints do not disambiguate.
- Zero or multiple remaining candidates are respectively missing or ambiguous and return `false`.

### SH-ACT-004: Backend capabilities

| Backend | PID inventory | Stable target handle | Activation | Supported result |
|---|---:|---:|---:|---|
| Hyprland | `j/clients` PID | client address | focus by address | Yes |
| Sway | `GET_TREE` PID | container ID | focus by container criteria | Yes |
| Generic Wayland | No | No | No | Always `false` |

Capability reporting shall distinguish workspace activation from window activation.

### SH-ACT-005: Fail-safe behavior

Missing or closed windows, ambiguous matches, malformed inventory, unsupported backends, lost IPC connections, and a
full command queue shall return `false` without crashing or changing focus. A target that disappears after acceptance
may produce a diagnostic but shall not be replaced or re-resolved to a different window.

### SH-ACT-006: Input and resource bounds

- Accept at most 64 lineage entries and a `titleHint` of at most 512 UTF-8 bytes.
- Reject empty lineage, over-limit input, invalid PIDs, and strings containing NUL.
- Bound pending activation requests to one queued request per backend; a newer request shall not silently replace an
  already accepted activation request.
- Parsing and matching shall be linear in the bounded lineage and current compositor inventory.

### SH-ACT-007: No fallback launch

The shell shall never start a terminal, shell, agent, or other process to satisfy or recover a window activation
request. It shall not use title-only matching as a fallback.

## Non-goals

- Niri, KWin, labwc, or other compositor adapters.
- Generic foreign-toplevel or `xdg-activation-v1` implementation.
- Terminal spawning, workspace movement, notification routing, or notification-server specialization.
- Guaranteeing focus against compositor policy after a request has been accepted.

## Acceptance criteria

- Unit tests cover bounds, lineage priority, exact-title disambiguation, ambiguity, missing targets, capabilities,
  disconnected transports, queue rejection, and exact backend commands.
- D-Bus introspection exposes the exact method signature and maps each pre-acceptance failure to `false`.
- Manual checks activate the intended existing terminal on Hyprland and Sway and fail cleanly after it closes.

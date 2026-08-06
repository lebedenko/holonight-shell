# Phase 20 — NetworkManager Operation Error Detail: Design

**Input**: `poc-remediation-phase20/SPEC.md`
**Baseline**: Phase 19 accepted in `2b0c8c7`.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `NetworkManagerBackend.cpp` operation-failure paths | `test_network_service.cpp` session-bus fake and `operationError` assertion |

## 2. Design Decisions

### 2.1 Combine stable context with optional peer detail

Each operation already has a concise, user-facing context string. Preserve
that context and append the corresponding non-empty D-Bus error message at
the failure boundary. This gives users actionable feedback without replacing
the recognisable action description with transport jargon.

### 2.2 Capture detail before crossing the queued boundary

The D-Bus reply is local to the worker-thread lambda, whereas `emitError()`
runs on the backend's thread through a queued invocation. Build the complete
immutable error text before scheduling that invocation, then capture only the
text. This preserves the current thread-affinity model and avoids retaining a
D-Bus reply past its useful scope.

### 2.3 Cover one representative asynchronous failure through the D-Bus fake

Reuse the existing session-bus fixture and unregister the fake NetworkManager
object for one selected operation, producing a deterministic local D-Bus
failure. Assert the emitted error retains the existing context and contains
additional peer detail, without coupling the test to the bus implementation's
exact wording. The shared formatting helper then applies the same policy to
every listed failure path without requiring a live daemon.

## 3. Risks and Boundaries

- D-Bus peers may return an empty error message, so formatting must avoid a
  dangling delimiter and preserve the former generic message in that case.
- D-Bus error content is diagnostic input, not a new stable API; this phase
  neither parses it nor changes it into typed UI state.
- Do not alter local validation errors: they have no failed D-Bus reply and
  are already specific.

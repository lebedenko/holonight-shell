# Phase 22 — Bounded Portal D-Bus Probes: Design

**Input**: `poc-remediation-phase22/SPEC.md`
**Baseline**: Phase 21 accepted in `96fdc5f`.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `portal/NullPortalBackend.{h,cpp}` | `test_portal_service.cpp` session-bus delayed-reply regression and existing `PortalService` coverage |

## 2. Design Decisions

### 2.1 Keep timeout ownership at the concrete transport boundary

`PortalService` receives pending calls through `IPortalDBus` and deliberately
does not know how they are transported. Configure the timeout while each
`SystemPortalDBus` `QDBusInterface` is assembled, so all callers retain the
same abstraction and errors arrive on their existing watcher paths.

### 2.2 Use one production timeout with a narrow test seam

Define one five-second production default on `SystemPortalDBus`. Permit its
constructor to accept a shorter value and alternate portal service name for
the focused test, avoiding a five-second test delay and collision with a live
desktop portal while directly exercising Qt's timeout behavior. The seam
remains internal to the portal implementation and does not alter `IPortalDBus`
or `PortalService` APIs.

### 2.3 Test a delayed Settings reply over the session bus

Register a minimal fake Settings endpoint that receives a `Read` call but
delays its reply past the injected timeout. Assert the resulting pending call
finishes with Qt's D-Bus `NoReply` error before the delayed reply could arrive.
Existing `NullPortalDBus` tests continue to verify the service's handling of
completed success and error replies.

## 3. Risks and Boundaries

- A timeout is an error reply, not proof that a portal is absent; preserve the
  current service error paths and do not add retries or state reinterpretation.
- Setting the timeout only on `Introspect` would leave the initial ownership,
  backend-list, and appearance reads unbounded; apply the shared policy to all
  four listed calls.
- User-initiated portal requests can legitimately take longer than a probe, so
  they are deliberately outside this phase.

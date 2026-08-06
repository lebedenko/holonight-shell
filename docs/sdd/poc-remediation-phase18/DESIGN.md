# Phase 18 — Power Profiles Signal Lifecycle: Design

**Input**: `poc-remediation-phase18/SPEC.md`
**Baseline**: Phase 17 accepted in `b0c5635`.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `PowerProfilesService.cpp/.h` | `test_power_profiles_service.cpp` connect/disconnect call accounting |

## 2. Design Decisions

### 2.1 Model the successful subscription, not merely the selected service

The selected bus endpoint alone cannot distinguish a successful signal
registration from a failed one. Keep a private boolean that becomes true only
after `connectSignal()` succeeds. A small private helper disconnects only when
that state is true, using the endpoint fields that remain valid until
`clearState()` completes.

### 2.2 Disconnect at every replacement boundary

Call the helper before selecting/rebinding in `initFromService()` and before
erasing endpoint fields in `clearState()`. Either entry path is then safe:
watcher unregistration removes the old binding, and a registration event that
arrives without an observed unregistration cannot leave a stale binding.

### 2.3 Extend the existing fake rather than add D-Bus integration machinery

The current focused fake already exposes `connectSignal()` and
`disconnectSignal()`. Record their calls and assert the exact lifecycle in
unit tests, avoiding a system-bus dependency and keeping failure-path behavior
deterministic.

## 3. Risks and Boundaries

- Disconnect must use the original service, path, interface, signal, receiver,
  and slot before those fields are cleared; doing so afterward would fail to
  remove the real binding.
- A failed `connectSignal()` must not mark the subscription active, otherwise a
  later cleanup could make an unrelated disconnect call.
- This phase deliberately retains equality-gated profile setters: it removes
  redundant D-Bus deliveries without changing state semantics.

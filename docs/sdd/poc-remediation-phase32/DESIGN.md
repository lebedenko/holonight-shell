# Phase 32 — MIME Cache Update Detach Avoidance: Design

**Input**: `poc-remediation-phase32/SPEC.md`
**Baseline**: Phase 31 accepted in `3c76ca4`.
**Status**: Complete — automated checks and user verification passed.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | Centralize MIME cache mutation and capture resolved role values before the one-key update | Existing MIME-service default and stale-query tests; focused signal-regression coverage as needed |

## 2. Design Decisions

### 2.1 Compare observable role values, not the cache container

The existing implementation copies `mime_cache_` to supply the pre-update
side of `emitChangedSignals()`. Because the following `operator[]` mutation
then sees a shared `QHash`, Qt detaches the full container. Instead, calculate
each role's resolved default before changing the one cache entry, mutate the
entry, then compare each current resolved role value with its saved value.

This retains the actual API contract—whether each role-default property
changed—without preserving data that callers cannot observe.

### 2.2 Use one mutation-and-notification helper

`onQueryResult()` and the successful branch of `setDefaultAsync()` have the
same snapshot/mutate/notify sequence. Move that sequence into one private
`MimeService` helper so both paths preserve the same no-detach behavior and
signal semantics.

### 2.3 Preserve current resolution work and scope

The helper continues to check every tracked role, as `emitChangedSignals()`
does today. This phase removes the cache copy; it deliberately does not change
role resolution rules or introduce a more speculative affected-role index.

## 3. Risks and Boundaries

- A missed role comparison could suppress a QML update. Existing default-role
  and stale-query tests, supplemented where necessary by signal assertions,
  cover this observable contract.
- Resolving role values before and after mutation preserves the current
  behavior for roles containing multiple MIME types, including their
  first-non-empty precedence.
- This is a local performance cleanup; it does not warrant live compositor
  verification.

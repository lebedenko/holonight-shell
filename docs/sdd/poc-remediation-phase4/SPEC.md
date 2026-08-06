# POC Remediation Phase 4 SPEC

## Overview

Phase 4 addresses five high-severity unit-level findings from the POC Readiness Review (docs/sdd/poc-readiness-review/REPORT.md §4, §7). These findings cluster around two themes: (1) audio subsystem failure modes and device index correctness in services, and (2) data-lifecycle gaps in notifications and calendar persistence, plus a critical QML bug affecting launcher usability. Unlike Phase 3's cross-cutting infrastructure consolidation or Phase 1's systemic convention-establishment, Phase 4 items are self-contained service-layer and QML repairs with no inter-item dependencies.

**Key goals:**
- Restore PulseAudio reconnection after context failure, preventing permanent audio stalenesss.
- Separate sink and source removal signals, eliminating index-namespace collisions.
- Migrate notification-rule persistence to async-write with failure signaling.
- Reconcile calendar cache with sync results; wire the existing account-removal API into production paths.
- Fix launcher keyboard-selection dispatch and visual highlight.

Verification is GTest-only for items 1–4 (C++ services); item 5 (QML launcher) uses QML TestCase with equivalent falsifiable assertions. No live-compositor smoke tests are required for acceptance.

---

## 1. PulseAudio Context Failure and Reconnect (U-04 [F-01])

### Context

`AudioService` and its PulseAudio backend wrap the libpulse C API via a `pa_threaded_mainloop` (reviewed and verified thread-correct in U-04). When a PulseAudio context fails (server disconnection, permission loss, server crash) or is explicitly terminated, the context callback is invoked but no reconnection logic follows — the service silently transitions to a stale, non-functional state. Audio operations (volume queries, device lists) continue to reference dead context pointers and fail invisibly. Shell restart is the only recovery path.

The fix requires bounded exponential backoff on reconnect attempts: 1s → 2s → 4s → 8s → 16s → 30s (cap), with a maximum retry count (chosen during Design stage; recommend 6–8 attempts) before giving up and surfacing a persistent `healthState` signal so the UI can display degraded status.

### Requirements

**REQ-F-001: Detect PulseAudio context failure and loss**

*When* PulseAudio context callback indicates `PA_CONTEXT_FAILED` or `PA_CONTEXT_TERMINATED`, *the system* shall record the failure and initiate a bounded reconnection sequence instead of silently consuming the callback.

- **Acceptance:** A unit test injects a mock PulseAudio context that fires `PA_CONTEXT_FAILED`; verifies that `AudioService` records the failure state (via `QSignalSpy` on a `contextLost()` signal or by inspecting `isConnected()` property transitioning to false); and confirms that a reconnection timer is armed. Document the expected state transition as a falsifiable property check.

---

**REQ-F-002: Bounded exponential backoff on reconnect attempts**

*While* attempting to reconnect to PulseAudio after context failure, *the system* shall space reconnection attempts using exponential backoff (1s, 2s, 4s, 8s, 16s, 30s cap) rather than retrying immediately or indefinitely.

- **Acceptance:** A test injects a mock context that fails on the first three reconnection attempts, then succeeds on the fourth. Spy on the `connect()` call timestamps; measure the elapsed time between consecutive attempts; verify that delays increase exponentially (within ±200ms jitter tolerance) until the 30s cap is reached. Use `std::chrono::high_resolution_clock` to measure timing.

---

**REQ-F-003: Reconnect attempt ceiling and health-state signal**

*If* PulseAudio reconnection fails after a maximum number of attempts (determined in Design stage, recommend 6–8), *the system* shall stop retrying and emit a `healthState()` signal indicating `Failed` or `Disconnected` so the UI can display persistent error state and avoid misleading silence.

- **Acceptance:** A test injects a mock context that never succeeds on reconnection; runs `AudioService`; waits for the retry ceiling (e.g., 8 attempts); verifies via spy that `healthState(HealthState::Failed)` (or equivalent enum) signal is emitted exactly once; confirms that no further reconnection attempts fire after the ceiling is reached (spy count remains at 8, not 9+).

---

**REQ-F-004: Successful reconnection recovery**

*If* a reconnection attempt after a prior failure succeeds, *the system* shall reset the attempt counter and the `healthState` to `Connected`, allowing the service to return to normal operation.

- **Acceptance:** A test sequence: (1) inject failure on first connect, spy confirms `healthState(Failed)`; (2) inject success on a later reconnection attempt; (3) verify `healthState(Connected)` emitted; (4) query properties like `devices()`, `currentVolume()` — confirm they return valid, non-stale values, not cached failure state.

---

**REQ-NF-001: Reconnect preserves existing device/volume state**

*The system* shall not modify `AudioService`'s existing property getters (`devices()`, `currentVolume()`, `isMuted()`) to return `QString`-based error messages or sentinel values; they shall continue returning their native types (list, float, bool), with stale/invalid data acceptable during a failed state, as the calling code already handles this.

- **Acceptance:** Existing GTests exercising `devices()` and volume getters pass without modification. No changes to return types or NOTIFY signal lists. A quick grep confirms no callers expect error-string returns from these getters.

---

## 2. Sink/Source Index Separation (U-04 [F-06])

### Context

PulseAudio maintains two independent index spaces: sink indices and source indices. `AudioService` collapses both into a single, untyped device-removal signal (`deviceRemoved(uint32_t)`), losing the information of which type (sink vs. source) was removed. When a sink is removed, the removal handler can silently delete a still-connected source from its internal tracking (e.g., a working microphone), causing phantom device losses. The fix requires splitting the single signal into two distinct, typed signals, and updating all QML consumers.

### Requirements

**REQ-F-005: Dual removal signals for sink and source**

*When* PulseAudio notifies `AudioService` of a sink or source removal, *the system* shall emit one of two distinct, typed signals: `sinkRemoved(uint32_t index)` for sink removal, `sourceRemoved(uint32_t index)` for source removal, instead of the current untyped `deviceRemoved(uint32_t)`.

- **Acceptance:** A unit test injects PulseAudio callbacks with both sink-removal and source-removal scenarios; spies on both `sinkRemoved` and `sourceRemoved` signals; verifies:
  1. Sink removal fires `sinkRemoved(42)`, not `sourceRemoved` or generic `deviceRemoved`.
  2. Source removal fires `sourceRemoved(99)`, not `sinkRemoved` or generic `deviceRemoved`.
  
  Use distinct, non-overlapping indices to ensure no cross-contamination. Verify via `EXPECT_CALL(..., sinkRemoved(42)).Times(Exactly(1))` + `EXPECT_CALL(..., sourceRemoved(_)).Times(0)` for the first scenario.

---

**REQ-F-006: Update AudioDeviceModel to track sinks and sources separately**

*When* a sink or source removal signal is emitted, *the system* shall update `AudioDeviceModel` (or its QML consumers, identified via grep) to handle the two distinct signals independently, removing only the matching type from its internal tracking.

- **Acceptance:** A test constructs `AudioDeviceModel` with mock data (2 sinks, 2 sources); injects a sink-removal callback; verifies that the model's sink count decrements but source count remains unchanged (via `rowCount()` or a mock-intercepted `removeRow()` call specific to sinks). Repeat for source removal.

---

**REQ-F-007: Deprecate and remove old deviceRemoved signal**

*After* all QML consumers (identified via grep in Phase 4's Design stage) have been migrated to the new dual-signal pattern, *the system* shall remove the old `deviceRemoved(uint32_t)` signal declaration from `AudioService` to prevent accidental re-use.

- **Acceptance:** Grep the entire `apps/shell/qml/` directory for string `deviceRemoved` (excluding comments); a script finds zero references to the old signal by end of Phase 4 implementation. The signal is removed from `AudioService.h`. Any QML file that previously connected to it is updated to the new signals.

---

**REQ-NF-002: Existing AudioStreamModel index handling unchanged**

*The system* shall not modify `AudioStreamModel` or the per-stream volume/mute operations; the stream model's internal index tracking (distinct from sink/source indices) shall remain unaffected by this separation.

- **Acceptance:** Existing GTests for `AudioStreamModel` pass without modification. A grep confirms no references to `deviceRemoved` signal appear in stream-model-related code.

---

## 3. Notification Rule Persistence Async Write (U-06 [F-01])

### Context

`NotificationRuleModel` persists filtering rules to disk on every incoming notification (even DND-suppressed ones) via a synchronous, blocking write. This is the single persistence path in the notifications subsystem that never adopted the async-write pattern its sibling store (e.g., notification-history or a similar persistent-state class) already uses. Silent write failures are invisible to the user, so a rule save can fail without any diagnostic signal. The fix requires migrating to async-write and emitting a failure signal using the Phase 1 convention (see `SessionCommandResult.h` and `SessionService::commandFailed(QString action, QString reason)`).

### Requirements

**REQ-F-008: Migrate rule persistence to async write**

*When* `NotificationRuleModel` updates are persisted to disk, *the system* shall use non-blocking async write (matching the established async-write pattern in the sibling store), not a synchronous blocking call.

- **Acceptance:** A test invokes rule-update logic; the call returns immediately (verified via wall-clock timer: <10ms roundtrip, not blocking for the full disk I/O latency); a separate spy tracks the async write completion callback. Verify the callback fires after the async write finishes (indicating proper decoupling).

---

**REQ-F-009: Emit rulePersistenceFailed signal on write failure**

*If* rule persistence to disk fails (file permissions, disk full, etc.), *the system* shall emit a `rulePersistenceFailed(QString action, QString reason)` signal (following the Phase 1 convention documented in `SessionCommandResult.h`) so the failure is observable and not silently swallowed.

- **Acceptance:** A test injects a fake filesystem that fails write operations; updates a rule; verifies via `QSignalSpy` that `rulePersistenceFailed("updateRule", "<reason>")` is emitted exactly once, with a non-empty reason string.

---

**REQ-F-010: Preserve rule update semantics during async migration**

*The system* shall preserve identical rule-update semantics (order of application, deduplication, conflict resolution) before and after migration to async write; the only visible change shall be that writes no longer block the caller.

- **Acceptance:** A regression test exercises the same rule-update scenarios (add, remove, modify) before and after the async migration; captures the persisted rule state (either via snapshot before/after or by reading the file after async completion); verifies identical output (order, values, no lost updates).

---

**REQ-NF-003: Sibling async-write pattern consistency**

*The system* shall reuse the async-write implementation pattern already proven in the sibling notification store (identified during Design stage via codebase search), not invent a new async mechanism.

- **Acceptance:** The implementation reuses the existing `QFutureWatcher` / `QtConcurrent::run()` or similar pattern already in place elsewhere. Grep confirms the pattern appears in ≥1 other notification-persistence class. Zero new async-abstraction classes introduced.

---

## 4. Calendar Cache Reconciliation and Account Removal (U-07 [F-02]/[F-03])

### Context

**[F-02]**: `CalendarCache` persists events indefinitely, never deleting those absent from a fresh sync result. Cancelled meetings remain in the cache and continue firing pre-event notifications. **[F-03]**: The cache's account-removal cleanup API (`removeStaleAccounts`, `clearAccountEvents`) is fully implemented with doc comments, but has zero production callers. A user who removes a CalDAV account sees its events persist forever.

Additionally, U-07 notes that `onCalendarConfigChanged()` may silently ignore config-change signals after the first (an observation flagged in the REPORT as meriting Design-stage investigation and explicit scoping decision, not silent assumption).

### Requirements

**REQ-F-011: Reconcile cache after sync: delete missing events**

*After* a successful calendar sync for a given account/calendar, *the system* shall compare the fresh event list against the cached events for that account/calendar and delete any cached events absent from the fresh result.

- **Acceptance:** A test:
  1. Populates the cache with events {A, B, C} for calendar "Work".
  2. Runs a sync that returns events {A, C} (B was cancelled).
  3. Queries the cache for "Work" events; verifies only {A, C} remain.
  4. Checks that event B is *not* in the cache and will not fire notifications.
  
  Use a mock CalDAV provider returning a specific event list; verify via cache query or `QSignalSpy` on event-removal operations.

---

**REQ-F-012: Wire account removal into config-change path**

*When* a CalDAV account is removed (detected via `onCalendarConfigChanged()` or an analogous config-change signal), *the system* shall invoke `CalendarCache::clearAccountEvents(account_id)` to clean up stale events for that account.

- **Acceptance:** A test:
  1. Registers an account in calendar config with events in the cache.
  2. Triggers a config-change notification removing that account.
  3. Verifies that `clearAccountEvents(account_id)` is called (via mock spy).
  4. Queries the cache; confirms no events from that account remain.
  5. (Optional) Manually inspect `CalendarService::onCalendarConfigChanged()` source to confirm the wiring is present.

---

**REQ-F-013: Flag onCalendarConfigChanged() repeated-signal handling for Design review**

*The system* shall document (as an inline comment with a reference to this requirement) the **Design-stage finding** that `onCalendarConfigChanged()` may silently ignore signals after the first, and explicitly note: (a) whether this behavior is intentional (caching after first call), (b) whether it blocks Item F-012's wiring (does the second account-removal change fire the signal?), or (c) whether it is a latent bug requiring separate remediation. This comment serves as a flag for Design review and does not require code changes in Phase 4 Implementation; Design must make an explicit decision on this point.

- **Acceptance:** At least one inline code comment (minimum 2 lines) appears in `CalendarService::onCalendarConfigChanged()` or a related location documenting the observed behavior, the question, and a reference to "POC Remediation Phase 4 REQ-F-013 (SPEC.md)". A Design-stage review of the code and REPORT context shall examine this flag and file the decision (intentional, bug fix needed, or out-of-scope) in the Phase 4 DESIGN.md.

---

**REQ-NF-004: Sync-path performance unchanged**

*The system* shall not degrade sync performance (latency, memory, CPU) with the reconciliation logic; the comparison and deletion operations shall be efficient enough to complete within the existing sync timeout bounds (e.g., <1s for typical cache sizes).

- **Acceptance:** A performance test with a realistic cache (500+ events, 10+ calendars) runs a sync and measures reconciliation overhead; verifies that the additional logic adds <100ms. If latency is higher, optimize (e.g., use a set intersection instead of O(n²) comparison).

---

## 5. Launcher Keyboard-Selection Dispatch and Highlight (U-10 [D-001])

### Context

In launcher search mode, keyboard-selected action rows (e.g., "Open in Terminal", "Open in …") have no visual highlight and pressing Enter always launches the parent app's default command, never the selected action's command. This is a silent wrong-command-launch bug with zero visual warning. The fix requires two parallel changes: (1) route the Enter-key dispatch to the actually-selected action's command, and (2) add a visual highlight state matching existing launcher selection visuals.

### Requirements

**REQ-F-014: Keyboard-selected action Enter dispatch fix**

*When* a user navigates to an action row in launcher search results using keyboard and presses Enter, *the system* shall invoke the *selected action's* command, not the parent app's default command.

- **Acceptance:** A QML TestCase:
  1. Mocks the launcher with a test app "MyApp" (default command: "open-app"), with an action "Open in Editor" (command: "xdg-open %u").
  2. Simulates keyboard navigation to the action row.
  3. Simulates pressing Enter.
  4. Verifies via a spy on the command-invocation mock that "xdg-open %u" was called, not "open-app".
  
  Use `mouseDoubleClick` + keyboard navigation to reach the action row, or directly set focus if the test framework supports it. Verify the invocation via a Mockable/spy function passed to the launcher.

---

**REQ-F-015: Keyboard-selected row visual highlight**

*When* a user navigates to an action row in launcher search results using keyboard, *the system* shall apply a visual highlight state (background color, border, or opacity change) to that row, matching the existing visual language for mouse-hover or active selection used elsewhere in the launcher.

- **Acceptance:** A QML TestCase:
  1. Loads the launcher's search-results delegate component.
  2. Simulates keyboard focus on an action row.
  3. Queries the visual state: checks that a highlight property (e.g., `highlighted: true`, or a color property transitioned to the "selected" palette token) is true or matches the expected color.
  4. Simulates keyboard focus away; verifies the highlight clears.
  
  Compare the highlight appearance to an existing mouse-hover state in the same file to ensure consistency. Document the visual token used (e.g., `HoloniightPalette.selectionBackground`) in the test comment.

---

**REQ-F-016: Existing browse-mode selection unchanged**

*The system* shall not modify launcher browse mode (non-search result rows) selection behavior or visuals; only search-mode action rows are affected.

- **Acceptance:** Existing QML tests for browse-mode selection (if any) pass without modification. A manual visual inspection in live session confirms browse-mode selection/highlight is unchanged (no regression). This is a low-risk assert given the changes are scoped to search-mode action row handling.

---

**REQ-NF-005: Action row visual identity preservation**

*The system* shall not change the action row's font, size, or icon rendering; the highlight shall be applied via background/border/opacity only, not font changes.

- **Acceptance:** The visual appearance of text and icons in a highlighted action row remains identical to unhighlighted state, except for the applied highlight layer. A grep/inspection confirms no changes to `Text` component properties like `font.pixelSize`, `color` (except via ColorAnimation or palette token), or icon sources.

---

## Non-Goals

The following items are **explicitly out of scope** for Phase 4 and are reserved for future phases or explicit deferral:

1. **PulseAudio volume/mute callback hardening (U-04 [F-02]):** While [F-01] addresses context failure, [F-02] (null callbacks on mutation calls) is a separate issue requiring callback wiring changes; it is deferred to Phase 4+ as a distinct item, not bundled with [F-01].

2. **Session-lock command failure convention (U-04 [F-03]):** Already resolved in `poc-remediation-phase1` via `SessionCommandResult` and `SessionService::commandFailed(QString action, QString reason)`; not revisited here. Phase 4's notification-rule failure signal (REQ-F-009) reuses that established convention rather than inventing a new one.

3. **Launcher cache invalidation improvement (U-06 [F-03]):** The launcher cache's mtime-truncation and directory-watch gaps are out of scope; Phase 3's REQ-F-014/F-015 address the redundant scan problem via caching, not by improving the watcher.

4. **UI redesign of the launcher:** Only the keyboard-selection dispatch and highlight are fixed; no broader launcher UI redesign, menu reordering, or icon changes.

5. **PulseAudio stream moves (U-04 [F-07]):** The sink/source index bug also affects stream moves; Phase 4 fixes the removal signals and device model only. Stream move dispatch is deferred as a follow-on item.

6. **Live-compositor smoke testing:** Phase 4 acceptance criteria are GTest (for C++ services) and QML TestCase-only. No live Hyprland environment testing, no manual launcher interaction, no screenshot validation.

7. **Broader Gap #1 silent-failure-convention rollout:** Only the two highest-priority instances (audio reconnect health-state, notification-rule failure signal) receive the convention-pattern implementation in Phase 4; U-04 [F-03], U-07 [F-01] (already fixed in Phase 0), and other silent-failure instances await Phase 1's systematic rollout.

---

## Verification Strategy

**GTest + QML TestCase Approach:**

All acceptance criteria in this SPEC map directly onto testable assertions:

- **Call-count verification:** `EXPECT_CALL(mock, Method()).Times(Exactly(N))` or via `QSignalSpy::count()`.
- **Timing bounds:** `EXPECT_LT(elapsed_ms, timeout_ms + jitter)` using `std::chrono::high_resolution_clock` (for audio backoff) or `QElapsedTimer` (for quick checks).
- **Signal emission:** `QSignalSpy` verifying signal type, count, and arguments.
- **State-flag checks:** Direct property inspection or `QSignalSpy` on `NOTIFY` signals.
- **Index/type correctness:** Mock PulseAudio callbacks with distinct indices; verify matching signal emissions.
- **QML visual state:** `TestCase::findChild()` + property inspection to verify `highlighted` or equivalent, or direct evaluation of QML expressions in the test.
- **Async behavior:** Fakes/mocks that delay responses; wall-clock timer assertions confirm non-blocking return.

**For Item 5 (Launcher QML):**

Tests are written as `TestCase` components under `tests/qml/tst_*.qml` and run via the `test_holonight_qml_harness` target with CTest, following the project's established QML test convention. The harness instantiates mocks (command invocation spies, property setters) at test setup.

**No Smoke Tests:**

This phase does NOT require:
- Launching Hyprland or any live Wayland compositor.
- Manually opening the sidebar, toggling widgets, or observing audio device switches.
- Live PulseAudio server interactions (all tests use mocks).
- Live calendar sync to a CalDAV server.
- Manual launcher interaction or search-mode navigation.

**Test Infrastructure:**

- C++ tests use GTest (Google Test) with Qt's `QSignalSpy`, `QTimer`, and mock/fake PulseAudio callback injection.
- QML tests use Qt's `TestCase` framework with mockable invokables for command dispatch.
- No external mocking library beyond Qt's built-in testing utilities.

---

## Summary

**5 scope items, 21 requirements:**

| Category | Count | IDs |
|----------|-------|-----|
| Functional | 16 | REQ-F-001 to REQ-F-016 |
| Non-Functional | 5 | REQ-NF-001 to REQ-NF-005 |
| **Total** | **21** | |

Each requirement has a distinct, falsifiable acceptance criterion phrased as a GTest assertion, `QSignalSpy` verification, timing check, or QML `TestCase` property inspection. No implementation or design details appear in this SPEC (reserved for Stage 2, Design). The seven non-goals are clearly listed to prevent scope creep and clarify deferral boundaries.

# SDD Tasks — poc-remediation-phase4

## Item 1 — PulseAudio Context Failure and Reconnect

- [x] T-001: Implement PulseAudio reconnect infrastructure (enum, signals, helpers, timer)
  - REQs: REQ-F-001, REQ-NF-001
  - Check: `AudioTypes.h` defines `enum class AudioHealthState : uint8_t { Connected, Reconnecting, Failed }`; `PulseAudioBackend.h` declares `sinkRemoved(uint32_t)`/`sourceRemoved(uint32_t)` signals, `healthStateChanged(AudioHealthState)` signal, `onContextLost()`/`onReconnectSucceeded()`/`attemptReconnect()` private slots, `reconnect_attempt_`/`reconnect_timer_`/`health_state_` member fields, and static `setReconnectBackoffScheduleForTests()`/`resetReconnectBackoffSchedule()` test seams; `PulseAudioBackend.cpp` implements `Impl::teardownContext()` and `Impl::connectNewContext()` helpers.

- [x] T-002: Implement reconnect scheduling and exponential backoff
  - REQs: REQ-F-002, REQ-F-003
  - Check: `scheduleReconnect()` method implements exponential backoff schedule `{1000,2000,4000,8000,16000,30000}` ms with ceiling at `kMaxReconnectAttempts` (8), `attemptReconnect()` calls `teardownContext()`/`connectNewContext()` on timer fire, re-entrancy guard via `reconnect_timer_->isActive()` check in `onContextLost()`, `PulseAudioBackend::stop()` halts `reconnect_timer_` as first statement before freeing mainloop.

- [x] T-003: Implement successful reconnection recovery and AudioService healthState property
  - REQs: REQ-F-004, REQ-NF-001
  - Check: `onReconnectSucceeded()` resets `reconnect_attempt_ = 0` and calls `setHealthState(Connected)`, `onContextLost()` calls `setHealthState(Reconnecting)` on first failure (not at ceiling), `AudioService::healthState()` property returns `int` and emits `healthStateChanged()` NOTIFY signal when set via `setHealthState(AudioHealthState)`, existing `devices()`/`currentVolume()`/`isMuted()` getter signatures and `NOTIFY` lists unchanged.

- [x] T-004: GTest PulseAudioBackend context failure detection and backoff timing
  - REQs: REQ-F-001–003
  - Check: Inject mock `PA_CONTEXT_FAILED` callback; spy on `healthStateChanged` signal observing `Reconnecting` emission; inject millisecond-scale test schedule via `setReconnectBackoffScheduleForTests({10,20,40,80,160,300})`; measure reconnect-call timestamps; verify delays increase (each ≈2× previous); assert no calls beyond `kMaxReconnectAttempts`; verify `healthStateChanged(Failed)` emitted exactly once after ceiling reached.

- [x] T-005: GTest reconnect success recovery and existing audio-API preservation
  - REQs: REQ-F-004, REQ-NF-001
  - Check: Inject context failure on first connect, second reconnect succeeds; spy confirms `healthStateChanged(Failed)` then `healthStateChanged(Connected)` sequence; query `devices()`, `currentVolume()` after recovery, confirm valid non-stale values; existing `test_pulse_audio_backend.cpp` and `test_audio_service.cpp` audio-getter tests pass unmodified.

---

## Item 2 — Sink/Source Index Separation

- [x] T-006: Split device-removal signals and update AudioService handlers
  - REQs: REQ-F-005, REQ-F-006, REQ-F-007
  - Check: `PulseAudioBackend::Impl::handleSinkEvent()` emits `sinkRemoved(idx)` (not `deviceRemoved`) on `REMOVE` branch; `handleSourceEvent()` emits `sourceRemoved(idx)`; old `deviceRemoved(uint32_t)` signal removed from `.h`; `AudioService::start()` connects `sinkRemoved` to `onSinkRemoved()` (calls `outputs_->applyRemove(idx)` only) and `sourceRemoved` to `onSourceRemoved()` (calls `inputs_->applyRemove(idx)` only); grep of `apps/shell/qml/` confirms zero references to old `deviceRemoved` signal (already vacuous, no QML migration needed).

- [x] T-007: GTest sink/source signal separation and AudioDeviceModel tracking
  - REQs: REQ-F-005–007, REQ-NF-002
  - Check: Backend test: inject sink-removal callback (index 42), spy on `sinkRemoved`/`sourceRemoved`, assert `sinkRemoved(42)` exactly once, `sourceRemoved` zero times; repeat for source (index 99); Service test: construct `AudioDeviceModel` with 2 sinks/2 sources, call `onSinkRemoved(idx)`, verify sink `rowCount()` decrements and source count unchanged; existing `test_audio_stream_model.cpp` passes unmodified.

---

## Item 3 — Notification Rule Persistence Async Write

- [x] T-008: Implement NotificationRuleStore async-write infrastructure with dirty-flag coalescing
  - REQs: REQ-F-008, REQ-F-010, REQ-NF-003
  - Check: Add `RulePersistOutcome{bool ok; QString reason;}` struct to `.h`; `persist(const QList<AppNotificationRule>&, const QString& action)` signature (no longer `const`); fields `write_in_flight_`, `write_dirty_`, `pending_rules_`, `pending_action_`, `in_flight_action_`, `QFutureWatcher<RulePersistOutcome>* watcher_`; `launchWrite()` dispatches `QtConcurrent::run(writeRulesToDisk)` to watcher; `onWriteFinished()` clears in-flight flag, emits `writeCompleted()`, re-launches if dirty flag is set (coalescing pattern identical to `NotificationStore`).

- [x] T-009: Implement rule-persistence failure signaling and NotificationRuleModel action labels
  - REQs: REQ-F-009
  - Check: `NotificationRuleStore::onWriteFinished()` emits `persistFailed(in_flight_action_, result.reason)` if `!result.ok`; `writeRulesToDisk()` returns `RulePersistOutcome{ok=false, reason=...}` on `QSaveFile` open/commit failure instead of only logging; `NotificationRuleModel` adds `rulePersistenceFailed(QString action, QString reason)` signal; all four `persist()` call sites pass distinct action labels (`"ensureApp"`, `"setEnabled"`, `"setUrgencyFilter"`, `"pruneRules"`); model's `loadPersistedRules()` connects `store_->persistFailed` to `this->rulePersistenceFailed` (signal-to-signal forwarding).

- [x] T-010: Wire ShellApplication composition-root failure notifications
  - REQs: REQ-F-009
  - Check: `ShellApplication::connectNotificationRuleFailureNotifications()` private method implemented; connected from `startServices()` after existing `connectSessionFailureNotifications()` call; lambda captures rule-model and notification-server, calls `notification_server_->Notify(...)` with formatted failure message (`"Notification rule save failed: %1".arg(action)`); rule-failure toasts appear to user matching Phase 1's established convention.

- [x] T-011: Update tests and add async-persistence GTests
  - REQs: REQ-F-008–010, REQ-NF-003
  - Check: `tests/test_notification_rules.cpp`: existing `WritesExpectedJson` test drops `const` from store declaration, adds `QSignalSpy(store, &NotificationRuleStore::writeCompleted)` before `persist()`, waits `spy.wait()` before asserting file contents (matching `test_notification_history.cpp` idiom); new GTest injects write-permission failure via `QFile::setPermissions()`, verifies `rulePersistenceFailed` emission with non-empty reason exactly once; new test exercises coalescing (rapid persist calls produce single file write); all existing rule-model tests pass unmodified.

---

## Item 4 — Calendar Cache Reconciliation and Account Removal

- [x] T-012: Implement CalendarCache reconciliation and wire into sync-finished path
  - REQs: REQ-F-011, partial REQ-NF-004
  - Check: `CalendarCache::reconcileAccountEvents(provider_type, account_name, fresh_events)` method added; inside `CalendarSyncManager::onSyncFinished()`, call `cache_.reconcileAccountEvents(...)` after `upsertEvents()` and before `pruneExpired()`; reconciliation queries cached events for the account, diffs against fresh result in C++ (using `QSet<QString>` membership for O(n) complexity), deletes stale cached events via `DELETE ... WHERE uid = ?` in one transaction; performance test confirms <100ms for 500+ events across 10+ accounts.

- [x] T-013: Implement account-removal infrastructure and restructure onCalendarConfigChanged (REQ-F-012 and REQ-F-013 coupled)
  - REQs: REQ-F-012, REQ-F-013
  - Check: `CalendarSyncManager::removeAccount(provider_type, account_name)` calls `cache_.clearAccountEvents(...)`, removes entry from `in_progress_`/`backoff_` by account_name, calls `refreshModel()` (existing state cleanup, provider vector untouched per scope); `CalendarService` adds `known_account_keys_` QSet member tracking "provider_type:account_name" composite keys; `onCalendarConfigChanged()` restructured to run diff logic on *every* call (not just when `sync_manager_ == nullptr`), detects removed keys, calls `sync_manager_->removeAccount(...)` for each; inline comment documents the REQ-F-013 finding: "this method previously only acted on the FIRST call... this was a genuine latent bug, not intentional caching — see POC Remediation Phase 4 DESIGN.md Item 4 Scope boundary."

- [x] T-014: GTest calendar reconciliation and account-removal scenarios
  - REQs: REQ-F-011–013, REQ-NF-004
  - Check: Cache-level unit test: `upsertEvents({A,B,C})`, `reconcileAccountEvents("caldav","work",{A,C})`, `queryRange` confirms only `{A,C}` remain; Sync integration test: two `CalendarSyncManager` instances on same DB (first sync returns `{A,B,C}`, second returns `{A,C}`), verify cached events reconciled end-to-end; Account-removal test: pre-populate cache, trigger config change removing an account, assert `clearAccountEvents` called (via direct method call or via spy), verify cache query shows zero events for that account afterward; performance test: 500+ events, 10+ synthetic accounts, measure `reconcileAccountEvents` duration, assert <100ms.

---

## Item 5 — Launcher Keyboard-Selection Dispatch and Highlight

- [x] T-015: Implement LauncherModel accessors and fix LauncherService dispatch
  - REQs: REQ-F-014, partial REQ-NF-005
  - Check: `LauncherModel::isActionRow(int row)` and `actionIndexAt(int row)` const accessors added; `LauncherService::launchSelected()` refactored to check `model_.isActionRow(selected_index_)` and dispatch to `launchAction(selected_index_, model_.actionIndexAt(selected_index_))` for action rows, or `launch(selected_index_)` for app rows; browse-mode logic unchanged (all browse-mode rows have `is_action = false` by construction).

- [x] T-016: Add LauncherActionRow selected property and highlight visual state
  - REQs: REQ-F-015, REQ-NF-005
  - Check: `LauncherActionRow.qml` adds `property bool selected: false`; two `Rectangle` elements added (background at 0.10 opacity and border frame at 0.65 opacity, matching `LauncherResultRow.qml`'s existing selected-state colors), positioned before the existing mouse-hover `Rectangle` in paint order (hover renders on top when both active); colors/opacity driven by `root.selected`; text/icon properties unchanged.

- [x] T-017: Wire Launcher binding and add dispatch+highlight verification tests
  - REQs: REQ-F-014–016, REQ-NF-005
  - Check: `Launcher.qml` binds `LauncherActionRow { selected: searchDelegate.index === LauncherService.selectedIndex }` (identical to sibling appRow binding two lines above); GTest in `tests/test_launcher_service.cpp`: construct `LauncherModel` with entry that has one action, set `selected_index_` to action's flat row, call `launchSelected()`, verify `FakeLauncherBackend::last_launch_exec_` matches action's exec, not app's default; new QML TestCase `tests/qml/tst_LauncherActionRow.qml`: instantiate component, toggle `selected`, use `TestCase.findChild()` to verify highlight `Rectangle` opacity matches expected values (0.10/0.65); browse-mode selection tests (`SelectionClampsToResultBounds`, etc.) pass unmodified.

---

## Summary

**17 tasks covering all 21 requirements:**

| Item | Count | Task IDs | Requirement IDs |
|---|---|---|---|
| 1. Audio Reconnect | 5 | T-001–T-005 | REQ-F-001–004, REQ-NF-001 |
| 2. Sink/Source Separation | 2 | T-006–T-007 | REQ-F-005–007, REQ-NF-002 |
| 3. Notification Async Persistence | 4 | T-008–T-011 | REQ-F-008–010, REQ-NF-003 |
| 4. Calendar Reconciliation | 3 | T-012–T-014 | REQ-F-011–013, REQ-NF-004 |
| 5. Launcher Keyboard Selection | 3 | T-015–T-017 | REQ-F-014–016, REQ-NF-005 |
| **Total** | **17** | **T-001–T-017** | **All 21** |

**Dependency and sequencing notes:**
- All 5 items are independent across-item; may be implemented in any order or in parallel.
- **Item 4 intra-item coupling**: T-013 and T-014 together constitute the inseparable REQ-F-012 and REQ-F-013 fix (the `onCalendarConfigChanged()` restructuring is one change satisfying both); T-012 (reconciliation) is independent and can land first.
- **Composition-root wiring**: T-010 (Item 3) is the dedicated task for `ShellApplication::connectNotificationRuleFailureNotifications()` per DESIGN.md.
- **Test infrastructure**: All tests reuse existing GTest fixtures (`FakePulseAudioSystem`, `FakeLauncherBackend`, `FakeCalendarProvider`), `QSignalSpy`, `QElapsedTimer`, and QML `TestCase` — no new external mocking library or live-compositor requirements.

All requirements map to at least one task; each task is independently verifiable via unit test assertions and integration test scenarios documented above.

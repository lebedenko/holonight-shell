# POC Remediation Phase 4 — Design

Stage 2 (Design) for `docs/sdd/poc-remediation-phase4/SPEC.md`. Satisfies REQ-F-001 through
REQ-F-016 and REQ-NF-001 through REQ-NF-005. Five scope items — PulseAudio reconnect, sink/source
signal separation, notification-rule async persistence, calendar cache reconciliation +
account-removal wiring, launcher keyboard-selection dispatch/highlight. SPEC's Overview frames all
five as independent; §8 (Sequencing) confirms that holds **across** items and flags one **intra-item**
coupling discovered during design (REQ-F-012 and REQ-F-013 inside Item 4 are not separable).

Grounded against the actual current source: `libs/holonight-services/src/audio/`,
`libs/holonight-services/src/notifications/`, `libs/holonight-services/src/calendar/`,
`libs/holonight-services/src/CalendarService.{h,cpp}`, `apps/shell/qml/Launcher/`, and the Phase 1
(`SessionCommandResult`/`SessionService::commandFailed`) and Phase 3 (`GuardedProcessRunner`,
`QtConcurrent`/`QFutureWatcher` patterns) precedents this design reuses or deliberately diverges from.

---

## Component Map

| Item | File(s) touched | File(s) added |
|---|---|---|
| 1 | `libs/holonight-services/src/audio/PulseAudioBackend.{h,cpp}`, `AudioTypes.h`, `AudioService.{h,cpp}` | — |
| 2 | `libs/holonight-services/src/audio/PulseAudioBackend.{h,cpp}`, `AudioService.{h,cpp}` | — |
| 3 | `libs/holonight-services/src/notifications/NotificationRuleStore.{h,cpp}`, `NotificationRuleModel.{h,cpp}`, `apps/shell/app/ShellApplication.{h,cpp}` | — |
| 4 | `libs/holonight-services/src/calendar/CalendarCache.{h,cpp}`, `CalendarSyncManager.{h,cpp}`, `libs/holonight-services/src/CalendarService.{h,cpp}` | — |
| 5 | `libs/holonight-services/src/launcher/LauncherModel.{h,cpp}`, `LauncherService.cpp`, `apps/shell/qml/Launcher/LauncherActionRow.qml`, `apps/shell/qml/Launcher/Launcher.qml` | — |

No new files in any item — every fix extends an existing class or QML component.

---

## Item 1 — PulseAudio Context Failure and Reconnect

### Context recap

`PulseAudioBackend::Impl::contextStateCallback` (`PulseAudioBackend.cpp:254`) already handles
`PA_CONTEXT_FAILED`/`PA_CONTEXT_TERMINATED` by marshalling `availableChanged(false)` to the main
thread via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` — but nothing after that. The
`pa_context*` is left dead; no new context is ever created, no `pa_context_connect` is ever retried.
`start()` (`.cpp:408`) is the only place a context is created/connected, and it is idempotent-guarded
(`impl_->started`), so it cannot be called again to recover.

Per libpulse semantics, a context in `PA_CONTEXT_FAILED`/`TERMINATED` cannot be reconnected in place —
it must be disconnected/unref'd and a **new** `pa_context` created and connected against the
already-running `pa_threaded_mainloop`. This design extracts that "create + wire callbacks + connect"
sequence out of `start()` into a reusable `Impl` method so both the initial connect and every retry
share one code path.

### Interfaces

```cpp
// AudioTypes.h — new, plain (non-QObject) enum, mirrors AudioDeviceType/AudioStreamType's existing style
enum class AudioHealthState : uint8_t { Connected, Reconnecting, Failed };
```

```cpp
// PulseAudioBackend.h
 public:
  ...
  // Test seam only — overrides the production backoff schedule (default: 1s,2s,4s,8s,16s,30s-cap)
  // so reconnect-timing tests don't block on real wall-clock delays. Mirrors the existing
  // setPulseAudioSystem()/resetPulseAudioSystem() static test-seam pattern in this same class.
  static void setReconnectBackoffScheduleForTests(std::vector<int> delays_ms);
  static void resetReconnectBackoffSchedule();

 Q_SIGNALS:
  void deviceAdded(AudioDevice device);
  void sinkRemoved(uint32_t idx);      // was deviceRemoved — see Item 2
  void sourceRemoved(uint32_t idx);    // was deviceRemoved — see Item 2
  void deviceChanged(AudioDevice device);
  void streamAdded(AudioStream stream);
  void streamRemoved(uint32_t idx);
  void streamChanged(AudioStream stream);
  void availableChanged(bool available);
  void healthStateChanged(AudioHealthState state);   // NEW

 private Q_SLOTS:
  void onContextLost();          // NEW
  void onReconnectSucceeded();   // NEW
  void attemptReconnect();       // NEW — QTimer::timeout handler

 private:
  void scheduleReconnect();
  void setHealthState(AudioHealthState state);

  static constexpr int kMaxReconnectAttempts = 8;

  int reconnect_attempt_{0};
  QTimer* reconnect_timer_{nullptr};
  AudioHealthState health_state_{AudioHealthState::Connected};
```

```cpp
// AudioService.h — thin forwarding, matching the existing available/availableChanged pattern
 public:
  [[nodiscard]] int healthState() const { return static_cast<int>(health_state_); }
 Q_SIGNALS:
  ...
  void healthStateChanged();
 private:
  void setHealthState(AudioHealthState value);
  AudioHealthState health_state_{AudioHealthState::Connected};
```

`Impl` gains two private helpers replacing the inline body of `start()`:

```cpp
struct PulseAudioBackend::Impl {
  ...
  void teardownContext();                    // NEW — disconnect/unref the current context, if any
  [[nodiscard]] bool connectNewContext();     // NEW — pa_context_new + set callbacks + lock/connect/unlock
};
```

### Data flow

**Before**: `start()` creates mainloop → creates context inline → sets callbacks → connects.
`PA_CONTEXT_FAILED`/`TERMINATED` → `availableChanged(false)` → dead end.

**After**:

1. `start()`: create mainloop/api (unchanged) → `threaded_mainloop_start()` → `impl_->connectNewContext()`
   (new helper, same body the old inline code had).
2. `PA_CONTEXT_FAILED`/`TERMINATED` fires on the PA thread → `contextStateCallback` marshals a single
   queued lambda that both emits `availableChanged(false)` (unchanged observable behavior) **and**
   calls `PulseAudioBackend::onContextLost()` on the main thread (new).
3. `onContextLost()`: if the reconnect timer is already armed, or `health_state_` is already `Failed`
   (ceiling reached), no-op. Otherwise `setHealthState(Reconnecting)` and `scheduleReconnect()`.
4. `scheduleReconnect()`: if `reconnect_attempt_ >= kMaxReconnectAttempts` (8), `setHealthState(Failed)`
   and stop — **no timer is armed**, guaranteeing no 9th attempt. Otherwise look up the backoff delay
   for `reconnect_attempt_` (0-based) in the schedule (`{1000,2000,4000,8000,16000,30000}`, clamped to
   the last entry for indices ≥ 5 — i.e., attempts 6/7/8 all wait the 30s cap), increment
   `reconnect_attempt_`, and `reconnect_timer_->start(delay_ms)` (single-shot).
5. Timer fires → `attemptReconnect()` → `impl_->teardownContext(); impl_->connectNewContext();` — a
   fresh `pa_context` is created and `pa_context_connect`'d, with the **same** static callbacks
   (`contextStateCallback`/`subscribeCallback`) re-registered, so success/failure of *this* attempt
   flows back through the exact same state machine.
6. If the new context reaches `PA_CONTEXT_READY`: `onContextReady()` (unchanged trigger) now *also*
   marshals `PulseAudioBackend::onReconnectSucceeded()` alongside the existing `availableChanged(true)`.
   `onReconnectSucceeded()` resets `reconnect_attempt_ = 0`, stops the timer defensively, and
   `setHealthState(Connected)`.
7. If the new context **also** fails before reaching `READY`: step 2 repeats. Since
   `health_state_ == Reconnecting` (not yet `Failed`) and the timer has already fired (inactive),
   `scheduleReconnect()` runs again with the incremented counter — this is what produces the
   "fails 3 times, succeeds on the 4th, with exponentially increasing gaps" behavior REQ-F-002 tests.
8. `stop()` (called from both the public API and `~PulseAudioBackend()`) now stops `reconnect_timer_`
   **before** tearing down the mainloop/context — see Risks.

### Key decisions

- **No new `contextLost()` signal.** REQ-F-001's acceptance text offers `contextLost()` as one
  *example* verification mechanism, alongside "or inspecting `isConnected()`/`available`
  transitioning to false" — the latter already exists and already fires correctly today (this bug
  predates Phase 4). Adding a redundant signal purely to match illustrative SPEC prose was rejected;
  `availableChanged(false)` (failure detected) followed by `healthStateChanged(Reconnecting)`
  (reconnect sequence initiated) together give strictly more observable detail than a bare
  `contextLost()` would, using signals this design needs to add anyway.
- **`healthState` modeled as `Q_PROPERTY` + `NOTIFY` on `AudioService`, not an event signal.**
  Phase 1's `SessionService::commandFailed(action, reason)` is an *event* signal (Phase 1's own
  rationale: "there is no meaningful 'current' failure to query later"). Health state is the
  opposite: it is genuinely a *state* a UI would want to query at any time ("is audio currently
  degraded?"), not just react to at the moment of transition. A property+NOTIFY pair (matching the
  existing `available`/`availableChanged` shape on the same class) is the better fit and keeps the
  new surface consistent with its sibling property rather than introducing a second idiom on the
  same class for what is conceptually the same kind of thing (connection health).
- **`AudioService::healthState()` returns `int`, not the enum, and is not `Q_ENUM`-registered.**
  None of REQ-F-001–004's acceptance criteria touch QML — they are GTest/`QSignalSpy` only, and
  `PulseAudioBackend` (where the enum-typed signal actually lives and gets tested) is not itself a
  QML type. Registering `AudioHealthState` as a `Q_ENUM` on `AudioService` purely to look complete
  would add QML-registration surface nothing in this phase exercises or requires. Keeping
  `AudioService::healthState()` as a plain `int` getter is the minimal change that still lets a
  future phase wire a status icon (`AudioService.healthState === 2 /* Failed */`) without any
  further C++ change — deferred, not blocked.
- **Backoff schedule and ceiling: `{1000,2000,4000,8000,16000,30000}` ms, cap sustained through
  attempt 8.** SPEC recommends 6–8 max attempts; 8 (the upper end) was chosen because the cost of a
  wrong choice is asymmetric — one extra 30s-spaced retry is unnoticeable, while giving up one
  attempt early after a transient multi-minute PulseAudio restart (common after a `systemctl --user
  restart pulseaudio` or a Bluetooth-driven server bounce) means the shell needs a manual restart,
  exactly the failure mode this item exists to fix.
- **Test seam: injectable backoff schedule, not literal 1s–30s waits in tests.** SPEC's acceptance
  text asks for real `std::chrono`-measured delays with "±200ms jitter tolerance," which is
  incompatible with running a full 8-attempt ceiling test in CI (worst case ≈ 1+2+4+8+16+30×3 = 121s
  real time). `setReconnectBackoffScheduleForTests()` lets tests substitute a millisecond-scale
  schedule (e.g. `{10,20,40,80,160,300}`) that preserves the doubling-then-cap *shape* the assertions
  care about, while keeping the full test suite under ~1s. This is flagged explicitly in Testing
  Approach below as a deliberate correction to SPEC's literal timing language, in the same spirit as
  Phase 3's DESIGN.md opening "Correction to SPEC's test-strategy language" section.
- **Reconnect logic lives on the Qt main thread, not inside the PA-thread state callback.** The
  callback only marshals a functor via the existing `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`
  idiom already used for every other cross-thread emission in this file. Tearing down and recreating
  a `pa_context` from *within* its own failing state callback (same PA thread, same call stack) is
  fragile and undocumented behavior in libpulse; doing it from a `QTimer` on the main thread, with
  its own explicit `threaded_mainloop_lock`/`unlock` pair (matching every other `PulseAudioBackend`
  mutator), is the same locking discipline already proven correct by `setDeviceVolume` et al.

### Alternatives considered

- **Retry immediately (no backoff) with a fixed retry count.** Rejected — hammering a crashed or
  restarting PulseAudio server with immediate reconnect attempts is exactly the kind of retry-storm
  behavior REQ-F-002 exists to prevent; a fixed short interval also doesn't distinguish "server is
  mid-restart, back in 2s" from "server is gone for the rest of the session."
  A separate `PulseAudioReconnectScheduler` class wrapping the timer/attempt-counter state. Rejected
  as unnecessary indirection: the state (`reconnect_attempt_`, `reconnect_timer_`, `health_state_`) is
  small, has no reuse target elsewhere in the codebase, and `PulseAudioBackend` already owns the
  context lifecycle it's driving — splitting it out would just relocate three fields and three
  methods behind an extra pointer for no testability gain (the class is already fully testable via
  the existing `FakePulseAudioSystem` seam).

### Risks

- **`stop()` must halt `reconnect_timer_` before touching `impl_->mainloop`.** If a scheduled
  reconnect timer is still armed when `stop()` (or the destructor) runs, `attemptReconnect()` could
  fire after `impl_->mainloop` has been freed, calling `teardownContext()`/`connectNewContext()`
  against dangling PA pointers — a use-after-free. `PulseAudioBackend::stop()` must call
  `reconnect_timer_->stop()` as its **first** statement, before any of the existing
  lock/disconnect/unref/free sequence. This is a new invariant this design introduces, not present
  in the current code, and must not be dropped during implementation.
- **`onContextLost()` re-entrancy from a second FAILED/TERMINATED before the timer fires.** Guarded
  by checking `reconnect_timer_->isActive()` at the top of `onContextLost()` — if a backoff is already
  counting down, a duplicate failure notification (e.g., both `errorOccurred`-adjacent PA quirks
  firing close together) is a no-op, not a schedule restart. This preserves the exponential shape
  even under noisy failure signaling.
- **`property()`-style undocumented behavior does not apply here** (unlike Phase 3 Item 4's
  `QDBusInterface::property()` concern) — this item only touches libpulse's C callback API via the
  already-abstracted `PulseAudioSystem` seam, which is fully mockable and already proven reliable by
  the existing 400+ line `test_pulse_audio_backend.cpp` suite.

### Requirement map

REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-NF-001.

---

## Item 2 — Sink/Source Index Separation

### Context recap

`AudioService::onDeviceRemoved(uint32_t idx)` (`AudioService.cpp:169`) is wired to
`PulseAudioBackend::deviceRemoved(uint32_t)` and unconditionally calls **both**
`outputs_->applyRemove(idx)` and `inputs_->applyRemove(idx)`. PulseAudio sink indices and source
indices are independent counters (each typically starts near 0/1) — if a sink with index 5 is
removed while an unrelated, still-connected source also has index 5, `inputs_->applyRemove(5)`
silently deletes that live microphone from the `inputs` model. Confirmed via `grep`: the only
consumers of `PulseAudioBackend::deviceRemoved`/`AudioService::onDeviceRemoved` are internal
C++ (`AudioService.cpp`, `PulseAudioBackend.cpp`, `tests/test_pulse_audio_backend.cpp`) — **zero**
QML files reference `deviceRemoved`, so REQ-F-007's "update all QML consumers" is satisfied
vacuously; there is nothing in `apps/shell/qml/` to migrate.

### Interfaces

```cpp
// PulseAudioBackend.h — deviceRemoved(uint32_t) removed entirely, replaced by:
 Q_SIGNALS:
  void sinkRemoved(uint32_t idx);
  void sourceRemoved(uint32_t idx);
```

```cpp
// AudioService.h
 public:
  void onSinkRemoved(uint32_t idx);     // replaces onDeviceRemoved
  void onSourceRemoved(uint32_t idx);   // replaces onDeviceRemoved
```

`AudioDeviceModel`, `AudioDeviceModel::applyRemove(uint32_t)`, and `AudioStreamModel` are **not
touched** — `outputs_`/`inputs_` are already two separate `AudioDeviceModel` instances; the bug is
entirely in which instance `AudioService` calls `applyRemove` on, not in the model class itself.

### Data flow

**Before**: `PA_SUBSCRIPTION_EVENT_SINK|REMOVE` and `PA_SUBSCRIPTION_EVENT_SOURCE|REMOVE` both funnel
through `handleSinkEvent`/`handleSourceEvent`'s `REMOVE` branch into the *same* emitted signal
(`deviceRemoved(idx)`), which `AudioService::onDeviceRemoved` applies to *both* device models.

**After**: `PulseAudioBackend::Impl::handleSinkEvent`'s `REMOVE` branch (`.cpp:193-198`) emits
`sinkRemoved(idx)` instead of `deviceRemoved(idx)`; `handleSourceEvent`'s `REMOVE` branch
(`.cpp:207-212`) emits `sourceRemoved(idx)`. `AudioService::start()` connects each to its own handler:

```cpp
connect(backend_, &PulseAudioBackend::sinkRemoved, this, &AudioService::onSinkRemoved);
connect(backend_, &PulseAudioBackend::sourceRemoved, this, &AudioService::onSourceRemoved);
```

```cpp
void AudioService::onSinkRemoved(uint32_t idx) {
  outputs_->applyRemove(idx);
  if (idx == default_output_id_) {
    default_output_id_ = kInvalidId;
    emit defaultOutputIdChanged();
  }
}

void AudioService::onSourceRemoved(uint32_t idx) {
  inputs_->applyRemove(idx);
  // No default-input tracking exists today (only default_output_id_) — nothing else to update.
}
```

`default_output_id_` is only ever populated from `AudioDeviceType::Sink` devices
(`applyDefaultDeviceState`, `.cpp:193`), so gating its reset on `onSinkRemoved` only (not
`onSourceRemoved`) is correct by construction — this was implicitly already correct in the old
combined handler (the `idx == default_output_id_` check just happened to also run, harmlessly, on
source removals), so no behavioral narrowing beyond fixing the cross-contamination bug itself.

### Key decisions

- **Split at the `PulseAudioBackend` signal boundary, not inside `AudioService`.** REQ-F-005
  explicitly asks for two *typed* signals from the point where PulseAudio's own event already
  distinguishes sink vs. source (`PA_SUBSCRIPTION_EVENT_SINK` vs. `_SOURCE`, already dispatched to
  separate `handleSinkEvent`/`handleSourceEvent` methods) — the information is available for free at
  the earliest possible point; threading a single combined signal further down and having
  `AudioService` re-derive the type from context would just relocate the same ambiguity one layer up.
- **`AudioDeviceModel` is unchanged.** REQ-F-006 asks to "update `AudioDeviceModel` ... to track
  sinks and sources separately," but investigation shows this is already true — `outputs_` and
  `inputs_` are two independent `AudioDeviceModel` instances, and `applyRemove(uint32_t)` only ever
  removes from whichever instance it's called on. The actual defect is 100% in the *caller*
  (`AudioService::onDeviceRemoved` calling both), not the model. No `AudioDeviceModel.{h,cpp}` change
  is needed or made; REQ-F-006's acceptance ("sink count decrements, source count unchanged") is
  satisfied by the `AudioService`-level fix alone, verified with `outputs_`/`inputs_` `rowCount()`.

### Alternatives considered

- **Keep one `deviceRemoved(uint32_t idx, AudioDeviceType type)` signal instead of two.** Rejected:
  SPEC's REQ-F-005 explicitly specifies two distinct signal names for the acceptance test's
  `EXPECT_CALL(..., sinkRemoved(42)).Times(Exactly(1))` / `EXPECT_CALL(..., sourceRemoved(_)).Times(0)`
  shape — a single signal with a type parameter would satisfy the *data* requirement but not this
  literal test shape, and two signals are marginally more self-documenting at every call site
  (`connect(..., &PulseAudioBackend::sinkRemoved, ...)` reads unambiguously without inspecting a
  second argument).

### Risks

- **REQ-F-007's "migrate all QML consumers first" ordering is inverted from the SPEC's phrasing but
  harmless here.** SPEC frames F-007 as "after all QML consumers have been migrated, remove the old
  signal" — but since grep confirms zero QML consumers exist, this design removes
  `deviceRemoved(uint32_t)` in the **same** change as adding the two new signals (Item 2's own
  change), not as a separate follow-up. Flagged explicitly so implementation doesn't spend a cycle
  searching for QML call sites that don't exist.
- **`AudioDeviceModel migration must land in the same commit as the signal split, or QML silently
  stops updating"** (the exact risk phrasing suggested in the task prompt) **does not apply** to this
  codebase's actual architecture: `AudioDeviceModel` needs no migration (see Key Decisions above), and
  there is no QML binding to the raw signal name that could silently break — QML only ever observes
  `AudioService.outputs`/`.inputs` (the `AudioDeviceModel*` properties), which are updated correctly
  regardless of which `AudioService` slot method ends up calling `applyRemove`. This risk is
  called out here specifically to record that it was investigated and found not to apply, not left
  unconsidered.

### Requirement map

REQ-F-005, REQ-F-006, REQ-F-007, REQ-NF-002.

---

## Item 3 — Notification Rule Persistence Async Write

### Context recap

`NotificationRuleStore::persist(const QList<AppNotificationRule>&) const` (`NotificationRuleStore.h:25`)
performs a fully synchronous `QSaveFile` write on the calling thread — called from
`NotificationRuleModel::persist() const` (`NotificationRuleModel.cpp:126`), itself invoked from
`ensureApp()` (fires on **every** incoming notification, including DND-suppressed ones per
`NotificationService::addOrReplace`'s filter-then-`ensureApp` ordering), `setEnabled()`, and
`setUrgencyFilter()`. The sibling store, `NotificationStore` (`libs/holonight-services/src/notifications/NotificationStore.{h,cpp}`,
backs notification *history*, not rules), already solves this exact problem: `persist()` schedules a
`QtConcurrent::run()` write via a `QFutureWatcher<void>`, serialized through a `write_in_flight_`/
`write_dirty_`/`pending_snapshot_` dirty-flag pattern so writes never overlap and a burst of calls
collapses to one write of the latest snapshot. This design ports that exact pattern into
`NotificationRuleStore`, adding a failure-signal path `NotificationStore` doesn't have (nothing today
asks `NotificationStore` to signal write failure — out of scope here, REQ-F-009 is specific to rules).

### Interfaces

```cpp
// NotificationRuleStore.h
// Outcome of one async rule-file write. Same {bool ok; QString reason;} shape as Phase 1's
// SessionCommandResult, but NOT the same type — this is a different bounded context (disk-write
// diagnostics vs. session-command dispatch) and the two have no reason to share a type beyond an
// incidentally identical shape.
struct RulePersistOutcome {
  bool ok{true};
  QString reason;
};

class NotificationRuleStore : public QObject {
  Q_OBJECT
 public:
  ...
  [[nodiscard]] QList<AppNotificationRule> load() const;

  // Schedules an async write (was: synchronous, `const`). `action` identifies which caller
  // triggered the write, threaded through to rulePersistenceFailed on failure — it is a diagnostic
  // label only, never persisted to disk.
  void persist(const QList<AppNotificationRule>& rules, const QString& action);   // was: `... const`

  [[nodiscard]] QString filePath() const { return file_path_; }

 Q_SIGNALS:
  void writeCompleted();                                            // NEW — test/consumer sync hook
  void persistFailed(const QString& action, const QString& reason); // NEW — REQ-F-009

 private Q_SLOTS:
  void onWriteFinished();   // NEW

 private:
  void launchWrite(QList<AppNotificationRule> rules, QString action);   // NEW
  void ensureDirectoryExists() const;   // unchanged

  QString file_path_;
  bool write_in_flight_{false};             // NEW
  bool write_dirty_{false};                 // NEW
  QList<AppNotificationRule> pending_rules_;   // NEW
  QString pending_action_;                  // NEW
  QString in_flight_action_;                // NEW
  QFutureWatcher<RulePersistOutcome>* watcher_{nullptr};   // NEW
};
```

```cpp
// NotificationRuleModel.h — new Q_SIGNALS section (none existed before)
 Q_SIGNALS:
  void rulePersistenceFailed(const QString& action, const QString& reason);

 private:
  void persist(const QString& action) const;   // was: persist() with no parameter
```

### Data flow

1. `loadPersistedRules()` connects `store_`'s new `persistFailed(QString,QString)` directly to the
   model's own `rulePersistenceFailed(QString,QString)` — a signal-to-signal connection (identical
   parameter list), forwarding with zero glue code:
   ```cpp
   connect(store_, &NotificationRuleStore::persistFailed, this, &NotificationRuleModel::rulePersistenceFailed);
   ```
2. Each of the model's four call sites now passes an explicit action label instead of calling the
   old parameterless `persist()`:
   - `loadPersistedRules()`'s post-prune persist → `persist(QStringLiteral("pruneRules"))`
   - `ensureApp()` (both the existing-entry-update and new-entry-insert branches) → `persist(QStringLiteral("ensureApp"))`
   - `setEnabled()` → `persist(QStringLiteral("setEnabled"))`
   - `setUrgencyFilter()` → `persist(QStringLiteral("setUrgencyFilter"))`
3. `NotificationRuleModel::persist(const QString& action) const` calls
   `store_->persist(rules_, action)` — this compiles even though the enclosing method is `const`
   because `store_` is a raw (non-`const`) pointer *member*; C++ `const`-qualification of a member
   function only constrains the object itself, not objects reachable through a pointer it holds.
4. `NotificationRuleStore::persist()`: if a write is already in flight, buffer `rules`/`action` into
   `pending_rules_`/`pending_action_` and return immediately (dirty-flag coalescing — matches
   `NotificationStore` exactly). Otherwise `launchWrite(rules, action)` immediately.
5. `launchWrite()`: sets `write_in_flight_ = true`, stashes `action` into `in_flight_action_` (needed
   at completion time, since the `QFutureWatcher<RulePersistOutcome>::finished` signal carries no
   context of *which* call triggered it), and dispatches
   `QtConcurrent::run([path, items = std::move(rules)] { return writeRulesToDisk(path, items); })`
   onto the watcher.
6. `writeRulesToDisk()` (free function, anonymous namespace, runs on a `QtConcurrent` worker thread) —
   identical `QSaveFile` open/write/commit logic to today's `persist()` body, but returns
   `RulePersistOutcome{ok=false, reason=...}` on either the `open()` or `commit()` failure path
   instead of only `qCWarning`-logging it.
7. `onWriteFinished()` (main thread, via the watcher's `finished` signal): clears `write_in_flight_`,
   reads `watcher_->result()`, emits `writeCompleted()` unconditionally (existing consumers/tests can
   synchronize on this), and — new — emits `persistFailed(in_flight_action_, result.reason)` if
   `!result.ok`. If a call arrived while this write was in flight (`write_dirty_`), immediately
   `launchWrite(std::move(pending_rules_), std::move(pending_action_))` for the coalesced latest state.
8. **Composition root**: `ShellApplication::startServices()` wires
   `notification_rule_model_->rulePersistenceFailed` to a toast via `NotificationServer::Notify()`,
   mirroring Phase 1's `connectSessionFailureNotifications()` exactly (see below).

### Key decisions

- **Reuse `NotificationStore`'s `QtConcurrent::run()` + `QFutureWatcher` + dirty-flag pattern
  verbatim in shape, not by extracting a shared base class.** REQ-NF-003 asks specifically for reuse
  of "the async-write implementation pattern already proven ... not a new async mechanism" — it does
  not ask for a shared abstraction. The two stores' write bodies differ enough (rules need a
  `RulePersistOutcome` return value to drive REQ-F-009; history's `writeHistoryToDisk` returns `void`
  and only logs) that a shared base class would need a virtual/templated result-handling seam for a
  net negative: two call sites, one abstraction, no reuse economy. Copying the *pattern* (dirty-flag
  fields + watcher + `launchWrite` helper) is cheap, obviously correct by inspection against its
  proven sibling, and keeps `NotificationStore` itself completely untouched (zero regression risk to
  notification history, which is unrelated to this phase's scope).
- **`RulePersistOutcome`, not `SessionCommandResult`, for the result type.** Same `{bool ok; QString
  reason;}` shape as Phase 1's convention, deliberately **not** the same type. `SessionCommandResult`
  lives in `libs/holonight-services/src/session/` and is documented (Phase 1 DESIGN.md) as specific
  to command-dispatch semantics ("launches are detached/fire-and-forget ... no exit code to report").
  Reusing it here would either import a session-domain type into the notifications subsystem for an
  unrelated concept (a disk-write outcome), or require relocating it to a shared location — a bigger,
  unrelated refactor. The Phase 1 *convention* being reused is the two-argument failure-signal shape
  (`(QString action, QString reason)`), not the literal C++ type; REQ-F-009's own wording ("following
  the Phase 1 convention") supports this reading.
- **`persist()` signature change (`const` → non-`const`, one parameter → two) is a breaking API
  change to an existing method, deliberately, not additive.** An overload preserving the old
  zero-parameter `persist(rules)` signature was considered and rejected: every production call site
  is inside this same subsystem (`NotificationRuleModel`) and gets updated in this same change, so
  there is no external caller an overload would protect; keeping a parameterless overload would let a
  future call site silently opt out of the action-labeling REQ-F-009 needs.
- **Action strings are one per call site (`"pruneRules"`, `"ensureApp"`, `"setEnabled"`,
  `"setUrgencyFilter"`), not a single generic `"updateRule"` string.** SPEC's own acceptance text
  uses `"updateRule"` illustratively ("e.g."). Per-call-site labels carry strictly more diagnostic
  value in the eventual toast/log line and match Phase 1's own established convention of specific
  per-command action strings (`"lock"`, `"logout"`, `"sleep"`, `"reboot"`, `"shutdown"`) rather than
  one generic label.
- **Composition-root wiring to a visible toast, not just an unobserved signal.** SPEC's acceptance
  criteria for REQ-F-009 only require the signal to fire (`QSignalSpy`); no QML/UI wiring is
  mandated. This design wires it anyway, mirroring Phase 1's `connectSessionFailureNotifications()`
  pattern exactly, because SPEC's own Non-Goal #7 explicitly names *this* signal (alongside audio's
  `healthState`) as one of "the two highest-priority instances" selected to receive the full
  convention-pattern rollout in Phase 4 — a signal nothing ever listens to still leaves the failure
  effectively silent to the user, which is the exact problem REQ-F-009 exists to fix.

  ```cpp
  // ShellApplication.h — new private method declaration, alongside connectSessionFailureNotifications()
  void connectNotificationRuleFailureNotifications();
  ```
  ```cpp
  // ShellApplication.cpp — new method, called once from startServices() right after the existing
  // connectSessionFailureNotifications(); call (~line 213)
  void ShellApplication::connectNotificationRuleFailureNotifications() {
    connect(notification_rule_model_, &NotificationRuleModel::rulePersistenceFailed, this,
            [this](const QString& action, const QString& reason) {
      notification_server_->Notify(QStringLiteral("HoloNight Shell"), 0, QString(),
                                   QStringLiteral("Notification rule save failed: %1").arg(action),
                                   reason, {}, {}, -1);
    });
  }
  ```
  Both `notification_rule_model_` and `notification_server_` are already-constructed `ShellApplication`
  members at the point `startServices()` runs (constructor order: `notification_rule_model_` at
  ShellApplication.cpp:122, `notification_server_` at :124, both before `startServices()`'s body
  executes) — no new `#include`, no ordering change.

### Alternatives considered

- **`QFuture<RulePersistOutcome>` returned directly from `persist()` for the caller to inspect.**
  Rejected: none of the three call sites (`ensureApp`/`setEnabled`/`setUrgencyFilter`) are positioned
  to usefully act on a future synchronously — they're QML-invoked slots that return immediately by
  contract already (`Q_INVOKABLE void setEnabled(...)`). A signal-based failure path fits the
  existing calling convention without changing any public `Q_INVOKABLE` return type.
- **Skip the dirty-flag coalescing and just fire-and-forget a new `QtConcurrent::run()` per
  `persist()` call.** Rejected: REQ-F-010 requires "no lost updates" under repeated rapid calls
  (e.g., a burst of notifications each calling `ensureApp()` → `persist()`); uncoalesced concurrent
  writes to the same file from multiple worker threads could interleave `QSaveFile` temp-file
  renames unpredictably. The dirty-flag pattern (already proven safe in `NotificationStore`)
  guarantees writes are strictly serialized and only the latest snapshot is ever written.

### Risks

- **`tests/test_notification_rules.cpp:118` (`const NotificationRuleStore store(rulesPath(dir));`)
  will fail to compile once `persist()` is non-`const`.** This is an expected, necessary test-file
  change (drop the `const` on that one local declaration), not a design defect — flagged explicitly
  so it isn't mistaken for an unplanned regression during implementation. The same test
  (`WritesExpectedJson`) also currently reads the output file synchronously immediately after
  `persist()` returns; it must be updated to wait for `writeCompleted()` (via `QSignalSpy`) before
  asserting on file contents, matching `test_notification_history.cpp`'s existing
  `updateConfigAppliesToNextPersist` test's proven waiting idiom for the identical async pattern.
- **`chmod`-based write-failure simulation assumes POSIX permission enforcement is active for the
  test-running user.** REQ-F-009's test needs a deterministic, injectable write failure; no fake-
  filesystem seam exists for `NotificationRuleStore` today (nor for its sibling `NotificationStore`).
  This design's test plan (below) uses `QFile::setPermissions()` to strip write access from the
  rules directory before calling `persist()`. If tests are ever run as root (some CI containers), the
  permission bits are ignored by the kernel and the induced failure won't reproduce — flagged for
  implementation-time verification of the test-running UID, not solved by this design (adding a
  filesystem-injection seam was judged disproportionate to one test's needs).
- **Dirty-flag coalescing means the `action` label attached to a failure signal is always the
  *latest* triggering call, not necessarily the specific call whose data change is reflected in the
  failed write.** Acceptable per this design: the label is diagnostic-only (never persisted, never
  used to determine *what* to retry), and `NotificationStore`'s existing snapshot-coalescing already
  accepts the equivalent "only the latest state survives a burst" behavior for its own data.

### Requirement map

REQ-F-008, REQ-F-009, REQ-F-010, REQ-NF-003.

---

## Item 4 — Calendar Cache Reconciliation and Account Removal

### Context recap

**[F-02]/REQ-F-011**: `CalendarSyncManager::onSyncFinished()` (`.cpp:162-195`) calls
`cache_.upsertEvents(*result)` on every successful sync but never deletes cached events absent from
the fresh result — `cache_.pruneExpired()` only removes events outside the ±30/180-day date window,
not events the upstream server no longer reports (i.e., cancelled/deleted meetings). Cancelled
events linger in the cache indefinitely and continue to be picked up by `dispatchNotifications()`.

**[F-03]/REQ-F-012**: `CalendarCache::clearAccountEvents(provider_type, account_name)` and
`removeStaleAccounts(active_keys)` (`CalendarCache.h:38,57`) are both fully implemented and unit-tested
(`CalendarCacheV2Test.ClearAccountEventsRemovesOnlyThatAccount`,
`.RemoveStaleAccountsClearsDeletedAccounts` in `tests/test_calendar_integration.cpp`) but have **zero**
production callers — confirmed via `grep`.

### REQ-F-013 — explicit resolution

**Finding**: `CalendarService::onCalendarConfigChanged()` (`CalendarService.cpp:117-135`) is:

```cpp
void CalendarService::onCalendarConfigChanged() {
  ...
  const bool has_accounts = !cal_cfg.caldav_accounts.isEmpty() || !cal_cfg.ics_accounts.isEmpty();
  if (sync_manager_ == nullptr && has_accounts) {
    initSyncManager();
  }
}
```

The `sync_manager_ == nullptr` guard means `initSyncManager()` — the **only** place this method does
anything beyond the (unconditional) `week_start_day_` update — runs *exactly once*, the first time
`onCalendarConfigChanged()` observes a non-empty account list. Every subsequent invocation of this
slot (triggered by `ConfigService::calendarConfigChanged`, which fires on *any* calendar config edit
— adding, removing, or modifying an account) is a **no-op** beyond the week-start-day check, once
`sync_manager_` is non-null. This is confirmed by direct code reading, not inference.

**Decision: this is a genuine latent bug, and it directly blocks REQ-F-012 — the two requirements
are fixed together in this design, not independently.**

Rationale: REQ-F-012's own acceptance test scenario is "(1) account with events in the cache exists,
(2) config-change notification *removes* that account, (3) verify `clearAccountEvents` is called."
Step (1) requires an account to already exist and have synced — which means `sync_manager_` is
already non-null by the time step (2)'s removal-triggering `onCalendarConfigChanged()` call happens.
Under the *current* guard, that call is a no-op; no code path in the existing method would ever reach
new account-removal-detection logic placed inside (or gated by) the `sync_manager_ == nullptr`
branch. Implementing REQ-F-012 correctly is therefore **not possible** without restructuring this
method to act on every invocation, independent of `sync_manager_`'s state — so REQ-F-013's "does it
block F-012's wiring" question (explicitly posed in SPEC.md) resolves to **yes, it blocks it**, and
the fix is scoped as part of F-012's implementation, not deferred.

**Explicit scope boundary** (this design does *not* fix the whole class of gaps this bug represents):
only **account removal** detection is added to `onCalendarConfigChanged()`. Detecting a *newly added*
account after `sync_manager_` already exists (i.e., hot-adding a second CalDAV account without a
shell restart) remains unfixed — `initSyncManager()`'s `sync_manager_ == nullptr` guard is untouched,
so a second account added after the first will still not be picked up until restart. This is a
real, related gap, but expanding `CalendarSyncManager`'s provider list at runtime is a materially
larger change (see Risks) than REQ-F-012's stated scope ("invoke `clearAccountEvents`"), and is
explicitly deferred to a future phase.

Per REQ-F-013's acceptance criterion, an inline comment recording this finding, the question, and the
reference is added at the fixed call site in `CalendarService.cpp` (see code below) — this design
document is the "Design-stage review" REQ-F-013 asks for; the comment is the required pointer back to
it.

### Interfaces

```cpp
// CalendarCache.h
// Deletes cached events for (provider_type, account_name) whose uid is absent from fresh_events.
// Called after every successful sync so events the upstream server no longer reports (cancelled/
// deleted meetings) are removed instead of lingering until pruneExpired()'s unrelated date-window
// eventually catches them (or never, if the cancelled event was still inside the retention window).
bool reconcileAccountEvents(const QString& provider_type, const QString& account_name,
                            const QList<CalendarEvent>& fresh_events);
```

```cpp
// CalendarSyncManager.h
 public:
  ...
  // Cleans up all cached data for one account, without touching the live provider list or
  // in-flight sync state for other accounts. See CalendarService::onCalendarConfigChanged() for
  // the only production caller (config-driven account removal).
  void removeAccount(const QString& provider_type, const QString& account_name);
```

```cpp
// CalendarService.h
 private:
  ...
  QSet<QString> known_account_keys_;   // NEW — "provider_type:account_name" composite keys, diffed
                                        // on every onCalendarConfigChanged() call to detect removals
```

### Data flow

**REQ-F-011 (reconciliation)** — `CalendarSyncManager::onSyncFinished()`:

```cpp
backoff_.remove(account);
cache_.upsertEvents(*result);
cache_.reconcileAccountEvents(provider_type, account, *result);   // NEW
cache_.pruneExpired();
```

`reconcileAccountEvents()` (`CalendarCache.cpp`, new method, same file/class as the existing
`upsertEvents`/`clearAccountEvents`/`removeStaleAccounts`): `SELECT uid FROM events WHERE
provider_type=? AND account_name=?`, diff against the fresh result's uid set in C++ (a `QSet<QString>`
membership check per row — O(n) total, not the O(n²) SPEC's REQ-NF-004 warns against), then `DELETE
... WHERE uid = ?` per stale uid inside one transaction (mirroring `upsertEvents`'s existing
per-row-prepared-statement-inside-one-transaction shape). For a typical per-account event count (tens,
even in the 500-events/10-calendars stress scenario REQ-NF-004 describes, since events are spread
across accounts), this is a handful of prepared-statement executions — comparable cost to the
`upsertEvents` call that already runs on the same path.

**REQ-F-012/013 (account removal)** — `CalendarService::onCalendarConfigChanged()`, restructured to
run its diff on every call, independent of `sync_manager_`'s state:

```cpp
void CalendarService::onCalendarConfigChanged() {
  const auto* config = ConfigService::instance();
  if (config == nullptr) return;

  const QString new_day = ...;   // unchanged
  if (new_day != week_start_day_) { week_start_day_ = new_day; emit weekStartDayChanged(); }

  const auto& cal_cfg = config->calendarConfig();
  QSet<QString> current_keys;
  for (const auto& acct : cal_cfg.caldav_accounts) {
    current_keys.insert(QStringLiteral("caldav:") + acct.account_name);
  }
  for (const auto& acct : cal_cfg.ics_accounts) {
    current_keys.insert(QStringLiteral("ics:") + acct.account_name);
  }

  // POC Remediation Phase 4 REQ-F-013 (SPEC.md): this method previously only acted on the FIRST
  // call where sync_manager_ was still null (see the initSyncManager() guard below); every later
  // config change was silently a no-op beyond the week-start-day check. This was a genuine latent
  // bug, not intentional caching — REQ-F-012's account-removal wiring cannot function without
  // detecting removals on every call, so the diff below now runs unconditionally, independent of
  // sync_manager_'s state. Scope note: only REMOVAL is detected here; a newly added second account
  // after the first sync is still not hot-picked-up (initSyncManager()'s guard is unchanged) —
  // see Phase 4 DESIGN.md Item 4 for why that narrower gap is deliberately out of scope.
  QSet<QString> removed_keys = known_account_keys_;
  removed_keys.subtract(current_keys);
  for (const QString& removed_key : removed_keys) {
    const qsizetype sep = removed_key.indexOf(QLatin1Char(':'));
    const QString provider_type = removed_key.left(static_cast<int>(sep));
    const QString account_name = removed_key.mid(static_cast<int>(sep) + 1);
    if (sync_manager_ != nullptr) {
      sync_manager_->removeAccount(provider_type, account_name);
    }
  }
  known_account_keys_ = current_keys;

  const bool has_accounts = !current_keys.isEmpty();
  if (sync_manager_ == nullptr && has_accounts) {
    initSyncManager();
  }
}
```

```cpp
// CalendarSyncManager.cpp
void CalendarSyncManager::removeAccount(const QString& provider_type, const QString& account_name) {
  cache_.clearAccountEvents(provider_type, account_name);
  in_progress_.remove(account_name);
  backoff_.remove(account_name);
  refreshModel();
}
```

### Key decisions

- **`clearAccountEvents`, not `removeStaleAccounts`, for the removal call.** `removeStaleAccounts`
  additionally deletes `sync_state`/`accounts` table rows, which requires the `accounts` table to be
  kept in sync via `upsertAccount()` — currently dead code with zero callers (a separate, pre-existing
  gap, not requested by REQ-F-012). REQ-F-012's acceptance criterion explicitly names
  `clearAccountEvents` as the call to verify. Calling it directly, once per removed account, is both
  the literal ask and the smaller, more targeted change.
- **`known_account_keys_` is seeded from *config*, not from what actually got a `CalDavProvider`/
  `IcsProvider` constructed.** `initSyncManager()` skips accounts with missing `url`/`username`
  (logging a warning) — but tracking only "successfully constructed" accounts would mean an account
  that's *fixed up* later (e.g., a username typo corrected) is invisible to the removal-diff the
  first time it's added, then falsely flagged "removed" the next config change even though it's
  present, just re-triggering a harmless no-op `clearAccountEvents` on an account with nothing
  cached. Tracking every key present in *config* (regardless of provider-construction success) makes
  every removal detection correct and every false-positive `clearAccountEvents` call a guaranteed
  no-op (nothing was ever cached for an account whose provider was never built).
- **`removeAccount()` does not remove the provider object from `caldav_providers_`/`ics_providers_`.**
  See Risks — this is a deliberate, called-out scope limitation, not an oversight.

### Alternatives considered

- **Rebuild `sync_manager_` from scratch on every config change** (destroy and reconstruct via
  `initSyncManager()`, dropping the `sync_manager_ == nullptr` guard entirely). Rejected: this would
  also incidentally fix the "hot-add a second account" gap, but at much higher risk — destroying a
  live `CalendarSyncManager` requires draining in-flight `QtConcurrent` sync tasks (the destructor
  already does this via `findChildren<QFutureWatcherBase*>()->waitForFinished()`, which blocks the
  calling thread — i.e., blocks `onCalendarConfigChanged()`, a main-thread slot, for however long any
  in-flight HTTP sync takes). A config-driven full rebuild on the main thread introduces a new
  UI-thread-blocking-I/O risk (§5 gap #5 in the POC Readiness Review) to fix a scope this phase
  doesn't require fixing. Deferred as a much larger, separately-scoped change.
- **Diff against `CalendarCache`'s `accounts` table (via `upsertAccount`) instead of an in-memory
  `known_account_keys_` set on `CalendarService`.** Rejected: this would require also wiring
  `upsertAccount()` calls (dead code today) to keep that table current, which is strictly more
  surface area for the same information `CalendarService` can track in memory with one `QSet<QString>`
  member and zero new SQL.

### Risks

- **A removed account's provider object keeps running its periodic sync until shell restart.**
  `removeAccount()` clears cached events but does **not** remove the corresponding
  `CalDavProvider`/`IcsProvider` from `caldav_providers_`/`ics_providers_` — those vectors are
  populated once in `CalendarSyncManager`'s constructor and never mutated afterward by design (the
  constructor comment/architecture predates Phase 4). Removing an in-use element from that vector
  while a `QtConcurrent::run([ptr, ...])` task holds a raw pointer into it (`runProviderSync`,
  `.cpp:152`) would be a dangling-pointer hazard — the existing destructor's "wait for all watchers"
  pattern only guarantees safety at full-object teardown, not for live single-provider removal.
  **Consequence**: if the removed account's credentials/URL still resolve successfully (e.g., only
  removed from *this* shell's config, not actually revoked upstream), its next periodic sync will
  re-fetch and re-`upsertEvents()` its events, effectively undoing the `clearAccountEvents()` cleanup
  on the next 15-minute (CalDAV) or 60-minute (ICS) cycle. This is scoped out of Phase 4 deliberately
  — REQ-F-012 asks only for cache cleanup, and safely draining/removing a live provider is a
  materially larger, separately-reviewable change. Recommended as a follow-up phase item.
- **`in_progress_`/`backoff_` are keyed by `account_name` alone, not `(provider_type, account_name)`**
  — a pre-existing simplification (not introduced by this design) that assumes account names are
  unique across CalDAV and ICS. `removeAccount()`'s `in_progress_.remove(account_name)` inherits this
  same assumption; flagged for awareness, not fixed here (out of this item's scope).
- **REQ-NF-004 performance claim is analytical, not load-tested in this design.** The reconciliation
  query pattern is a straightforward extension of `upsertEvents`'s already-proven-fast shape; a
  dedicated performance test (500+ synthetic events across 10+ calendars, per SPEC's own acceptance
  wording) is part of the Testing Approach below to make this an empirical, not just structural, claim.

### Requirement map

REQ-F-011, REQ-F-012, REQ-F-013, REQ-NF-004.

---

## Item 5 — Launcher Keyboard-Selection Dispatch and Highlight

### Context recap

`Launcher.qml`'s search-mode delegate (`.qml:260-307`) flattens app rows and their desktop actions
into one `LauncherModel` list (`isAction`/`actionIndex`/`actionParent` roles). Mouse click on an
action row already correctly calls `LauncherService.launchAction(searchDelegate.index,
searchDelegate.actionIndex)` (`.qml:305`) — this works because `LauncherModel::entryAt(row)`
(`LauncherModel.cpp:169`) returns `&results_.at(row).entry`, and **every** `ScoredEntry` (app row or
action row) carries its own full copy of the parent `DesktopEntry` including its `.actions` list
(`LauncherModel.cpp:233,244,247,256`) — so `entryAt(flatRowIndex)` returns the correct backing entry
regardless of whether that flat row represents the app itself or one of its actions.

The keyboard path is different: `LauncherSearchField.onLaunchRequested` (wired in `Launcher.qml:161`)
calls `LauncherService.launchSelected()`, which is simply `return launch(selected_index_);`
(`LauncherService.cpp:404`) — **always** the app's default-launch path, never
`launchAction(selected_index_, ...)`, regardless of whether `selected_index_` currently points at an
action row. This is the exact defect U-10 [D-001] describes: keyboard Enter always launches the
wrong command for a keyboard-selected action row.

Separately, `LauncherActionRow.qml` has **no** `selected` property or highlight visual at all — only
a mouse-hover `Rectangle` (`rowArea.containsMouse`) — unlike its sibling `LauncherResultRow.qml`,
which already has an established cyan background+border "selected" visual language
(`.qml:21-36`) used for app rows. `Launcher.qml`'s `LauncherActionRow { ... }` instantiation
(`.qml:298-306`) does not bind any `selected:` property either — there is nothing to bind to.

### Interfaces

```cpp
// LauncherModel.h — two new public const accessors, alongside entryAt()
[[nodiscard]] bool isActionRow(int row) const;
[[nodiscard]] int actionIndexAt(int row) const;
```

```cpp
// LauncherModel.cpp
bool LauncherModel::isActionRow(int row) const {
  if (row < 0 || row >= results_.size()) return false;
  return results_.at(row).is_action;
}

int LauncherModel::actionIndexAt(int row) const {
  if (row < 0 || row >= results_.size()) return -1;
  return results_.at(row).action_index;
}
```

```cpp
// LauncherService.cpp — the entire fix
bool LauncherService::launchSelected() {
  if (model_.isActionRow(selected_index_)) {
    return launchAction(selected_index_, model_.actionIndexAt(selected_index_));
  }
  return launch(selected_index_);
}
```

```qml
// LauncherActionRow.qml — new property, matching LauncherResultRow.qml's existing selected pattern
property bool selected: false
```

### Data flow

**Before**: Enter → `LauncherSearchField.onLaunchRequested` → `LauncherService.launchSelected()` →
`launch(selected_index_)` → `model_.entryAt(selected_index_)`'s **default** `exec` — wrong for an
action row.

**After**: Enter → `launchSelected()` first asks `model_.isActionRow(selected_index_)`. Since
`selected_index_` is the same flat-list index space `LauncherModel::entryAt()`/`actionIndexAt()`
already operate on (confirmed above — it's the *same* index the mouse-click path already passes as
`entry_index` to `launchAction`), `launchAction(selected_index_, model_.actionIndexAt(selected_index_))`
dispatches the exact action `exec` the row displays, reusing `launchAction`'s existing, already-correct
implementation unchanged. For a non-action row, `isActionRow()` returns `false` (this is also true for
every row in **browse mode**, where `ScoredEntry.is_action` is always constructed `false` —
`LauncherModel.cpp:233`), so `launch(selected_index_)` runs exactly as before — REQ-F-016 (browse
mode unaffected) holds by construction, not by a separate mode check.

For the highlight: `Launcher.qml`'s `LauncherActionRow { ... }` instantiation gains
`selected: searchDelegate.index === LauncherService.selectedIndex` (identical binding shape to the
sibling `appRow`'s existing `selected: searchDelegate.index === LauncherService.selectedIndex` two
lines above it). `LauncherActionRow.qml` gains the same two `Rectangle`s
`LauncherResultRow.qml` already uses for its selected state (cyan-tinted background at 0.10 opacity,
cyan border frame at 0.65 opacity, both driven by `root.selected`), placed above the existing
mouse-hover `Rectangle` in paint order (hover then renders on top when both are simultaneously true,
matching `LauncherResultRow.qml`'s existing layering).

### Key decisions

- **Fix lives entirely in `launchSelected()`, not in the QML Enter-key handler.** `LauncherSearchField`
  emits a mode-agnostic `launchRequested` signal; `Launcher.qml` wires it unconditionally to
  `LauncherService.launchSelected()`. Branching on row type inside QML (querying `isAction`/
  `actionIndex` roles from the model at the current `selectedIndex` and calling `launch()` vs.
  `launchAction()` directly from QML) was considered and rejected: it would duplicate the exact
  index-interpretation logic `launchAction()`'s C++ implementation already encodes, in a second
  place, in a different language — exactly the kind of "diverged duplication" pattern the POC
  Readiness Review's §5 gap #2 calls out as an active bug source in this codebase (it directly
  caused U-07's Critical CalDAV timeout finding). Keeping the branch in C++, next to `launch()`/
  `launchAction()` themselves, is the smaller, more maintainable, single-source-of-truth fix.
- **`isActionRow`/`actionIndexAt` are new, minimal, bounds-checked accessors on `LauncherModel`, not
  a `data()` role read from `LauncherService`.** `LauncherService::launchSelected()` already has
  direct C++ access to `model_` as a value member (not through the `QAbstractItemModel*` interface
  QML uses) — reading `results_.at(row).is_action`/`.action_index` through two small named accessors
  is clearer and more type-safe than round-tripping through `data(index(row), IsActionRole)` and a
  `QVariant` unwrap, and matches the existing `entryAt()` accessor's exact shape/bounds-checking
  style in the same class.
- **Highlight limited to background/border opacity — no Enter-hint icon copy.** `LauncherResultRow.qml`
  also shows a "↵ Enter" hint chip when `selected` (`.qml:107-155`), narrowing its text column to make
  room. This design does **not** replicate that chip onto `LauncherActionRow.qml`: REQ-NF-005 restricts
  the highlight mechanism to "background/border/opacity only," and adding a new child element (even a
  small one) is a layout change beyond that literal scope, not merely a highlight. Two opacity-driven
  `Rectangle`s are the minimal, unambiguously-compliant change; a follow-up visual-polish pass can add
  the hint chip later if desired.

### Alternatives considered

- **Change `launchAction(entry_index, action_index)`'s first parameter semantics** (e.g., accept a
  "canonical" app index instead of the flat row index) to make the keyboard path's intent more
  explicit. Rejected: it works correctly today precisely *because* every flat row (app or action)
  carries its own full entry copy — changing this would require also changing the already-correct,
  already-shipped mouse-click call site (`Launcher.qml:305`) for no behavioral gain, widening the
  change's blast radius for a purely cosmetic API preference.

### Risks

- **`selected_index_` out-of-range at the moment `launchSelected()` is called.** Both new accessors
  are bounds-checked (`row < 0 || row >= results_.size()`) and return safe defaults (`false`/`-1`)
  matching `entryAt()`'s existing `nullptr`-on-out-of-range convention — `launchSelected()` falls
  through to the unchanged `launch(selected_index_)` path, which already handles an invalid index via
  its own `entry == nullptr` guard. No new failure mode introduced.
- **qmllint's Loader/`required property` gotchas (CLAUDE.md) do not apply here** — `LauncherActionRow`
  is instantiated directly (not through a `Loader`), and `selected` is declared as a plain
  `property bool` (not `required`), matching the non-required pattern CLAUDE.md documents for
  optional per-branch properties — though here there is only one branch, so this is simply the
  correct default, not a workaround.

### Requirement map

REQ-F-014, REQ-F-015, REQ-F-016, REQ-NF-005.

---

## Testing Approach

| Requirement group | Test file | New/existing | What it covers |
|---|---|---|---|
| Item 1 (REQ-F-001–004, NF-001) | `tests/test_pulse_audio_backend.cpp` | **Existing, extended** | New `TEST(PulseAudioBackend, ...)` cases using the existing `FakePulseAudioSystem` seam: drive `state_cb(mock_context, PA_CONTEXT_FAILED, ...)` directly, assert `healthStateChanged` spy sees `Reconnecting` then (after the injected/scaled backoff schedule elapses, via `setReconnectBackoffScheduleForTests`) `Connected` or `Failed`; assert `connect_calls` increments once per scheduled attempt, never more than `kMaxReconnectAttempts` (8) after sustained failure. Existing `devices()`/volume-getter tests in this file and `test_audio_service.cpp` run unmodified (REQ-NF-001). |
| Item 2 (REQ-F-005–007, NF-002) | `tests/test_pulse_audio_backend.cpp` + `tests/test_audio_service.cpp` | **Existing, extended** | Backend-level: inject `PA_SUBSCRIPTION_EVENT_SINK|REMOVE` and `_SOURCE|REMOVE` via `subscribe_cb`, spy separately on `sinkRemoved`/`sourceRemoved`, assert non-overlapping calls with distinct indices. Service-level: `AudioService(AudioService::SkipInit)` (existing seam), seed `outputs_`/`inputs_` with 2 devices each via `onDeviceAdded`, call `onSinkRemoved`/`onSourceRemoved` directly, assert only the matching model's `rowCount()` decrements. `tests/test_audio_stream_model.cpp` runs unmodified (REQ-NF-002). |
| Item 3 (REQ-F-008–010, NF-003) | `tests/test_notification_rules.cpp` | **Existing, extended** | `QElapsedTimer` around a `persist()` call proves sub-10ms return (REQ-F-008); `QFile::setPermissions()` strips write access from the rules directory before `persist()` to deterministically fail the write, `QSignalSpy` on `persistFailed`/`rulePersistenceFailed` asserts one emission with a non-empty reason (REQ-F-009); the existing `WritesExpectedJson`/`TogglingEnabledAndFilterPersists`/`PersistedRulesSurviveNewInstance` tests are updated to wait on `writeCompleted` (`QSignalSpy`) before asserting file/model state, proving identical persisted output before/after (REQ-F-010). |
| Item 4 (REQ-F-011–013, NF-004) | `tests/test_calendar_integration.cpp` | **Existing, extended** | Two-tier: (a) direct `CalendarCacheV2Test` unit test — `upsertEvents({A,B,C})`, `reconcileAccountEvents("caldav","work",{A,C})`, `queryRange` confirms only `{A,C}` remain (REQ-F-011, isolated from sync-manager timing); (b) `CalendarSyncManagerTest` integration test extending the existing `FakeCalendarProvider` with a mutable events list — two sequential `CalendarSyncManager` instances against the same on-disk temp DB (first returns `{A,B,C}`, second returns `{A,C}`), proving the `onSyncFinished()` wiring end-to-end. Account removal (REQ-F-012): construct `CalendarService` (or exercise `CalendarSyncManager::removeAccount` directly against a `CalendarCache` pre-populated via `upsertEvents`), assert `queryRange` shows zero events for the removed account afterward. REQ-NF-004: a dedicated perf test populates 500+ events across 10+ synthetic accounts and asserts `reconcileAccountEvents` completes in comfortably under 100ms via `QElapsedTimer`. |
| Item 5 (REQ-F-014–016, NF-005) | `tests/test_launcher_service.cpp` (C++) + `tests/qml/tst_LauncherActionRow.qml` (**new**) | **Existing extended + new** | REQ-F-014's actual defect and fix are 100% C++ (`LauncherModel::isActionRow`/`actionIndexAt`, `LauncherService::launchSelected`) — a GTest extending the existing `FakeLauncherBackend` fixture constructs a `LauncherModel` with an entry that has one action, sets `selected_index_` (via `setSelectedIndex`) to the action's flat row, calls `launchSelected()`, and asserts `FakeLauncherBackend::last_launch_exec_` (or equivalent capture field) matches the *action's* exec, not the app's default exec — this is the faithful, deterministic test; it supersedes SPEC's illustrative QML-spy framing for F-014 specifically because no QML code changes for F-014 at all. REQ-F-015 (the highlight) is the actual QML change and gets a new `tests/qml/tst_LauncherActionRow.qml` `TestCase`: `createTemporaryObject(LauncherActionRow, ...)`, toggle `selected`, use the built-in `TestCase.findChild(row, "actionRowSelectedHighlight")` to read the highlight `Rectangle`'s `opacity`, assert it matches `LauncherResultRow.qml`'s selected-state opacity values (0.10/0.65) for consistency (documented in the test per REQ-F-015's acceptance wording). REQ-F-016/NF-005 verified by inspection (no browse-mode or font/icon-property lines touched) plus the existing `LauncherService` selection tests (`SelectionClampsToResultBounds`, `SelectionWrapsAround`) running unmodified. |

**Correction to SPEC's test-strategy language, Item 1**: REQ-F-002's acceptance criterion asks for
real `std::chrono`-measured delays with "±200ms jitter tolerance" against the production 1s–30s
schedule. Run literally, an 8-attempt ceiling test would take ≈121s of real wall-clock time — this
design's `setReconnectBackoffScheduleForTests()` seam (§Item 1 Key Decisions) substitutes a
millisecond-scale schedule preserving the doubling-then-cap shape, and tests assert *relative*
growth (each delay ≈2× the previous, within a proportional tolerance) and the *count* of attempts at
the ceiling, rather than literal 1000ms/2000ms/…/30000ms wall-clock measurements. This mirrors Phase
3 DESIGN.md's precedent of explicitly correcting SPEC's test-strategy assumptions where the literal
wording is impractical for the actual test infrastructure.

**Correction to SPEC's test-strategy language, Item 5 (REQ-F-014)**: SPEC frames REQ-F-014 as a QML
`TestCase` exercising keyboard navigation end-to-end inside the `Launcher` component. Investigation
shows the defect and its fix are entirely within `LauncherService`/`LauncherModel` (C++) — the QML
Enter-key wiring (`LauncherSearchField.onLaunchRequested` → `LauncherService.launchSelected()`) is
unchanged and was never the bug. A GTest against the real `LauncherService` (reusing
`tests/test_launcher_service.cpp`'s existing `FakeLauncherBackend`) is the more direct, deterministic
test for the actual fix; per this codebase's established convention (`tests/test_session_service.cpp`
et al., documented in Phase 3 DESIGN.md's opening correction), hand-written fakes with inspectable
call-log fields are used throughout this project instead of gmock `EXPECT_CALL`.

---

## Sequencing

SPEC's Overview states the five items are "self-contained service-layer and QML repairs with no
inter-item dependencies." This holds **across** items 1–5: no file touched by one item is touched by
another (see Component Map — zero overlap), and no item's test plan depends on another item's code
landing first. They may be implemented and reviewed in any order, or in parallel, exactly as SPEC
frames them.

One **intra-item** dependency was found during design, entirely within Item 4: **REQ-F-012 cannot be
correctly implemented without also resolving REQ-F-013's finding** (see the REQ-F-013 section above)
— the `onCalendarConfigChanged()` restructuring that makes account-removal detection work on every
call (not just the first) is the same code change REQ-F-012 needs to land at all. These two
requirements are implemented as one inseparable change within Item 4's scope; REQ-F-011 (reconciliation
in `CalendarSyncManager`/`CalendarCache`) has no dependency on REQ-F-012/013 and could land first or
independently within the item.

No item in this phase depends on Phase 1 or Phase 3 code changing further — both are consumed
read-only (Phase 1's `SessionCommandResult`/`commandFailed` convention is referenced as a pattern for
Item 3's signal shape, not called; Phase 3's `QtConcurrent`/`QFutureWatcher` idiom is likewise
followed by example, not by shared code).

---

## Cross-Cutting Risks Summary

| Risk | Item | Severity | Mitigation / status |
|---|---|---|---|
| `PulseAudioBackend::stop()` must halt `reconnect_timer_` before freeing the mainloop, or a queued retry can fire against dangling PA pointers | 1 | Medium | New explicit ordering requirement documented in Design; not present in current code, must not be dropped during implementation |
| SPEC's literal real-time backoff measurement (±200ms jitter over a 1s–30s schedule) is impractical for an 8-attempt CI test (~121s worst case) | 1 | Low | `setReconnectBackoffScheduleForTests()` seam substitutes a scaled schedule preserving shape, not literal timing; documented as a SPEC-language correction |
| A removed CalDAV/ICS account's provider keeps syncing (and can silently re-populate deleted events) until shell restart, since the provider vector is never mutated live | 4 | Medium | Explicitly out of scope for REQ-F-012 (cache-cleanup only); flagged as a follow-up phase candidate, not silently accepted |
| `tests/test_notification_rules.cpp`'s `WritesExpectedJson` and related tests read file state synchronously right after `persist()`, which becomes async | 3 | Low (expected, planned) | Test file updated in the same change to wait on `writeCompleted` via `QSignalSpy`, matching `test_notification_history.cpp`'s existing idiom for the identical pattern |
| `chmod`-based write-failure test simulation assumes non-root test execution | 3 | Low | Flagged for implementation-time verification of test-running UID; no filesystem-injection seam added (judged disproportionate) |
| `known_account_keys_`/removal-diff logic in `CalendarService` does not detect a newly *added* second account after `sync_manager_` already exists | 4 | Low (pre-existing, unchanged) | Explicitly scoped out — `initSyncManager()`'s single-shot guard is untouched; only removal detection is added |

## Non-Goals Respected

This design makes no change to `PulseAudioBackend`'s volume/mute null-callback behavior (Non-Goal 1,
U-04 [F-02]), does not touch `SessionCommandResult`/`SessionService::commandFailed` beyond reusing
their *convention* (Non-Goal 2), does not touch the launcher cache's watcher/mtime logic (Non-Goal 3),
makes no launcher UI/menu redesign beyond the keyboard-dispatch fix and its minimal highlight
(Non-Goal 4), does not address PulseAudio stream-move index conflation (Non-Goal 5, deferred per
SPEC), and no test plan in this design requires a live Hyprland/Wayland compositor, live PulseAudio
server, or live CalDAV server (Non-Goal 6) — every verification mechanism above is a GTest (with
`FakePulseAudioSystem`/`FakeCalendarProvider`/`FakeLauncherBackend` fakes), a `QSignalSpy`/
`QElapsedTimer` assertion, or a QML `TestCase` using `createTemporaryObject`. Only the two
highest-priority silent-failure instances named in SPEC's Non-Goal 7 (audio `healthState`,
notification-rule `rulePersistenceFailed`) receive the full convention-pattern rollout in this phase.

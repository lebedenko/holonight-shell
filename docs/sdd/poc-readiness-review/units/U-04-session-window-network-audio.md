# U-04 — Session, Window Identity, Network & Audio Services — Deep Review Findings

**Task**: T-004 · **Skill**: `qt-cpp-review` (Phase 1 deterministic lint + Phase 2 six-agent deep analysis) · **Scope**: 34 files, read-only

## Scope

- `libs/holonight-services/src/` root-level: `SessionService`, `ActiveWindowService`, `MonitorOccupancyService`, `SystemInfoService`, `NetworkService` (10 files)
- `libs/holonight-services/src/session/` — `CommandRunner`, `HyprlandSessionBackend`, `Locker`, `LogindSessionBackend`, `ProcessEnvironment`, `SessionBackend` (12 files)
- `libs/holonight-services/src/network/` — `NetworkManagerBackend`, `WifiNetworkModel` (4 files)
- `libs/holonight-services/src/audio/` — `AudioDeviceModel`, `AudioService`, `AudioStreamModel`, `AudioTypes`, `PulseAudioBackend`, `PulseAudioSystem` (8 files)

## Prior Context

Consulted per T-004 instructions: `docs/sdd/session-lock-backend/DESIGN.md`, CLAUDE.md "Per-Monitor Active Window" section, CLAUDE.md "NetworkManager WiFi: use `SpecificObject`... `Devices` returns empty in practice" gotcha. (`docs/sdd/audio-service/SPEC.md`+`DESIGN.md` referenced by the task list does not exist under that exact name in `docs/sdd/` — no equivalent audio-specific prior-cycle doc was found; treated as no prior-cycle context available for audio.)

**libpulse thread-boundary marshalling** (CLAUDE.md-implied, `pa_threaded_mainloop` direct C API usage): **verified exemplary.** Manually traced every registered PulseAudio callback (`contextStateCallback`, `subscribeCallback`, `serverInfoCallback`, `sinkListCallback`/`sourceListCallback`/`sinkInputListCallback`/`sourceOutputListCallback`, `sinkChangedCallback`/`sourceChangedCallback`/`sinkInputChangedCallback`/`sourceOutputChangedCallback`, and the sink/source/sink-input/source-output event handlers). Every path that ultimately touches Qt/QObject state converts the raw PulseAudio C struct (`pa_sink_info`, etc.) into a plain-data `AudioDevice`/`AudioStream` value **synchronously on the PulseAudio thread** (safe — only reading C struct fields into a Qt value type), then hands that POD value to `QMetaObject::invokeMethod(self, [self, dev]{ emit self->deviceAdded(dev); }, Qt::QueuedConnection)`. The actual signal emission always executes later, on the main thread. The one exception, `Impl::default_sink_name`/`default_source_name` (written in `serverInfoCallback`, read in `sinkToDevice`/`sourceToDevice`), is written and read exclusively from other PulseAudio-thread callbacks — never touched from the Qt main thread — so it is not a cross-thread race despite being unsynchronized. No agent's automated pass delivered a conclusive trace of this pattern despite being flagged as this unit's top-priority check, so it was verified directly by hand; documented here as a **strength worth preserving**, not a gap.

## Tool Sign-off — Phase 1 Deterministic Lint

71 raw lint hits. Excluded as noise, same rationale as prior units:

- **VAR-3** (46 hits, brace-init style) — not reported.
- **HDR-3** (2 hits, Windows-only) — not reported.

Three `MDL-7` hits (`AudioDeviceModel.cpp:30`, `AudioStreamModel.cpp:32`, `WifiNetworkModel.cpp:72`, all "`data()` switch has `default:`") were independently verified by the Model Contracts agent and are **all false positives** — `roleNames()` and each model's `data()` switch enumerate identical role sets with no gaps (6/6, 7/7, 11/11 respectively). Remaining categories (`PAT-2` ×11, `PAT-9` ×8 `QList<QString>`, `PAT-7` ×1 `QMap` copy) are low-severity; `PAT-7` (`NetworkManagerBackend.cpp:509`) is deepened below as [I-05].

## Confirmed Findings (confidence ≥ 80/100)

### [F-01] `PulseAudio` context failure/termination never triggers a reconnect — audio models go permanently stale until shell restart
- **Severity**: High
- **Effort**: M
- **Location**: `PulseAudioBackend.cpp:254-268` (`contextStateCallback`), `:408-438` (`start()`), `AudioService.cpp:145-151` (`setAvailable`)
- **Rationale**: When PulseAudio/PipeWire-pulse restarts mid-session (a routine event — driver reload, suspend/resume, session manager restart), `PA_CONTEXT_FAILED`/`PA_CONTEXT_TERMINATED` only triggers `availableChanged(false)`. Nothing calls `pa_context_connect` again, no retry timer exists, and `outputs_`/`inputs_`/`playback_streams_`/`recording_streams_` are never cleared — the UI keeps showing the last-known devices/streams/volumes indefinitely, frozen, with audio functionality never recovering without restarting the entire shell process. Confidence 88/100.
- **Suggested Direction**: On `PA_CONTEXT_FAILED`/`PA_CONTEXT_TERMINATED`, clear the device/stream models and add a bounded-backoff reconnect (`stop()` then `start()` again) once the server returns.

### [F-02] Every PulseAudio mutation call passes null success/error callbacks — user-triggered volume/mute/default-device changes fail completely silently
- **Severity**: Medium
- **Effort**: S
- **Location**: `PulseAudioBackend.cpp:460-609` (`setDeviceVolume`, `setDeviceMuted`, `setSourceVolume`, `setSourceMuted`, `setDefaultOutput`/`setDefaultInput`(`ByName`), `setStreamVolume`, `setStreamMuted`, `moveStreamToDevice`)
- **Rationale**: Every mutation is issued with `nullptr, nullptr` for the completion callback. The code only confirms the request was *queued* (`pa_operation*` non-null), never that it *succeeded*. If the target device/stream index has since disappeared server-side, the request fails silently with no log line, no signal, and no UI feedback — the slider simply doesn't reflect reality until an unrelated subscribe event happens to correct it. Confidence 85/100.
- **Suggested Direction**: Pass a real completion callback that logs and/or emits a failure signal (mirroring `NetworkManagerBackend`'s existing `operationError` pattern) on non-success.

### [F-03] Session lock/logout/suspend/reboot/shutdown command failures are silently discarded end-to-end
- **Severity**: High
- **Effort**: M
- **Location**: `session/SessionBackend.cpp:19` (`run()`), `session/Locker.cpp:42,48` (`lock()`)
- **Rationale**: `CommandRunner::run()` returns `bool` specifically to detect launch failure, but every call site discards it: `SessionBackend::run()` (used by `sleep()`/`reboot()`/`shutdown()`/`HyprlandSessionBackend::logout()`) is `void` and never checks the result; `Locker::lock()` does the same for both the daemon path and direct-locker path. All corresponding `SessionService` `Q_INVOKABLE`s are `void` with no failure signal. If the locker binary vanishes, or `loginctl`/`systemctl` isn't on `PATH`, the user presses "Lock" and the screen simply stays unlocked with zero indication beyond a log line almost no user will ever read — a real security-relevant UX gap. Confidence 85/100.
- **Suggested Direction**: Propagate the `bool` result up through `SessionBackend`/`SessionService` as a failure signal QML can surface as a toast/notification.

### [F-04] Wi-Fi activation only checks the immediate D-Bus reply, not the actual connection outcome — a wrong password produces no user-facing error
- **Severity**: Medium
- **Effort**: M
- **Location**: `network/NetworkManagerBackend.cpp:245-258` (`activateKnown`), `:712-729` (`addAndActivate`)
- **Rationale**: Only the synchronous `ActivateConnection`/`AddAndActivateConnection2` D-Bus reply is checked (NetworkManager accepted the request and created an `ActiveConnection` object) — the actual WPA handshake/authentication happens asynchronously afterward, and nothing subscribes to the resulting Active Connection's state/state-reason. A wrong PSK eventually surfaces only as the 2.5s poll reverting `connection_status` to "Disconnected," with no distinguishing message — the user can't tell "wrong password" from any other transient disconnect. Confidence 82/100.
- **Suggested Direction**: Subscribe to `PropertiesChanged`/`StateChanged` on the returned Active Connection object for a bounded window and emit a specific error (e.g. "Incorrect password" for `NM_ACTIVE_CONNECTION_STATE_REASON_NO_SECRETS`) when it lands in a failed state.

### [F-05] `setDefaultOutput(uint32_t)`/`setDefaultInput(uint32_t)` pass a stringified numeric index where PulseAudio expects the sink/source **name**
- **Severity**: Medium
- **Effort**: S
- **Location**: `PulseAudioBackend.cpp:516-542`
- **Rationale**: `QString::number(idx)` is passed to `pa_context_set_default_sink`/`pa_context_set_default_source`, which require the device's symbolic name (e.g. `"alsa_output.pci-..."`) — a stringified index like `"3"` will never match a real device, so this API is permanently non-functional against a live daemon, and (per [F-02]) fails silently. Currently unreached from production — the only QML caller (`AudioDeviceDelegate.qml:32,34`) correctly uses the `*ByName` variants — but the unit test (`test_pulse_audio_backend.cpp:488-492`) locks in the broken behavior as expected, making it a live landmine for any future caller. **Independently flagged by three separate agents** (Model Contracts 85/100, API & Correctness 85/100, Performance & Quality 82/100) — strong cross-agent corroboration.
- **Suggested Direction**: Remove the dead index-based overloads in favor of the working `*ByName` variants, or resolve `idx` → `name` (already tracked in `AudioDevice::name`) before calling the PulseAudio API, and fix the test to assert the resolved name.

### [F-06] `AudioService::onDeviceRemoved`/`onStreamRemoved` apply removals to both sibling models — sink/source (and sink-input/source-output) index namespaces collide, silently deleting an unrelated device
- **Severity**: High
- **Effort**: S
- **Location**: `AudioService.cpp:169-176` (`onDeviceRemoved`), `:188-191` (`onStreamRemoved`); root cause in `PulseAudioBackend.cpp:193-219` (`handleSinkEvent`/`handleSourceEvent` both emit a type-erased `deviceRemoved(idx)`)
- **Rationale**: PulseAudio sink indices and source indices are independent per-type counters — a common single-sink/single-source machine routinely has sink #0 and source #0 coexisting. `handleSinkEvent`/`handleSourceEvent` both collapse to the same untyped `deviceRemoved(idx)` signal despite the caller (`subscribeCallback`) already knowing the PA facility at dispatch time. `AudioService::onDeviceRemoved(idx)` then calls `outputs_->applyRemove(idx)` **and** `inputs_->applyRemove(idx)` unconditionally — removing sink #0 also silently evicts an unrelated, still-connected source #0 (e.g. a working microphone) from the input list. Same root cause affects `playback_streams_`/`recording_streams_` on stream removal. Confidence 82/100.
- **Suggested Direction**: Add a type parameter to `deviceRemoved`/`streamRemoved` (populated from the already-known facility) and route removal to only the matching model.

### [F-07] `PulseAudioBackend::moveStreamToDevice` always issues the sink-input move primitive, even when moving a recording stream
- **Severity**: Medium
- **Effort**: M
- **Location**: `PulseAudioBackend.cpp:598-609`, `PulseAudioSystem.h:64-65`
- **Rationale**: The only move primitive exposed is `pa_context_move_sink_input_by_index`, which operates exclusively on playback streams. `AudioService::moveStreamToInput` (for recording streams) forwards to the same function, so a recording-stream move is issued as a sink-input move using the source-output's numeric id — either failing outright or, worse, silently re-routing an unrelated playback stream sharing that id (same index-namespace-collision root cause as [F-06]). Currently unwired from QML, so latent rather than observed. Confidence 81/100.
- **Suggested Direction**: Add `pa_context_move_source_output_by_index` to `PulseAudioSystem`, and dispatch `moveStreamToDevice` on stream type.

### [F-08] `pa_operation` leaked from `pa_context_subscribe`
- **Severity**: Low
- **Effort**: S
- **Location**: `PulseAudioBackend.cpp:144-145` (`onContextReady`) — self-verified directly against the file
- **Rationale**: The returned `pa_operation*` is discarded without `pa_operation_unref`, unlike every other async PA call in this file, which correctly captures and unrefs. Fires once per successful `PA_CONTEXT_READY` transition — a small, bounded leak (not per-call in a hot path) but a real deviation from the file's otherwise-consistent refcounting discipline. Confidence 88/100.
- **Suggested Direction**: Capture and unref, matching the pattern used everywhere else in the file.

### [F-09] `SystemInfoService` blocks its constructor (and shell startup) on an unbounded, default-timeout blocking D-Bus call
- **Severity**: Medium
- **Effort**: S
- **Location**: `SystemInfoService.cpp:83-84`
- **Rationale**: `readAccountsService()` calls `QDBusInterface::call()` (default blocking mode, no timeout override) from inside the constructor, on the main thread, during startup. A slow-to-activate or hung `org.freedesktop.Accounts` service would stall the entire shell for the default D-Bus timeout (tens of seconds) with the whole process appearing frozen. Confidence 80/100.
- **Suggested Direction**: Use `asyncCall()` with a `QDBusPendingCallWatcher`, or a bounded explicit timeout.

### [F-10] `roleNames()` rebuilt on every call across all three list models in this unit
- **Severity**: Low
- **Effort**: S
- **Location**: `WifiNetworkModel.cpp:77`, `AudioDeviceModel.cpp:35`, `AudioStreamModel.cpp:37`
- **Rationale**: Each constructs a fresh `QHash` literal per call rather than caching a static table, for role sets that never change. Called by every QML delegate binding pass, potentially many times per second while a popup is open and animating. Confidence 85/100.
- **Suggested Direction**: Cache as `static const QHash<int, QByteArray>` in each model.

### [F-11] `AudioDeviceModel` and `AudioStreamModel` are near-duplicate model implementations
- **Severity**: Low
- **Effort**: M
- **Location**: `audio/AudioDeviceModel.{h,cpp}`, `audio/AudioStreamModel.{h,cpp}`
- **Rationale**: `applyAdd`/`applyChange`/`applyRemove`/`rowCount`/bounds-checking/`clear` are structurally identical (same linear-scan-by-id pattern, same begin/end bracketing) between the two, differing only in element type and role enum — already visibly drifted (`applyRemove` has no "not found" fallback while `applyChange` does). Confidence 82/100.
- **Suggested Direction**: Factor the shared id-scan/insert/replace/remove/reset logic into a shared template/CRTP base, keeping only `data()`/`roleNames()` in the derived classes.

### [F-12] `NetworkManagerBackend::updateVisibleWifiNetworks` pays for a full saved-connection enumeration before checking whether any Wi-Fi device exists
- **Severity**: Low
- **Effort**: S
- **Location**: `network/NetworkManagerBackend.cpp:352-382`
- **Rationale**: `savedWifiConnections()` (one `GetSettings` D-Bus round trip per saved connection *of every type*, not just Wi-Fi) runs unconditionally before `wirelessDevicePaths()` is even checked. On a machine with no Wi-Fi hardware or radio disabled (common on desktops), this wastes N+1 blocking D-Bus calls every 2.5-second poll tick for a result that's immediately discarded. Confidence 82/100.
- **Suggested Direction**: Check `wirelessDevicePaths()` first; short-circuit if empty.

### [F-13] D-Bus `QVariant` unwrapping, SSID-decoding, and signal-strength-clamping logic duplicated verbatim between `NetworkService.cpp` and `NetworkManagerBackend.cpp`
- **Severity**: Medium
- **Effort**: M
- **Location**: `NetworkService.cpp:27-46,108-125` vs `network/NetworkManagerBackend.cpp:43-60,62-80,395`
- **Rationale**: `NetworkService::activeConnectionPaths()` and the free function `parseObjectPathList()` are functionally identical implementations of the same `QDBusArgument` unwrap loop; SSID-decode-and-validate logic and the strength `std::clamp(raw, 0, 100)` are each independently re-implemented in both files. Both classes independently poll overlapping NetworkManager D-Bus state on separate timers, so the same wire format is parsed twice by unshared code — a future protocol-edge-case fix applied to one copy can silently miss the other. Confidence 82/100.
- **Suggested Direction**: Extract shared helpers (QDBusArgument→path-list unwrap, SSID decode) into a common location used by both files.

## Investigation Targets (confidence 60-79 — human verification needed)

Capped at 10 per skill protocol; two items at the 60/62 floor (a `const`-but-mutable model-getter pattern self-flagged by its own reviewing agent as likely not worth changing given Qt/QML model-binding conventions, and an unverifiable "might block the GUI thread" concern about `NetworkService::queryAll` that couldn't be confirmed without reading an out-of-scope file) were dropped to stay within the cap.

#### [I-01] Stale raw-pointer snapshot + reentrant `processEvents()` in `QtNetworkManagerBackend`'s destructor risks a use-after-free
- **Severity**: Medium · **Effort**: M · **Confidence**: 75/100
- **Location**: `network/NetworkManagerBackend.cpp:144-169`
- **Rationale**: The destructor busy-waits for in-flight `QtConcurrent::run` work via a tight `processEvents()` loop, iterating a **snapshot copy** of `operation_watchers_` while each watcher's `finished` handler mutates the live list and calls `deleteLater()` on completion. If a different watcher in the stale snapshot finishes and its `DeferredDelete` event drains mid-loop, a later iteration can dereference a freed `QFutureWatcher*`.
- **Suggested Direction**: Re-read `operation_watchers_.isEmpty()` per outer iteration instead of snapshotting once, or avoid `processEvents()`-based teardown entirely in favor of a mechanism that doesn't require pumping the event loop while iterating live QObject pointers.

#### [I-02] `ActiveWindowService` per-monitor state is never pruned on monitor hotplug/removal
- **Severity**: Low · **Effort**: M · **Confidence**: 72/100
- **Location**: `ActiveWindowService.cpp:15-46,92-116`, `.h:17-18`
- **Rationale**: `monitor_windows`/`monitor_workspaces` (both `QHash<QString,...>`) are only ever inserted, never removed — no Hyprland monitor-added/removed event is recognized, and unlike `PerMonitorLayerManager`/`SidebarManager`, this service never subscribes to `screenAdded`/`screenRemoved`. On unplug, stale entries persist indefinitely; self-corrects on the next full requery, so bounded impact.
- **Suggested Direction**: On a full `j/monitors` requery, diff against existing keys and remove entries for monitor names no longer present.

#### [I-03] `ActiveWindowService::focusedMonitor()` and `focusedMonitorName()` are 100%-identical duplicate getters
- **Severity**: Low · **Effort**: S · **Confidence**: 72/100
- **Location**: `ActiveWindowService.h:58-59`, `.cpp:144,146`
- **Rationale**: Both simply return `active_window_state_.focused_monitor_name` with no behavioral difference — two names inviting inconsistent call-site usage across the QML tree.
- **Suggested Direction**: Have one delegate to the other, or remove one and update call sites.

#### [I-04] Volume-set calls hardcode a 2-channel `pa_cvolume` regardless of the target device's actual channel count
- **Severity**: Low · **Effort**: S · **Confidence**: 70/100
- **Location**: `PulseAudioBackend.cpp:465,493,575`
- **Rationale**: `pa_cvolume_set(&vol, 2, ...)` is hardcoded before every volume-set call, without querying the actual channel map (mono mic, 5.1 speakers). Combined with [F-02]'s null error callbacks, a channel-mismatch rejection would silently no-op with zero indication on any non-stereo device.
- **Suggested Direction**: Query the device's current channel count (already available from the last `pa_sink_info`/`pa_source_info`) and build a channel-matched `pa_cvolume`.

#### [I-05] `NetworkManagerBackend::connectionId` performs an avoidable `QMap<QString,QVariantMap>`→`QVariantMap` copy (deepens `PAT-7`)
- **Severity**: Low · **Effort**: S · **Confidence**: 68/100
- **Location**: `network/NetworkManagerBackend.cpp:491-527`
- **Rationale**: Manually demarshals into an intermediate `QMap`, then copies every entry into a `QVariantMap` purely to later call `.value("connection")` — which could read directly from the original map. The sibling `savedWifiConnections()` extracts the same `GetSettings` shape via a plain `QDBusReply<QVariantMap>` with no manual handling at all, suggesting this path is more complex than needed. Runs once per 2.5s poll cycle.
- **Suggested Direction**: Drop the intermediate copy; consider unifying the two `GetSettings`-parsing paths.

#### [I-06] Duplicated `.desktop`-file "Categories"/`Name`/`Exec` INI-parsing state machine within `ActiveWindowService.cpp`
- **Severity**: Low · **Effort**: S · **Confidence**: 66/100
- **Location**: `ActiveWindowService.cpp:331-360` (`readCategoriesFromFile`) and `:362-402` (`scanSingleDesktopFile`)
- **Rationale**: Both independently re-implement the identical `[Desktop Entry]`-section-scan-and-key=value-split loop, differing only in which keys are extracted — a bug fix to section-tracking would need to land in both places. Runs on a worker thread with caching, so runtime impact is low; a maintenance smell, not a perf issue.
- **Suggested Direction**: Factor into one shared "parse `[Desktop Entry]` key/value pairs" helper.

#### [I-07] `QtNetworkManagerBackend` destructor's reentrant `processEvents()` can re-arm `watcher_` and restart async work mid-teardown
- **Severity**: Low · **Effort**: M · **Confidence**: 65/100
- **Location**: `network/NetworkManagerBackend.cpp:144-169,193-204`
- **Rationale**: Worker-thread lambdas always finish with a queued `refresh()` call; `QFuture::cancel()` doesn't interrupt an already-running task, so that queued call is still delivered. The destructor's `processEvents()` can deliver it mid-teardown, calling `watcher_->setFuture(...)` again and spawning new async D-Bus work on a partially-destructed object.
- **Suggested Direction**: Guard with a `stopping_` flag checked at the top of `refresh()`/`onQueryFinished()`/`emitError()`; avoid `processEvents()`-based teardown if possible.

#### [I-08] NetworkManager D-Bus call failures surface only a generic hardcoded message, dropping the actual error detail
- **Severity**: Low · **Effort**: S · **Confidence**: 65/100
- **Location**: `network/NetworkManagerBackend.cpp:251-254,264,279-283,722-726`
- **Rationale**: `emitError()` call sites use fixed generic strings, discarding `reply.error().message()` (which for NetworkManager typically carries the real cause — `NoSecrets`, `DeviceNotReady`, etc.), inconsistent with `property()`/`allProperties()` in the same file, which do log the D-Bus error detail.
- **Suggested Direction**: Thread the actual D-Bus error message into the `emitError()` payload.

#### [I-09] Two independent, overlapping polling loops re-query overlapping NetworkManager state on different cadences
- **Severity**: Low · **Effort**: M · **Confidence**: 65/100
- **Location**: `NetworkService.cpp:141` (2000ms), `network/NetworkManagerBackend.cpp:138` (2500ms)
- **Rationale**: `NetworkService` and its injected `NetworkManagerBackend` each own an independent, unsynchronized poll timer, both querying overlapping primary-connection/type state. `NetworkService::onPollTimer()` fires both on every tick — largely redundant D-Bus traffic for what should be one polling source of truth.
- **Suggested Direction**: Consolidate to a single poll owner and have the other side rely on signals.

#### [I-10] `NetworkService::backend_` is double-owned via `unique_ptr` and `setParent(this)`
- **Severity**: Low · **Effort**: S · **Confidence**: 62/100
- **Location**: `NetworkService.cpp:134-148`, `.h:156`
- **Rationale**: `backend_` is a `std::unique_ptr<NetworkManagerBackend>` member that is additionally `setParent(this)`'d, self-parenting into the instance that already owns it. Not currently a double-free (destruction order happens to be safe today), but a fragile redundant-ownership idiom that a future refactor (`release()`, raw-pointer conversion) could turn into one.
- **Suggested Direction**: Drop `setParent(this)` and rely solely on the `unique_ptr`, or switch to a raw non-owning pointer if `QObject::children()` enumeration is genuinely needed.

## Summary

| Category | Lint (reported) | Deep (confirmed ≥80) | Investigation (60-79) | Total |
|---|---|---|---|---|
| Model Contracts | 3 (MDL-7, all refuted) | 3 (merged into F-05/06/07) | 0 | 0 |
| Ownership & Lifecycle | 0 | 1 (F-08) | 2 | 3 |
| Thread Safety | 0 | 0 (verified clean — see Prior Context) | 2 | 2 |
| API & C++ Correctness | 0 | 2 (merged into F-05/F-13) | 2 | 2 |
| Error Handling & Validation | 0 | 5 (F-01,02,03,04,09) | 2 | 7 |
| Performance & Code Quality | PAT-7 (1, deepened as I-05) | 4 (F-05,06,10,11,12 — some merged) | 3 | 7 |
| **Total** | **71 raw / 3 refuted / rest low-value** | **13** | **10** | **23 actionable** |

23 actionable items (13 confirmed + 10 capped investigation targets) — the largest and highest-risk finding set so far, proportionate to this unit's breadth (session lock security, direct libpulse C API, NetworkManager D-Bus). The libpulse thread-marshalling pattern — flagged as this unit's top-priority check — was manually verified **clean and exemplary**, a genuine architectural strength worth calling out explicitly (every callback correctly copies POD data on the PulseAudio thread and marshals via `Qt::QueuedConnection` before touching any QObject). Set against that, the confirmed findings cluster around two real risk areas: **silent failure paths** ([F-01] no PulseAudio reconnect, [F-02] no PulseAudio mutation error feedback, [F-03] silent session-lock command failures, [F-04] silent Wi-Fi auth failures, [F-09] blocking constructor) and **index/namespace-space bugs in the PulseAudio integration** ([F-05], [F-06], [F-07] — all stemming from conflating sink/source/sink-input/source-output numeric indices, which are independent per-type counters in PulseAudio's actual API contract). **[F-03]** (silent lock/logout failures) deserves particular priority given its security-adjacent nature — a user who believes their screen locked when it didn't is a real-world risk, not just a UX papercut.

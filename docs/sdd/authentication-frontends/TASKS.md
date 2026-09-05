# Authentication Frontends — Implementation Tasks

Fresh delivery evidence is recorded in [ACCEPTANCE.md](ACCEPTANCE.md). Unchecked
items can have substantial implementation already present; their exact stated
acceptance checks still have the gaps listed in that record. Historical notes
below are retained as history, not substituted for fresh verification. T-016
and T-024 were reopened because the historical evidence did not cover every
claimed contract/environment case.

## 1. Delivery Strategy

Tasks are ordered so each implementation slice has a narrow verification seam
before protocol and session integration is added:

```text
build foundations
  -> neutral model and security utilities
  -> standalone QML module
  -> askpass process and contract tests
  -> Polkit listener/session coordinator
  -> installation and session activation
  -> full and live verification
```

Every task is intended to be reviewable independently. No task may introduce a
secret-bearing shell API, shell import, persistent response cache, or automatic
replacement of a third-party Polkit agent.

## 2. Build and Core Foundations

- [x] T-001: Add required authentication dependencies and target skeletons
  - Add configuration-time checks for Qt 6 Quick/QML, PolkitQt6 Agent 0.200.0+
    and `polkit-agent-1`.
  - Declare the authentication core library, standalone QML module,
    `holonight-polkit-agent`, and one askpass implementation target without
    linking shell-owned targets.
  - Keep OpenSSH 8.2+ and sudo 1.8.15+ as documented runtime baselines.
  - REQs: REQ-F-001, REQ-F-008, REQ-F-018, REQ-I-001, REQ-NF-001
  - Check: configuration succeeds with supported dependencies, fails with an
    actionable diagnostic for missing/old PolkitQt, and the link graph contains
    no `holonight-shell`, shell service, or layer-shell dependency.

- [ ] T-002: Implement external-text normalization and safe Polkit details
  - Normalize line endings and controls, apply byte/code-point/line bounds,
    return plain text only, and allowlist display-safe Polkit detail keys.
  - Treat action IDs as inert request references and never as paths or URLs.
  - REQs: REQ-F-010, REQ-S-005, REQ-NF-003
  - Check: focused unit tests cover markup, links, paths, shell-like text, NUL,
    C0/C1 controls, excessive lines, truncation boundaries, and rejected detail
    keys without loading resources or executing anything.

- [x] T-003: Implement askpass secret validation
  - Validate non-empty input, UTF-8 round-trip, absence of CR/LF/NUL, and the
    1022-byte encoded maximum.
  - Return only a validated byte array or a fixed safe error classification.
  - REQs: REQ-F-025, REQ-S-003, REQ-C-003
  - Check: focused unit tests accept valid Unicode and exactly 1022 bytes, and
    reject empty, malformed/unpaired Unicode, forbidden characters, and 1023+
    bytes without retaining the input.

- [ ] T-004: Implement identity and message list models
  - Expose identity `stableId`/`displayLabel` roles and message
    `severity`/`text` roles through dedicated `QAbstractListModel` types.
  - Preserve authority order and use stable identities rather than labels for
    equality and selection.
  - REQs: REQ-F-003, REQ-F-011, REQ-F-012
  - Check: model tests verify roles, order, duplicate-position preservation,
    stable-ID selection, current-UID preference, first-entry fallback, and an
    empty identity list.

- [ ] T-005: Implement the neutral authentication prompt state machine
  - Add all specified properties, `InputMode` and `LifecycleState` values, and
    invokables to `AuthenticationPromptModel`.
  - Keep controller-only transitions separate from QML operations and deliver
    responses through one-shot process-local callbacks only.
  - REQs: REQ-F-002, REQ-F-003, REQ-F-004, REQ-S-001, REQ-S-003
  - Check: table-driven unit tests cover every legal transition, every invalid
    operation/mode combination, retry, cancellation, completion idempotency,
    and callback count without starting any external protocol.

- [x] T-006: Add lifecycle and response-exposure regression tests
  - Verify synchronous clearing events for submit, cancel, failure, retry,
    completion, shutdown, new request token, and session generation.
  - Scan model properties, signals, logs, and test persistence locations for a
    marker response after every transition.
  - REQs: REQ-F-005, REQ-S-002, REQ-S-003, REQ-NF-001
  - Check: all marker scans remain empty outside the single response callback,
    and no model property or shell-facing channel exposes the response.

## 3. Standalone QML Presentation

- [x] T-007: Create and register `Holonight.Authentication`
  - Register the module under its own resource prefix with dependencies limited
    to Qt Quick and installed `Holonight`, `Holonight.Core`, and
    `Holonight.Controls` modules.
  - Add authentication-specific prompt, identity, and message components.
  - REQs: REQ-F-001, REQ-F-002, REQ-F-007, REQ-NF-001
  - Check: an independent QML harness imports the module and instantiates its
    root component while `rg` and runtime import tracing find no
    `HolonightShell` or shell singleton dependency.

- [ ] T-008: Implement the compositor-managed authentication dialog
  - Use a regular application-modal Qt top-level window, request center/focus,
    constrain hostile content, and render every neutral model state.
  - Do not introduce layer-shell protocols or placement guarantees.
  - REQs: REQ-F-004, REQ-F-007, REQ-NF-005
  - Check: the QML harness transitions through identity selection, input,
    confirmation, notification, busy, retryable error, completion, and cancel
    without recreating the component; delayed fake work leaves it responsive.

- [x] T-009: Harden secret entry and accessibility behavior
  - Mask secret input, suppress selection/copy/cut and input-method learning,
    clear the control synchronously on every terminal or superseding edge, and
    exclude entered text from accessibility metadata.
  - Provide meaningful roles/names/state for all non-secret controls.
  - REQs: REQ-F-005, REQ-S-003, REQ-S-006, REQ-NF-004
  - Check: QML tests verify echo mode, copy suppression, input-method hints,
    clearing, accessible roles/names/states, and absence of the marker secret.

- [x] T-010: Complete keyboard, focus, busy, and palette behavior
  - Implement deterministic Tab/Backtab order, arrow identity navigation,
    Enter acceptance, Escape cancellation, retry focus, busy disabling, and
    visible focus indication in light and dark variants.
  - REQs: REQ-F-006, REQ-F-007, REQ-V-004
  - Check: offscreen tests complete secret, confirmation, identity, retry, and
    cancel flows without pointer input and assert busy controls cannot respond;
    palette tests exercise both variants.

## 4. Askpass Executable

- [ ] T-011: Implement basename-driven askpass mode selection
  - Map the sudo, SSH, and compatibility basenames exactly as designed, using
    only `argv[0]` and `SSH_ASKPASS_PROMPT`.
  - Normalize only the prompt argument; never infer mode from the parent.
  - REQs: REQ-F-018, REQ-F-021, REQ-F-022, REQ-F-023, REQ-F-024
  - Check: table-driven tests cover every basename with unset, `confirm`,
    `none`, recognized secret, and unknown hint values.

- [x] T-012: Implement the stdout protocol writer
  - Write a validated payload plus one LF with partial-write and `EINTR`
    handling; reserve file descriptor 1 exclusively for protocol output.
  - Define accepted status `0` and rejection/cancellation/failure status `1`.
  - REQs: REQ-F-019, REQ-F-020, REQ-F-021, REQ-F-025, REQ-F-026
  - Check: unit tests with an injectable write seam cover exact bytes, partial
    writes, interruption, zero writes, hard failure, and no extra LF or log
    output.

- [ ] T-013: Implement askpass dialog control and terminal outcomes
  - Connect secret acceptance to validation/writing, confirmation to status
    only, and all cancellation/setup/internal failures to empty stdout.
  - Ensure each process creates at most one prompt and terminates once.
  - REQs: REQ-F-018, REQ-F-019, REQ-F-020, REQ-F-022, REQ-NF-002
  - Check: controller tests assert exactly one terminal outcome, empty stdout on
    all failed/non-secret paths, and correction after validation failure.

- [x] T-014: Implement notification mode and Qt-safe `SIGTERM`
  - Present no editable or response controls and translate `SIGTERM` through a
    self-pipe or `signalfd`/`QSocketNotifier` path into clean Qt shutdown.
  - REQs: REQ-F-023, REQ-V-003, REQ-NF-002
  - Check: a real subprocess remains visible/active until `SIGTERM`, writes
    empty stdout, has no response path, and exits without a crash or error
    diagnostic.

- [x] T-015: Add pre-Qt process hardening and safe logging
  - Set `RLIMIT_CORE=0` before application construction and fail closed if that
    boundary cannot be established.
  - Log only frontend, phase, opaque token, and fixed safe classifications to
    stderr/journal; install no crash handler.
  - REQs: REQ-S-002, REQ-S-007, REQ-F-026, REQ-NF-003
  - Check: process tests observe a zero core limit, protocol-clean stdout under
    verbose logging, no marker response in logs, and no crash-report artifact
    from an intentional test-build crash.

- [ ] T-016: Add the inherited test-control pipe and subprocess suite
  - Compile the inherited control-FD seam only with `BUILD_TESTS`; never place
    response bytes in argv, environment, logs, or files.
  - Execute the real helper under all three entry names and capture raw stdout,
    stderr, and status independently.
  - REQs: REQ-V-002, REQ-V-003, REQ-V-005, REQ-S-001, REQ-S-002
  - Check: contract cases cover success, correction, cancellation, dismissal,
    setup/internal failure, UTF-8 boundaries, CR/LF/NUL, 1022/1023 bytes,
    compatibility precedence, confirmation, and notification termination.
  - Historical verification (before this delivery, 2026-09-05): the `BUILD_TESTS`-only inherited FD 3
    protocol and real-process suite pass for all three entry names. The suite
    independently checks raw stdout/stderr/status, correction and cancellation,
    UTF-8 and 1022/1023-byte boundaries, confirmation/notification, SIGTERM,
    and the zero core limit without placing response bytes in argv, environment,
    logs, or files.

- [x] T-017: Install the three askpass entry points
  - Install `holonight-sudo-askpass`, `holonight-ssh-askpass`, and
    `holonight-askpass` backed by the one implementation.
  - REQs: REQ-F-018, REQ-I-001
  - Check: a staged install contains three executable entry names resolving to
    the same implementation, and each passes its basename contract tests.

## 5. Polkit Agent

- [ ] T-018: Implement the raw `libpolkit-agent-1` listener bridge
  - Register for an explicitly supplied session subject, retain cookie and
    `GCancellable`, preserve authority identity order, and post callbacks to the
    Qt event loop.
  - Do not use PolkitQt's high-level `Listener` adapter.
  - REQs: REQ-F-009, REQ-F-010, REQ-F-011, REQ-NF-005
  - Check: an injected authority observes one correct registration and ordered
    request/cancellation mapping without blocking the render thread.

- [x] T-019: Implement immutable request records and the FIFO coordinator
  - Give every request an opaque token, ordered identities, cancellation
    handle, completion guard, and session generation.
  - Own one active request, queue later arrivals, remove cancelled queued
    requests, and activate the next request asynchronously.
  - REQs: REQ-F-015, REQ-F-017, REQ-NF-002
  - Check: deterministic tests cover FIFO presentation, queued cancellation,
    active cancellation, reentrant completion, and exactly one terminal result
    for every record.

- [x] T-020: Implement Polkit identity selection policy
  - Prefer an eligible current UID, otherwise the first authority identity;
    retain original order and require explicit choice when multiple identities
    are available.
  - Complete an empty list unsuccessfully once without creating a window.
  - REQs: REQ-F-011, REQ-F-012, REQ-V-001
  - Check: focused tests cover current-user, administrator-only, first-entry,
    multi-identity explicit selection, stable IDs rather than labels, and empty
    identity behavior.

- [x] T-021: Implement the PolkitQt PAM session adapter
  - Create one `PolkitQt1::Agent::Session` for the selected identity/cookie and
    map ordered visible/hidden prompts plus information/error messages.
  - Route each response only to its originating session.
  - REQs: REQ-F-013, REQ-S-001, REQ-S-004, REQ-NF-005
  - Check: an injectable session factory drives at least two mixed prompts and
    messages in order; no stdout, D-Bus, shell, or control-socket response path
    exists.

- [x] T-022: Implement retry, cancellation, and stale-callback defenses
  - On retry, destroy the old session, clear response state, increment the
    generation, permit identity reselection, and construct a fresh session.
  - Reject every callback whose token or generation is no longer active.
  - REQs: REQ-F-014, REQ-F-015, REQ-F-017, REQ-S-003
  - Check: tests cover failed-then-successful retry, identity change, user,
    authority, and session cancellation, shutdown, and late callbacks that
    cannot reopen or complete another request.

- [x] T-023: Implement agent startup and registration-conflict handling
  - Apply pre-Qt core-dump hardening, validate the bound session, register once,
    and keep the process persistent until orderly shutdown.
  - Exit with the dedicated non-retriable conflict status without touching the
    competing agent; classify other startup failures separately.
  - REQs: REQ-F-008, REQ-F-009, REQ-F-016, REQ-S-007, REQ-I-006
  - Check: mock registration tests cover success, transient failure, and
    conflict; conflict produces a safe actionable diagnostic, preserves the
    existing registration, and invokes no service-management command.

- [ ] T-024: Complete deterministic Polkit integration coverage
  - Compose the real model/coordinator with fake authority and session seams.
  - Add a two-session, one-UID scenario with different session IDs and Wayland
    environments.
  - REQs: REQ-V-001, REQ-V-005, REQ-NF-002
  - Check: focused integration tests cover mapping, multi-step PAM, retries,
    FIFO/cancellation races, exactly-once completion, conflict, and distinct
    per-session routing without a live authority.
  - Historical verification (before this delivery, 2026-09-05): injectable registration hooks compose
    the real listener bridge, coordinator, and prompt model with fake PAM
    sessions. Registration conflict preservation and two distinct session IDs
    for one UID pass without a live authority.

## 6. Installation and Session Activation

- [x] T-025: Install the session-keyed Polkit user unit
  - Add `holonight-polkit-agent@.service` independent of the shell with
    `LimitCORE=0`, bounded `Restart=on-failure`, and
    `RestartPreventExitStatus` matching the executable's conflict status.
  - Order it with the HoloNight graphical-session target without globally
    enabling it or binding it to `holonight-shell.service`.
  - REQs: REQ-I-002, REQ-I-003, REQ-I-006, REQ-C-001
  - Check: staged unit inspection verifies exact lifecycle directives and a
    conflict simulation does not restart-loop or mutate a third-party unit.
  - Fresh evidence: installed runtime instance exited 78 with NRestarts=0 after
    the restart window; the competing agent remained active at the same PID.

- [x] T-026: Implement the exact-session startup wrapper
  - Treat the service instance as the required `XDG_SESSION_ID`; wait a bounded
    time for its logind session and usable Wayland environment, validate owner
    and session type/state, capture the environment, then `exec` the agent.
  - Refuse mismatched mutable user-manager state instead of selecting another
    graphical session.
  - REQs: REQ-F-009, REQ-I-002, REQ-I-003, REQ-NF-002
  - Check: isolated script tests cover delayed compositor readiness, timeout,
    mismatched global state, direct post-compositor activation, and two sessions
    for one UID without cross-routing.
  - Fresh evidence: seven real Unix-socket peer tests and wrapper regressions
    cover UWSM routing, owner/session/runtime mismatches, ambiguity and failure.
    Staged service activation and real Polkit authentication passed in session 4.

- [x] T-027: Export askpass paths from HoloNight session activation
  - Export absolute installed `SUDO_ASKPASS` and `SSH_ASKPASS` paths to process
    descendants, the systemd user manager, and D-Bus activation.
  - Do not export `SSH_ASKPASS_PROMPT`, use the compatibility entry point, or
    read/write `/etc/sudo.conf`.
  - REQs: REQ-I-004, REQ-I-005, REQ-C-002
  - Check: isolated session tests observe the correct values in direct,
    systemd-user, and D-Bus-activated processes and verify no sudo configuration
    write attempt or global prompt-mode export.

- [x] T-028: Complete install-manifest and architecture checks
  - Install both executable implementations, all askpass names, QML resources
    and qmltypes, the service, wrapper, and explicit session hook.
  - Add boundary checks that forbid shell/module/service/IPC dependencies.
  - REQs: REQ-I-001, REQ-S-001, REQ-S-002, REQ-S-004, REQ-NF-001
  - Check: a staged install contains every artifact, QML type metadata is
    populated, dependency inspection is clean, and no package/install script
    manipulates another agent or `/etc/sudo.conf`.

## 7. Final Verification

- [x] T-029: Run focused and repository-wide automated verification
  - Run focused core, QML, askpass subprocess, Polkit integration, session
    wrapper, and install tests before the complete project checks.
  - REQs: REQ-V-007
  - Check: record outcomes for the focused commands followed by `task test`,
    `task qml-lint`, `task qmltypes-check`, and `task architecture-check`,
    distinguishing feature regressions from environmental or pre-existing
    failures.
  - Historical verification (before this delivery, 2026-09-05): focused authentication C++/process/
    Polkit integration tests passed (22/22); focused offscreen QML passed
    (6/6); wrapper, session-launcher, and staged-install scripts passed.
    `dbus-run-session -- task test` passed 1117/1117. `task qml-lint` passed
    with the existing unresolved `AudioService` warnings; `task qmltypes-check`
    and `task architecture-check` passed. `task format-check` initially found
    formatting in the new T-029 sources, which was corrected and rechecked.
    `task tidy` remains failing on authentication code introduced before T-029,
    primarily project-style diagnostics for braces, enum storage, nodiscard,
    special members, and designated initializers; no unrelated test failure was
    observed. Live acceptance was not performed and remains T-030.

- [ ] T-030: Perform the live Hyprland acceptance matrix
  - Guided continuation (2026-09-05): real sudo cancellation and authentication
    passed; OpenSSH forced passphrase, Allow, and Reject passed with a disposable
    encrypted key and isolated agent. Approved real Polkit authentication via
    `pkexec --disable-internal-agent /usr/bin/true` passed, and the original
    agent was restored active/running. The user confirmed Polkit initial focus,
    Enter submission, and clean closing. Installed UWSM service activation,
    non-restarting conflict handling and real authentication now pass. A SIGTERM
    shutdown veto found during acceptance was fixed and verified with an isolated
    process regression and a 54 ms clean live stop. The remaining live checks
    are still pending; see ACCEPTANCE.md.
  - Exercise daemon reload and explicit instance activation, a real test policy,
    `sudo -A`, forced SSH passphrase and confirmation modes, notification
    termination, a competing Polkit agent, keyboard focus, and dark/light
    styling. Exercise two graphical sessions when the environment permits.
  - Set `SSH_ASKPASS_REQUIRE=force` for OpenSSH checks and never capture a typed
    response in screenshots or logs.
  - REQs: REQ-V-006, REQ-F-006, REQ-F-007, REQ-I-003, REQ-I-006
  - Check: the smoke record lists every command and observed result, confirms
    the conflict is non-destructive and non-looping, verifies regular
    compositor-managed placement, and includes safe screenshots for both
    palettes.

- [x] T-031: Complete the security and scope audit
  - Inspect public interfaces, process arguments/environment, logs, IPC,
    persistence, crash behavior, and install/session scripts with marker data.
  - Confirm no credential cache and no support for out-of-scope password-agent,
    disk-unlock, keyring, browser, or application-login flows was introduced.
  - Record the compositor, input-method, kernel/root, and same-UID memory
    inspection boundaries without claiming secure erasure.
  - REQs: REQ-S-001, REQ-S-002, REQ-S-003, REQ-S-004, REQ-S-005, REQ-S-006,
    REQ-S-007, REQ-C-003, REQ-C-004, REQ-C-006
  - Check: marker scans find no response outside the intended PAM/stdout
    channels, core/crash artifacts are absent, and the final change report
    documents all operational boundaries.

## 8. Completion Criteria

The feature is complete only when all 31 tasks are checked and their stated
checks are recorded. In particular:

- the neutral model and QML module work without either protocol or the shell;
- all askpass entry names pass real-process byte/status contracts;
- the Polkit coordinator passes retry, cancellation, FIFO, stale-callback, and
  two-session tests with exactly-once outcomes;
- installation activates nothing outside an explicit HoloNight session and
  preserves third-party agents and sudo configuration;
- focused, full, lint, qmltypes, architecture, security, and live checks have
  recorded outcomes.

## 9. Requirement Coverage

<!-- markdownlint-disable MD013 -->

| Requirement group | Tasks |
| --- | --- |
| REQ-F-001–REQ-F-007 | T-001, T-005–T-010, T-030 |
| REQ-F-008–REQ-F-017 | T-018–T-024, T-026 |
| REQ-F-018–REQ-F-026 | T-011–T-017 |
| REQ-S-001–REQ-S-007 | T-005–T-006, T-009, T-015–T-016, T-021–T-023, T-028, T-031 |
| REQ-I-001–REQ-I-006 | T-001, T-017, T-023, T-025–T-028, T-030 |
| REQ-V-001–REQ-V-007 | T-010, T-014, T-016, T-020, T-024, T-029–T-030 |
| REQ-NF-001–REQ-NF-005 | T-001, T-006–T-009, T-013–T-015, T-018–T-019, T-021, T-024, T-026, T-028 |
| REQ-C-001–REQ-C-006 | T-003, T-006, T-025, T-027, T-031; this task artifact fulfills REQ-C-005 |

<!-- markdownlint-enable MD013 -->

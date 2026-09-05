# Authentication frontend acceptance — 2026-09-05

## Delivery status

Automated verification passes. Full feature acceptance remains incomplete.
The installed service wrapper now discovers the validated UWSM session through
the Wayland socket peer's verified process/session identity. Guided sudo authentication and
OpenSSH passphrase/confirmation and real Polkit authentication checks now
pass. With explicit user approval, hyprpolkitagent was temporarily stopped
and restored after the Polkit check. No sudo policy, session-manager
environment, or system unit file was modified. The pre-existing uncommitted implementation was extended in place;
nothing was committed or discarded.

This record supersedes historical verification notes in TASKS.md. Checkboxes
represent the stated acceptance evidence, not merely the presence of code.

## Changes in this delivery

- Added Authentication QML to the repository lint target and made generated
  authentication type metadata a dependency. Extended qmltypes packaging checks.
- Added deterministic Tab/Backtab traversal, preferred-identity keyboard focus,
  retry and prompt focus restoration, cancellable busy states, explicit
  accessible names, palette propagation, and inert requester-detail/identity
  presentation. Window closing cancels through the model and preserves the
  persistent agent's window for later requests.
- Made each input/identity operation enter Busy before its controller callback,
  preventing duplicate delivery. Cancellation now publishes terminal state
  before calling outward, preserving a controller's synchronous reset.
- Rejected callbacks from failed sessions before retry and callbacks after
  coordinator destruction. Explicitly queued the PolkitQt completion callback
  so session destruction does not occur inside its completion signal.
- Corrected supplementary-Unicode truncation and zero-sized bounds. Filtered
  requester details at the neutral model boundary as well as the listener.
- Added pre-Qt `PR_SET_DUMPABLE=0` alongside `RLIMIT_CORE=0` to both processes,
  SIGPIPE failure handling for askpass, orderly SIGTERM handling for the agent,
  and distinct notification/secret termination outcomes.
- Corrected the wrapper to query RuntimePath on logind User, not Session.
  The script mock now enforces this distinction.
- Added a Python 3 standard-library helper for compositors managed outside the
  logind session scope. It verifies socket peer UID/PID and original session
  environment before exporting an allowlist; ambiguous matches fail closed.
  Added real Unix-socket peer tests and installed the helper beside the wrapper.
- Corrected Polkit SIGTERM handling to exit the event loop despite the persistent
  window's close veto. A private-authority process regression and a repeated live
  service authentication/stop check verify cleanup and unregistration.
- Resolved authentication clang-tidy/formatting diagnostics. Narrow suppressions
  document unavoidable GObject/POSIX boundaries, Qt integer roles, and the
  intentionally explicit state/mode test matrix.

## Fresh verification

Commands were run from the repository root unless stated otherwise. Logs named
below are local `/tmp` artifacts and are not packaged or persistent audit logs.

| Check | Result |
| --- | --- |
| `task configure-tests` | Passed with staged sibling dependencies; sandbox escalation was needed for sibling build directories. |
| `build/tests/test_holonight_authentication` | 33/33 passed; `/tmp/auth-focused.log`. Includes real subprocesses and injected Polkit integration. |
| QML harness with `-input tests/qml/tst_AuthenticationDialog.qml` | 12/12 in each palette using isolated, complete dark/light appearance TOML files; `/tmp/auth-qml-dark.log`, `/tmp/auth-qml-light.log`. |
| `dbus-run-session -- task test` | Latest run 1130/1130 passed in 178.11 s, including peer routing and production-agent shutdown; `/tmp/auth-final-service-tests.log`. Earlier delivery run: 1128/1128 in `/tmp/auth-full-test-final.log`; an initial palette import/assertion failure was corrected. |
| `task qml-lint`, followed by final `cmake --build build --target qml-lint` | Passed; authentication files included. Only seven pre-existing unresolved `AudioService` warnings remain in shell audio QML. |
| `task qmltypes-check` | Passed, including populated AuthenticationPromptModel metadata and all four authentication QML entries. |
| `task architecture-check` | Passed. |
| `task format-check`, final `cmake --build build --target format-check` | Passed. |
| `task tidy` | Passed after corrections; `/tmp/auth-tidy-final.log`. Final focused run across all 16 authentication production/test translation units also passed; `/tmp/auth-tidy-final-focused.log`. |
| `bash tests/test_polkit_session_wrapper.sh "$PWD"` | Passed after correcting the real logind API mismatch. |
| `ctest --test-dir build --output-on-failure -R 'test_(wayland_session_environment\|polkit_session_wrapper\|authentication_install)$'` | 3/3 passed; peer discovery includes seven Python scenarios. |
| `ctest --test-dir build --output-on-failure -R '^PolkitAgentProcess\.'` | Failed its five-second shutdown deadline before the fix; passed afterward in 0.40 s. |
| UWSM continuation lint/metadata/format/architecture | `cmake --build build --target qml-lint format-check`, `scripts/check-qmltypes.sh build`, and `task architecture-check` passed; the same seven unrelated AudioService warnings remain. |
| Shutdown-focused tidy | `cmake -P build/strip_tidy_flags.cmake`, then `run-clang-tidy -quiet -p build/tidy apps/authentication/polkit/main.cpp tests/test_polkit_agent_process.cpp`: passed. |
| `bash tests/test_authentication_install.sh "$PWD" "$PWD/build"` | Passed staged executable/unit/qmltypes/dependency and forbidden-mutation checks. Also included in the full CTest run. |
| `tests/test_session_scripts.sh` | Passed in full CTest; observes descendant exports and activation-tool arguments through isolated mocks. |
| Missing/old PolkitQt configuration probes | The exact `pkg_check_modules(... polkit-qt6-agent-1>=0.200.0)` requirement rejected missing metadata and 0.199.0 with package/version diagnostics. These were isolated CMake probes, not complete alternate toolchains. |
| Release configuration/build | `cmake -S . -B /tmp/holonight-auth-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DCMAKE_PREFIX_PATH='/tmp/holonight-qt-prefix;/tmp/holonight-config-prefix;/tmp/holonight-system-services-prefix'`, then build targets `holonight-askpass-bin holonight-polkit-agent`: passed. |
| Release test-control exclusion | Binary contained no `HNAP` marker; a real process ignored an inherited FD-3 cancellation frame, stayed running, and then exited 1 on SIGTERM with empty stdout/stderr. |
| `task compositor-smoke-check` | Passed checklist command in live Hyprland. It prints checks; this is not evidence that all shell smoke interactions were performed. Shell control socket was absent. |
| Fixture validation | Acceptance probe passed `sh -n`; policy passed XML parsing. Neither fixture was installed. |

The isolated D-Bus full run emitted keyring/bus teardown messages after successful
CTest completion (`A connection to the bus can't be made`, a missing child in
cleanup). Exit status was 0; no failed test is attributed to those messages.

## Task-to-evidence map

Paths in this table are relative to the repository root. “Partial” means the
checkbox remains open even where implementation exists.

| Task | Implementation and fresh evidence | Status / remaining check |
| --- | --- | --- |
| T-001 | Root/library/app/QML CMake targets; supported configuration, missing/old package probes, install/architecture checks | Complete |
| T-002 | `ExternalText.cpp`; `AuthenticationText.*`, hostile QML context test | Partial: dedicated C0/C1, markup/link/path/shell-like unit matrix and all truncation boundary combinations are not exhaustive. |
| T-003 | `SecretValidator.cpp`; `AuthenticationSecret.*`, askpass byte/Unicode process cases | Complete |
| T-004 | `IdentityListModel.cpp`, `MessageListModel.cpp`; identity-order, duplicate, UID/fallback and mixed-message coordinator tests | Partial: complete list-model roleNames/invalid-index/message-role contract tests are missing. |
| T-005 | `AuthenticationPromptModel.cpp`; rejected invokable state/mode matrix, prompt consumption and coordinator flows | Partial: exhaustive controller-only legal/rejected transition matrix is missing. |
| T-006 | `ClearsEverySensitiveEdgeAndNeverPublishesResponse`; process marker audit below | Complete within documented process/security boundaries |
| T-007 | Standalone module, populated qmltypes, both real helpers loading it, boundary checks | Complete |
| T-008 | `AuthenticationDialog.qml`; identity/input/confirmation/notification/busy/retry QML tests | Partial: a single-component QML test cycling Idle/Completed/Cancelled and delayed fake work is still missing. |
| T-009 | `AuthenticationPrompt.qml`; synchronous clear, selection/keyboard copy/cut suppression, input hints, accessible metadata tests | Complete |
| T-010 | Keyboard secret/confirmation/identity/retry/cancel and busy tests; both palette runs | Complete for automated acceptance; wider live input checks are T-030. |
| T-011 | `AskpassMode.cpp`; basename/hint unit and real-process cases | Partial: every basename crossed with every unset/recognized/unknown hint is not table-driven yet. |
| T-012 | `ProtocolWriter.cpp`; EINTR/partial/zero/hard failure tests | Complete; partial-output limitation documented below. |
| T-013 | Askpass main; success/correction/cancel/confirmation, extra-argument and malformed-control failure tests | Partial: injected production QML-load/platform startup failure paths are not covered. |
| T-014 | signalfd/QSocketNotifier; offscreen subprocess and live notification termination | Complete |
| T-015 | Both pre-Qt hardening paths; zero-core-limit test, synthetic intentional crash, stderr/protocol audit | Complete; intentional crash exercised askpass, not the live agent. |
| T-016 | BUILD_TESTS-only FD-3 seam; process suite and Release exclusion probe | Partial: real window-manager dismissal and full startup/setup failure matrix remain uncovered. Historical completion was too broad. |
| T-017 | CMake installs three copies of one implementation; staged install and basename protocol suite | Complete |
| T-018 | `PolkitListenerBridge.cpp`; injected registration and raw listener request mapping | Partial: raw multi-identity order and GCancellable race tests at the bridge boundary are still missing. |
| T-019 | FIFO coordinator tests for queued/active cancellation, reentrant completion and exactly-once results | Complete |
| T-020 | Current-UID, first-fallback, duplicates, explicit choice and empty-list tests | Complete |
| T-021 | `PolkitSessionAdapter.cpp`; mixed visible/hidden prompt/message factory tests and response-path source checks | Complete |
| T-022 | Retry/reselection, failed-session, stale-generation, cancellation, shutdown and destroyed-coordinator tests | Complete at coordinator seam; raw authority cancellation race coverage belongs to T-018. |
| T-023 | Startup classification tests, pre-Qt hardening, real conflict exit 78 with preserved competing PID | Complete |
| T-024 | Real bridge/coordinator/model composed with fake authority/PAM in two instances | Partial: distinct session IDs are tested, but those instances share a process/environment; distinct Wayland environments are not exercised. Historical completion was too broad. |
| T-025 | Staged unit directives and actual systemd conflict exit 78, zero restarts, competing PID preserved | Complete; live evidence below. |
| T-026 | Wrapper/peer tests, corrected RuntimePath lookup and actual staged service authentication in UWSM | Complete; two-session routing uses isolated peers, not two live desktops. |
| T-027 | Session exports, descendant execution, mocked systemd and D-Bus activation command arguments, source mutation inspection | Complete for isolated script checks; real activated child environments remain a live check. |
| T-028 | Staged manifest, qmltypes/module metadata, runtime dependency inspection, architecture/source checks | Complete |
| T-029 | Focused checks followed by final full/lint/metadata/architecture/format/tidy results above | Complete |
| T-030 | Live conflict, regular notification windows, dark/light screenshots, SIGTERM, sudo cancellation/authentication, OpenSSH passphrase/Allow/Reject, and approved real Polkit authentication | Partial: remaining live matrix below. |
| T-031 | Security/scope audit below | Complete as an audit with explicit limits; does not certify secure erasure or complete live acceptance. |

## Security audit

Only synthetic responses were used. The model keeps response delivery in a
process-local controller callback. Its readable properties and public signals
were inspected via Qt metadata after sensitive transitions; signals carry no
response arguments. No shell singleton, shell IPC, credential cache, custom
crash handler, or response-bearing remote API was added. Source and link-boundary
inspection found no password-agent, disk-unlock, keyring, browser, or application
login integration in these authentication targets.

A real debug helper received an invalid synthetic marker through its inherited
pipe, remained open for correction, and was cancelled. Its command line was
marker-free; stdout/stderr remained empty. All four XDG config/data/cache/state
locations were directed to one fresh temporary directory: it contained zero
files afterwards. Runtime environment inspection was denied after
`PR_SET_DUMPABLE=0`; the audit does **not** claim it scanned that environment.
Source review finds no response export or process-spawn path. Actual PAM/Polkit
wire traffic and privileged memory were not captured.

`AuthenticationWriter` tests establish exact valid output, partial writes,
EINTR retry, and zero/hard-error failure. A writer can fail **after** transferring
some bytes; those bytes cannot be retracted. The function returns failure and
never retries the complete payload. Therefore “empty stdout on every failure”
can only hold before writing begins, not after an operating-system partial
write. This limitation is inherent in the pipe protocol.

Both executable startup paths apply zero soft/hard RLIMIT_CORE and disable
Linux dumpability before Qt. This host's `core_pattern` pipes to
`systemd-coredump`, so the additional dumpability guard matters: Linux ignores
RLIMIT_CORE for piped collectors ([Linux core(5)](https://www.man7.org/linux/man-pages/man5/core.5.html)).
The test-only intentional askpass abort exits by SIGABRT with WCOREDUMP false,
empty output streams, and no kernel-generated core. No real credential was
present. The live Polkit agent was not intentionally crashed.

Qt strings, clipboard paste, allocators, PAM libraries, input methods and the
compositor may make transient copies. Logical input clearing and disabled
selection/copy do not establish secure erasure. The compositor, input method,
kernel/root, and compromised same-UID processes remain outside the frontend's
credential isolation guarantee. Dumpability restrictions reduce ordinary
process inspection but do not create an authentication boundary against a
compromised desktop account.

## Live session evidence and remaining acceptance

Read-only discovery used `loginctl list-sessions`, `loginctl show-session 4`,
`loginctl show-user 1000 -p RuntimePath`, `hyprctl instances -j`, compositor
`/proc/2590/cgroup`, and only the session/display keys from selected process
environments. Session 4 belongs to UID 1000, is active, and has type wayland.
Hyprland PID 2590 advertises `XDG_SESSION_ID=4`; `hyprctl instances` maps that PID
to `wayland-1`. This mapping was not inferred from a socket's existence alone.

The existing `hyprpolkitagent.service` was active at PID 2832 before and after
the direct conflict test. With explicit session 4, runtime `/run/user/1000`,
Wayland `wayland-1`, Qt wayland platform and staged QML imports, the Release
HoloNight agent exited **78**, reported fixed conflict guidance, and left the
competing agent active at the same PID. No stop/disable/mask/kill/replacement
command was used.

Before the UWSM routing fix, the wrapper was invoked with timeout 0 and the Release agent override for
session 4. It returned `classification=session-not-ready`. The compositor is
under
`user@1000.service/session.slice/wayland-wm@hyprland.desktop.service`, outside
`session-4.scope`. Its original `/proc` environment also predates publication
of WAYLAND_DISPLAY. The original wrapper searched only session-scope descendants
and required that environment. The peer-verification fallback now resolves this
limitation without trusting mutable manager state or socket existence alone;
the original failed invocation remains historical evidence.

A Release SSH-basename helper with `SSH_ASKPASS_PROMPT=none` and
`SSH_ASKPASS_REQUIRE=force` was shown with each isolated appearance file.
Hyprland reported mapped, floating windows of 560×104 logical pixels. Only the
owned window was captured with `grim`; both windows had no editable field or
response controls. They exited 0 on SIGTERM with empty stdout and stderr.
These were direct helper invocations, not a claim that OpenSSH itself launched
them. The shell control socket was absent during these independent frontend
checks; the shell was neither restarted nor controlled.

- [Dark appearance](acceptance-dark.png)
- [Light appearance](acceptance-light.png)

Still pending:

- Subjective confirmation of focus, Escape, and keyboard-only interaction
  for the successful sudo/OpenSSH checks documented below.
- Live interactive identity/retry/busy behavior and a second graphical session.
- Observing real systemd/D-Bus-activated descendants' askpass environment exports.

At initial delivery, user interaction was requested asynchronously and
credential-dependent checks were deferred. The user subsequently started the
guided continuation recorded below. No privileged fixture was installed. The
prepared files in `fixtures/` are reviewable and would require installation
approval plus a suitable test session before use:

```sh
sudo install -D -o root -g root -m 0755 \
  docs/sdd/authentication-frontends/fixtures/holonight-authentication-acceptance \
  /usr/local/libexec/holonight-authentication-acceptance
sudo install -D -o root -g root -m 0644 \
  docs/sdd/authentication-frontends/fixtures/org.holonight.authentication.acceptance.policy \
  /usr/share/polkit-1/actions/org.holonight.authentication.acceptance.policy
pkexec /usr/local/libexec/holonight-authentication-acceptance
```

The probe only prints a fixed success message, accepts no arguments, and changes
no system state. The policy requires active-session administrator
authentication without retained authorization. It does not alter sudo policy or
replace any authentication agent. The commands above were **not run**.

## Guided acceptance continuation — 2026-09-05

The user started guided acceptance. Session discovery again found Hyprland PID
2590 / session 4 / `wayland-1`, with `hyprpolkitagent.service` still active at
PID 2832. Both Release executables were rebuilt from the current sources;
the askpass binary still contains no test-control protocol marker.

Using the explicit validated graphical environment and
`SUDO_ASKPASS=/tmp/holonight-live-acceptance/holonight-sudo-askpass`:

- `sudo -A -k -p '<cancellation instruction>' /usr/bin/true`: exited 1 with
  “no password was provided” / “a password is required”, as expected after
  cancellation. No password was requested for this first interaction.
- `sudo -A -k -p '<authentication instruction>' /usr/bin/true`: exited 0.
  The user was instructed to enter their password directly into the desktop
  prompt. The helper response was consumed by sudo, not captured by the agent.

No screenshots were taken during these checks. Subjective confirmation of
initial focus and Escape behavior is still requested separately; process exit
status alone does not establish all interaction details.

An initial disposable SSH fixture unexpectedly loaded without prompting.
That attempt does not count as passphrase acceptance. The fixture was then
encrypted through ssh-keygen's terminal input with a synthetic test passphrase,
and `ssh-keygen -y -P ''` was verified to fail before retrying. It is an isolated
local test identity under `/tmp/holonight-live-acceptance`, not an existing user
key or a key authorized on a server. OpenSSH checks use their own temporary
ssh-agent socket, never the user's existing SSH agent.

The corrected, encrypted SSH fixture passed the real OpenSSH flow with
`SSH_ASKPASS_REQUIRE=force` and an isolated SSH_AUTH_SOCK:

- `ssh-add -c /tmp/holonight-live-acceptance/disposable-ed25519`: exit 0 after
  the graphical passphrase interaction.
- `ssh-add -T <fixture-public-key>` followed by Allow: exit 0 (local
  sign/verify succeeded).
- The same sign/verify operation followed by Reject: exit 1, as expected.

The user was guided to use Tab/Enter for confirmation. The temporary SSH agent
was terminated after the checks; the user's SSH agent was not accessed.
`pkaction --action-id org.freedesktop.policykit.exec --verbose` confirms that
an existing auth_admin policy can support a subsequent `/usr/bin/true` Polkit
check without installing the prepared privileged fixture.


### Approved real Polkit authentication

The user explicitly approved a temporary agent switch for this check. The
reviewable `/tmp/holonight-live-acceptance/polkit-check.py` revalidated session 4,
its UID, active Wayland state, and the compositor PID/session/display mapping
before changing service state. It then:

1. Stopped `hyprpolkitagent.service` (original PID 2832).
2. Started the current Release `holonight-polkit-agent` directly with the
   validated session environment.
3. Ran `pkexec --disable-internal-agent /usr/bin/true`. The user was instructed
   to enter their administrator password only into the desktop prompt.
   **Result: exit 0**, using the existing `org.freedesktop.policykit.exec`
   policy; no new policy or privileged fixture was installed.
4. Terminated the temporary HoloNight agent and restarted
   `hyprpolkitagent.service` in the script's cleanup block.

Restoration was verified both by the script and a subsequent read-only service
query: `ActiveState=active`, `SubState=running`, `MainPID=490441`.
The credential response was not captured in tool output, screenshots, files,
arguments, or environment. The script discarded frontend and pkexec output
streams and retained only fixed result/status fields in
`/tmp/holonight-live-acceptance/polkit-results.json`.

This verifies real Polkit authentication through the directly launched
frontend. Installed service verification was performed in the later continuation
below. The user confirmed that the Polkit prompt appeared with
the response field focused, accepted the password with Enter, and closed
cleanly. This confirmation covers the Polkit interaction; the separately
listed sudo/SSH focus and Escape observations remain pending. Only
ACCEPTANCE.md and TASKS.md were updated in this continuation;
`git diff --check` passed.


## Installed UWSM service continuation — 2026-09-05

The wrapper now tries the original session-scope route first, then verifies a
Wayland connection's `SO_PEERCRED` UID/PID against the compositor's original
`XDG_SESSION_ID` and `XDG_RUNTIME_DIR`. The live peer was PID 2590, UID 1000,
session 4. It exports only verified routing and allowlisted Qt/cursor settings,
forces the Wayland Qt backend, and clears inherited `DISPLAY`. Conflicting
manager session/Wayland state is still rejected. Multiple matching peers are
rejected rather than guessed. Python 3 is a runtime dependency; no Python
packages are required.

`tests/test_wayland_session_environment.py` covers seven real Unix-socket peer
scenarios, including two sessions for one UID, stale sockets, wrong owner or
session/runtime, ambiguous peers and environment filtering. The Bash wrapper
regression additionally covers partial helper output followed by failure and
conflicting manager state. The old wrapper failed the UWSM regression before
the fix. The first full run could not launch the Python test because CMake's
interpreter variable was scoped to a sibling directory; interpreter discovery
was moved to the root configuration. The corrected routing-only full run passed
1129/1129 (`/tmp/auth-uwsm-full-test-final.log`).

The Release build was configured with `BUILD_TESTS=OFF` and
`CMAKE_INSTALL_PREFIX=/tmp/holonight-uwsm-acceptance`, then rebuilt and installed
using `cmake --install /tmp/holonight-auth-release/apps/authentication`.
`/tmp/holonight-live-acceptance/uwsm-service-check.py` linked the installed
service template with `systemctl --user link --runtime`, created an instance
QML-import-only drop-in, reloaded the manager, and started
`holonight-polkit-agent@4.service`. Neither routing variables nor manager-wide
environment were overridden for that service.

With the original agent running, the installed instance exited 78 and retained
`NRestarts=0` after five seconds, longer than its two-second restart interval.
The original agent PID was unchanged. Under the previously approved temporary
switch, the instance became active/running with `LimitCORE=0`, and
`pkexec --disable-internal-agent /usr/bin/true` exited 0. The first cleanup
exposed a stop timeout: systemd killed the test agent after 90 seconds, then the
script restored the original agent at PID 504072. This failed shutdown is
retained as evidence in `uwsm-service-results-before-shutdown-fix.json` and the
unit's systemd journal; successful authentication alone did not establish a
clean service lifecycle.

The Polkit SIGTERM callback used `quit()`, which can be rejected by the
persistent dialog's close handler. It now calls `exit(0)` on the GUI thread,
retaining `aboutToQuit` coordinator cleanup. The isolated
`PolkitAgentProcess.SigtermExitsPersistentDialogAndUnregisters` regression runs
the production executable against a private D-Bus authority, presents synthetic
identity selection without starting PAM, then sends SIGTERM. Before the fix it
failed its five-second exit deadline; afterward it passed, observed authority
unregistration and normal exit 0, and found empty stdout. It uses a 250 ms
rendering allowance because the production executable exposes no UI-readiness
test interface. Logs: `/tmp/auth-shutdown-before.log` and
`/tmp/auth-shutdown-after.log`. No production test interface was added.

The rebuilt Release installation was then exercised through the same runtime
service driver. Conflict again produced status 78 and zero restarts while
preserving original PID 504072. After the approved switch, the HoloNight agent
ran at PID 512426, real `pkexec` authentication exited 0, and the agent remained
active afterward. `systemctl --user stop holonight-polkit-agent@4.service`
completed in **0.054 seconds** with `Result=success`, `ExecMainStatus=0`, and
`NRestarts=0`. The original agent was restored active/running at PID **513101**.
An independent query verified restoration, empty HoloNight `FragmentPath`, and
absence of the owned runtime template link and instance drop-in. Fixed status
fields are in `/tmp/holonight-live-acceptance/uwsm-service-results.json`.

No real response was captured, no screenshot was taken during these prompts,
and no privileged fixture, sudo-policy change, permanent enablement, or
manager-wide environment change was made. The staged installation remains under
`/tmp`; existing agents retain their original configuration. T-025 and T-026
are now complete. T-030 retains the remaining subjective/interactive and
second-desktop checks listed above.

Final verification after both fixes: `dbus-run-session -- task test` exited 0
with **1130/1130 passed** in 178.11 seconds. The isolated keyring/bus teardown
messages remained non-failing. `git diff --check` passed. This continuation
changed the routing wrapper and new peer helper, Polkit main's termination
callback, root/authentication/test CMake files, routing/install/process tests,
README, DESIGN.md, TASKS.md and this acceptance record. Earlier uncommitted
implementation changes were preserved.

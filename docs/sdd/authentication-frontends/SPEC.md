# SPEC - authentication-frontends

## 1. Purpose

Provide HoloNight-native graphical authentication prompts without allowing
secrets to enter the long-running shell process. The feature presents one
shared user experience through two independently launched executables:

- `holonight-polkit-agent`, a persistent Polkit authentication agent for the
  graphical session; and
- one shared one-shot askpass implementation installed as
  `holonight-sudo-askpass`, `holonight-ssh-askpass`, and the compatibility
  entry point `holonight-askpass`.

The feature owns presentation and protocol adaptation only. The decision to
authorize an operation remains with Polkit policy, sudo policy, PAM, or
OpenSSH, as applicable.

## 2. Terminology

- **Authorization**: deciding whether a subject may perform an action.
  Authentication can satisfy an authorization challenge, but successful
  authentication does not itself define policy.
- **Authentication**: proving control of a requested identity or secret to the
  authority that issued a challenge.
- **Polkit identity**: an identity offered by Polkit as eligible to authenticate
  a particular authorization request. It can be the session user or another
  administrator and is not necessarily the process owner.
- **sudo password**: the secret requested by sudo according to its policy. The
  askpass frontend shall not assume it is the root password.
- **SSH passphrase**: a secret used by OpenSSH, commonly to unlock a private
  key. It is distinct from a remote account password even though both use the
  askpass string-response protocol.
- **Confirmation prompt**: a yes/no decision requested by OpenSSH; it does not
  return a secret.
- **Notification prompt**: an informational OpenSSH prompt launched
  asynchronously and later terminated by OpenSSH. It has no response or
  acknowledgement protocol.
- **accepted response**: the exact secret or textual response returned by an
  askpass UI after the user accepts the prompt.

## 3. Scope and Non-Goals

### Scope

- Provide a reusable `Holonight.Authentication` QML module with HoloNight
  styling and accessible keyboard interaction.
- Provide a neutral authentication prompt model usable by both frontends.
- Implement a persistent graphical Polkit agent using `polkit-qt6-agent-1`.
- Implement one one-shot helper with distinct sudo and OpenSSH entry names,
  compatible with `SUDO_ASKPASS` and `SSH_ASKPASS`.
- Install the executables, QML module, required runtime artifacts, and a
  systemd user service for the Polkit agent.
- Integrate both frontends into HoloNight-managed graphical sessions.
- Verify protocol behavior with unit, contract, QML, integration, and live
  compositor tests.

### Non-Goals

- Replace or modify Polkit, sudo, PAM, OpenSSH, or their policy configuration.
- Change `/etc/sudo.conf` or make ordinary terminal `sudo` use a graphical
  prompt. Terminal sudo remains TTY-driven unless its caller requests askpass
  behavior, such as with `sudo -A`.
- Implement systemd password agents, disk-unlock prompts, keyrings, browser
  credential prompts, or application-specific login dialogs.
- Cache passwords or passphrases. Authorization caching remains owned by
  Polkit, sudo, PAM, and OpenSSH.
- Route authentication requests through `holonight-shell` or reuse its process
  as either authentication frontend.
- Automatically stop, disable, mask, uninstall, or reconfigure an existing
  third-party authentication agent.

## 4. Shared Authentication UI and Model

**REQ-F-001** (Ubiquitous)  
The project shall provide a reusable QML module named
`Holonight.Authentication` that is shared by both authentication executables.

*Acceptance criterion*: Both executables instantiate their prompt UI from the
module without importing `HolonightShell` or shell-owned service singletons.

**REQ-F-002** (Ubiquitous)  
The shared UI shall be driven by a protocol-neutral prompt model rather than by
Polkit-, sudo-, or OpenSSH-specific QML behavior.

*Acceptance criterion*: An in-memory test model can exercise every visible
prompt state without starting Polkit, sudo, OpenSSH, or `holonight-shell`.

**REQ-F-003** (Ubiquitous)  
The neutral model shall expose `requestMessage`, a safe `requestReference`, safe
`requesterDetails`, `currentPrompt`, `inputMode`, `identities`,
`selectedIdentity`, `messages`, and `lifecycleState`. Each identity shall expose
a stable ID and a display label; each message shall expose an informational or
error severity and plain-text content.

*Acceptance criterion*: The QML layer can render and operate a complete prompt
using only those model properties. The input mode distinguishes no input,
visible input, secret input, confirmation, and notification through `None`,
`Visible`, `Secret`, `Confirmation`, and `Notification`; lifecycle state uses
`Idle`, `SelectingIdentity`, `AwaitingInput`, `Busy`, `RetryableError`,
`Completed`, and `Cancelled`.

**REQ-F-004** (State-driven)  
The model and UI shall support secret entry, confirmation, identity selection,
busy, error, retry, and cancellation states.

*Acceptance criterion*: Automated QML tests can transition through each state
without recreating the window, and stale input is cleared before a retry or a
new request. The model provides `respond(value)`, `confirm(accepted)`,
`acknowledge()`, `cancel()`, `selectIdentity(stableId)`, and `retry()` and
rejects operations that are invalid for its current lifecycle and input modes.

**REQ-F-005** (Conditional)  
While the active mode contains a secret, the UI shall hide entered characters
and shall not expose the secret through accessibility names, clipboard actions,
input-method suggestions, or non-secret model properties.

*Acceptance criterion*: QML tests observe masked rendering and disabled copy
actions, and model inspection does not reveal the entered secret outside the
one response operation.

**REQ-F-006** (Ubiquitous)  
The shared UI shall support complete keyboard operation, including focus entry,
identity navigation, accept, retry, and cancel.

*Acceptance criterion*: QML tests complete secret, confirmation, identity, and
cancellation flows without pointer input and verify that accept controls are
disabled while busy.

**REQ-F-007** (Ubiquitous)  
The shared UI shall use the active HoloNight appearance and remain legible in
both dark and light modes.

*Acceptance criterion*: Live checks confirm correct palette, focus indication,
contrast, and prompt placement in both appearance modes.

## 5. Polkit Authentication Agent

**REQ-F-008** (Ubiquitous)  
The project shall provide `holonight-polkit-agent` as a persistent executable
that is independent of `holonight-shell`.

*Acceptance criterion*: The agent can start, register, display a challenge, and
exit while the shell is stopped, and stopping or restarting either process does
not terminate the other.

**REQ-F-009** (Ubiquitous)  
The Polkit executable shall implement its authentication-agent integration with
`polkit-qt6-agent-1` and register for the specific graphical login session
identified by its service instance.

*Acceptance criterion*: A mock authority observes one registration for the
correct session subject and can initiate and cancel an authentication request.
Two graphical sessions for the same UID start distinct agent instances and
route prompts to their respective Wayland environments.

**REQ-F-010** (Event-driven)  
When Polkit begins an authentication request, the adapter shall map the action
message, action identifier, requester details, eligible identities, and
cancellation into the neutral prompt model without presenting untrusted details
as markup.

*Acceptance criterion*: Unit tests verify field mapping, plain-text treatment,
and cancellation propagation for representative requests.

**REQ-F-011** (Conditional)  
When more than one Polkit identity is eligible, the prompt shall allow the user
to choose among all eligible identities before authentication starts.

*Acceptance criterion*: The identity list contains every authority-provided
identity exactly once and selecting one starts authentication only for that
identity. Identity equality and selection use the stable Polkit identity or UID,
never a localized display label.

**REQ-F-012** (Conditional)  
When the current session user is eligible, the agent shall select that identity
by default; otherwise it shall select the first authority-provided identity as
the deterministic fallback without inventing or reordering identities. If the
authority provides no eligible identity, the agent shall not open a prompt and
shall complete the request unsuccessfully exactly once with a safe diagnostic.

*Acceptance criterion*: Unit tests cover stable-identity current-user
preference, an administrator-only list, first-entry fallback, and an empty list
that produces one unsuccessful terminal result without opening a window.

**REQ-F-013** (Event-driven)  
When PAM supplies a multi-step conversation through PolkitQt, the adapter shall
present prompts in order, distinguish visible and hidden responses, display
informational and error messages, and submit each response to its originating
authentication session.

*Acceptance criterion*: Integration tests cover two or more sequential prompts,
mixed visible/hidden input, informational messages, and successful completion.

**REQ-F-014** (Event-driven)  
When a PolkitQt authentication session completes unsuccessfully while its
original authorization request remains active, the UI shall show the failure,
clear all response state, and offer retry without completing the authorization
request. Retry shall return to identity selection and start a fresh PolkitQt
authentication session for the newly selected identity.

*Acceptance criterion*: Tests verify a failed attempt, optional identity
reselection, and a successful fresh-session retry, and confirm that the earlier
response and authentication session are neither retained nor reused.

**REQ-F-015** (Event-driven)  
When the user cancels, the authority cancels the request, or the authentication
session terminates, the agent shall cancel the active conversation, close its
prompt, clear transient state, and report no success.

*Acceptance criterion*: Tests cover cancellation from each source and verify
that a late callback cannot reopen or complete the dismissed request.

**REQ-F-016** (Unwanted behavior)  
If Polkit-agent registration fails because another agent owns the session
registration, `holonight-polkit-agent` shall emit a clear diagnostic, terminate
with a distinct non-retriable conflict outcome, and shall not disable or alter
the other agent.

*Acceptance criterion*: A conflict test reports the registration failure,
leaves the existing registration active, does not invoke service-management
commands, and confirms that systemd does not restart-loop on the conflict
outcome.

**REQ-F-017** (State-driven)  
The agent shall process at most one active Polkit request at a time and shall
queue additional requests in authority arrival order.

*Acceptance criterion*: Concurrent-request tests observe FIFO presentation,
remove a queued request when its authority cancellation arrives, and deliver
exactly one terminal outcome for every active or queued request. Cancellation
and late callbacks cannot complete a different request.

## 6. sudo and OpenSSH Askpass Helper

**REQ-F-018** (Ubiquitous)  
The project shall provide one separate one-shot graphical implementation
installed as `holonight-sudo-askpass [prompt]`,
`holonight-ssh-askpass [prompt]`, and the compatibility entry point
`holonight-askpass [prompt]`.

*Acceptance criterion*: Each invocation creates at most one prompt, returns one
result, and exits without requiring the shell or Polkit agent to be running.
Independent askpass processes may coexist with one another and with a Polkit
prompt; they share no lifecycle, response state, or secret-bearing IPC.

**REQ-F-019** (Event-driven)  
When invoked as a sudo askpass program, the helper shall display its prompt
argument and, after acceptance, write the accepted response as valid UTF-8 bytes
followed by exactly one LF to standard output and exit with status zero.

*Acceptance criterion*: A contract test captures stdout byte-for-byte and finds
only the accepted response plus its terminating LF, with UI text and diagnostics
absent.

**REQ-F-020** (Event-driven)  
When a sudo askpass prompt is cancelled, cannot be displayed, or otherwise
fails, the helper shall exit nonzero and shall write no response bytes to
standard output.

*Acceptance criterion*: Contract tests cover user cancellation, display/setup
failure, and internal failure and observe empty stdout for each.

**REQ-F-021** (Conditional)  
When invoked through `holonight-ssh-askpass` with an unset or unrecognized
`SSH_ASKPASS_PROMPT`, or with a secret-request mode, the helper shall present
hidden input and use the UTF-8-plus-LF accepted-response stdout protocol.

*Acceptance criterion*: A real one-shot process test supplies a passphrase and
observes exactly that response on stdout with a zero exit status.

**REQ-F-022** (Conditional)  
When `SSH_ASKPASS_PROMPT=confirm`, `holonight-ssh-askpass` shall present an
explicit confirmation prompt, return success with empty stdout for acceptance,
and return nonzero with empty stdout for rejection or cancellation.

*Acceptance criterion*: OpenSSH contract tests cover accept, reject, and window
dismissal and observe the required exit status and empty stdout.

**REQ-F-023** (Conditional)  
When `SSH_ASKPASS_PROMPT=none`, `holonight-ssh-askpass` shall present a
notification-only UI with no input or response controls, write nothing to
stdout, and remain dismissible until OpenSSH terminates it, normally with
`SIGTERM`. Its exit status shall not represent acknowledgement or another
caller-consumed result.

*Acceptance criterion*: An OpenSSH contract test verifies notification-only UI,
no editable field, no response path, empty stdout, and prompt dismissal without
an error diagnostic or crash when the process receives `SIGTERM`.

**REQ-F-024** (Conditional)  
When invoked through the `holonight-askpass` compatibility entry point,
recognized `SSH_ASKPASS_PROMPT` values `confirm` and `none` shall select their
OpenSSH modes; all other values shall select the shared secret mode.

*Acceptance criterion*: Contract tests verify the precedence without relying on
parent-process inspection and session integration never exports the compatibility
entry point for `SUDO_ASKPASS` or `SSH_ASKPASS`.

**REQ-F-025** (Ubiquitous)  
Secret mode shall reject an empty response, a response containing CR, LF, or
NUL, invalid UTF-8, or a UTF-8 payload longer than 1022 bytes and shall keep the
prompt open with a safe validation error.

*Acceptance criterion*: Boundary tests accept a 1022-byte payload and observe
its LF framing, reject every prohibited input without writing stdout, and allow
the user to correct the response or cancel.

**REQ-F-026** (Ubiquitous)  
The askpass executable shall reserve standard output exclusively for its caller
protocol and shall send diagnostics, when safe and necessary, to standard error
or the journal without including secrets.

*Acceptance criterion*: Contract tests that enable verbose diagnostics still
observe protocol-only or empty stdout as required by REQ-F-019 through
REQ-F-025.

## 7. Process Isolation and Secret Handling

**REQ-S-001** (Ubiquitous)  
Passwords, passphrases, and authentication responses shall remain within the
frontend process that collected them and the upstream authentication API or
stdout channel explicitly required by that frontend's protocol.

*Acceptance criterion*: Architecture review finds no secret-bearing call,
signal, socket message, or shared object crossing into `holonight-shell`.

**REQ-S-002** (Unwanted behavior)  
Authentication secrets shall never pass through `holonight-shell`, its control
socket, D-Bus, logs, persistent storage, or command-line arguments.

*Acceptance criterion*: Instrumented tests submit marker secrets and find no
marker in shell IPC, process arguments, captured logs, configuration, cache, or
persistent application data.

**REQ-S-003** (Ubiquitous)  
Each frontend shall keep secret values only as long as required to deliver the
current response and shall clear its UI and model state on submission,
cancellation, failure, retry, and shutdown.

*Acceptance criterion*: Lifecycle tests verify that completed and superseded
requests no longer expose their response through application objects. This is
a logical-clearing guarantee and does not claim erasure of every implicit copy
previously made by Qt or the operating system.

**REQ-S-004** (Ubiquitous)  
The Polkit agent shall have no command-line, stdout, D-Bus, control-socket, or
other application-facing channel that outputs a password.

*Acceptance criterion*: Public-interface inspection and integration tests show
that successful Polkit authentication reports completion only through
PolkitQt's authentication-session contract.

**REQ-S-005** (Ubiquitous)  
Prompt text and identity labels received from external processes or Polkit
shall be rendered as untrusted plain text and shall not control QML source,
rich-text markup, file paths, URLs, or command execution.

*Acceptance criterion*: Tests with markup, control characters, and shell-like
text display inert normalized text and do not load resources or execute actions.

**REQ-S-006** (Ubiquitous)  
Secret-entry controls shall disable copy operations and use Qt's available
sensitive-input and input-method hints to minimize disclosure and learning.

*Acceptance criterion*: QML tests verify the configured echo mode, copy
behavior, and input-method hints. A hostile or compromised input method or
compositor is outside the threat model.

**REQ-S-007** (Ubiquitous)  
Both authentication frontends shall disable core dumps and opt out of automatic
crash-report persistence before accepting a secret.

*Acceptance criterion*: Process-level tests verify a zero core-size limit or
equivalent platform control and confirm that an intentional test crash produces
no secret-bearing core or crash-report artifact.

## 8. Build, Installation, and Session Activation

**REQ-I-001** (Ubiquitous)  
`holonight-polkit-agent`, all three askpass entry names, the
`Holonight.Authentication` QML module, and PolkitQt support shall be required
build and package components for this feature. The supported baseline shall be
PolkitQt 0.200.0 or newer, OpenSSH 8.2 or newer, and sudo 1.8.15 or newer, in
addition to the project's existing Qt 6 requirements.

*Acceptance criterion*: Configuration fails with an actionable diagnostic when
the required Qt 6 Polkit agent development component is unavailable or too old,
and an install manifest contains the Polkit executable, the three askpass entry
names backed by one implementation, and their shared QML resources.

**REQ-I-002** (Ubiquitous)  
The project shall install a session-keyed systemd user template unit that runs
one `holonight-polkit-agent` instance per graphical `XDG_SESSION_ID`,
independently of `holonight-shell`.

*Acceptance criterion*: The installed unit has no lifecycle dependency on the
shell service, is ordered against the graphical session target used by
HoloNight, and binds to captured session identity and Wayland/compositor
environment rather than using mutable global user-manager values to select a
session.

**REQ-I-003** (Event-driven)  
When a HoloNight-managed graphical session is explicitly activated, session
integration shall start the HoloNight Polkit agent instance keyed by that
session's `XDG_SESSION_ID`, independently of the shell.

*Acceptance criterion*: Entering the managed session makes the unit active when
no conflicting agent is registered, while installing the package alone does
not mutate another agent's service state. Concurrent sessions for the same UID
do not overwrite one another's display routing.

**REQ-I-004** (Ubiquitous)  
HoloNight session integration shall export absolute installed paths for
`SUDO_ASKPASS` and `SSH_ASKPASS` into the process, systemd user manager, and
D-Bus activation environments used by the managed session.

*Acceptance criterion*: Applications launched directly, by a systemd user unit,
and by D-Bus activation observe `holonight-sudo-askpass` for `SUDO_ASKPASS` and
`holonight-ssh-askpass` for `SSH_ASKPASS` after a fresh session start. Session
integration does not globally export `SSH_ASKPASS_PROMPT`.

**REQ-I-005** (Ubiquitous)  
Installation and session activation shall leave `/etc/sudo.conf` under
administrator control and shall not add, remove, or rewrite its `Path askpass`
setting.

*Acceptance criterion*: Package installation and managed-session startup make
no write attempt to `/etc/sudo.conf`; `sudo -A` uses `SUDO_ASKPASS` from the
calling environment when exported.

**REQ-I-006** (Unwanted behavior)  
If another Polkit agent is already registered, activation shall surface the
diagnostic required by REQ-F-016 and shall not automatically stop, disable, or
mask any third-party unit or process.

*Acceptance criterion*: A live conflict check leaves the third-party agent
unchanged and provides a journal message naming the HoloNight registration
failure and a manual remediation direction. The unit's restart policy prevents
automatic restart for the distinct registration-conflict outcome while retaining
its documented recovery behavior for crashes.

## 9. Public Interface Contracts

### `holonight-polkit-agent`

- Persistent graphical-session executable instantiated once per
  `XDG_SESSION_ID`.
- Registers one Polkit authentication agent for its bound session.
- Accepts Polkit requests and cancellation through PolkitQt.
- Serializes requests through one FIFO queue per agent instance.
- Has no password output protocol or shell IPC dependency.

### Askpass entry points

- `holonight-sudo-askpass [prompt]` is the `SUDO_ASKPASS` entry point.
- `holonight-ssh-askpass [prompt]` is the `SSH_ASKPASS` entry point and consumes
  `SSH_ASKPASS_PROMPT`.
- `holonight-askpass [prompt]` is a compatibility entry point where recognized
  `confirm` and `none` hints take precedence and all other invocations use
  secret mode.
- Secret success writes valid UTF-8 payload bytes followed by one LF; secret
  cancellation or failure writes nothing and exits nonzero.
- Confirmation uses zero/nonzero exit status and empty stdout. Notification has
  no caller-consumed result and handles termination by OpenSSH.

### Neutral authentication prompt model

- Readable context: `requestMessage`, `requestReference`, `requesterDetails`,
  `currentPrompt`, and `messages` containing severity plus plain text.
- Readable interaction state: `inputMode`, `lifecycleState`, `identities`
  containing stable IDs plus display labels, and `selectedIdentity`.
- Operations: `respond(value)`, `confirm(accepted)`, `acknowledge()`, `cancel()`,
  `selectIdentity(stableId)`, and `retry()`, each valid only in its corresponding
  state.
- The adapter owns protocol-specific metadata and converts it to this neutral
  surface; the QML module does not call Polkit, sudo, or OpenSSH directly.

### `Holonight.Authentication`

- Shared QML module consumed by both executables.
- Contains prompt presentation and interaction components only.
- Does not import shell-owned services or create a dependency on the shell
  executable's QML module.

## 10. Quality and Verification Requirements

**REQ-V-001** (Ubiquitous)  
Unit tests shall cover Polkit request mapping, current-user identity preference,
first-entry fallback identity selection, empty identities, PAM prompt
sequencing, fresh-session retry with identity reselection, FIFO request
serialization, queued cancellation, and late callbacks.

*Acceptance criterion*: Each behavior has a deterministic test that does not
require a live system authority, uses stable identities rather than labels, and
asserts exactly one terminal outcome per request.

**REQ-V-002** (Ubiquitous)  
Askpass contract tests shall execute the real one-shot process and verify that
stdout contains only an accepted valid UTF-8 secret response plus exactly one LF
and is empty on cancellation, rejection, notification, or failure.

*Acceptance criterion*: Tests assert raw stdout bytes, exit status, and stderr
separately for the sudo, OpenSSH, and compatibility entry points, including
empty, forbidden-character, 1022-byte, and oversized responses.

**REQ-V-003** (Ubiquitous)  
OpenSSH tests shall cover secret, `confirm`, and `none` prompt modes.

*Acceptance criterion*: Secret and confirmation modes exercise acceptance plus
their applicable rejection, cancellation, and failure behavior. Notification
mode verifies asynchronous display, empty stdout, and graceful `SIGTERM`
termination without treating exit status as acknowledgement.

**REQ-V-004** (Ubiquitous)  
QML tests shall cover keyboard navigation, hidden input, identity selection,
error and retry presentation, disabled/busy controls, and cancellation.

*Acceptance criterion*: The offscreen QML test suite verifies every behavior
without importing shell-owned services.

**REQ-V-005** (Ubiquitous)  
Integration tests shall use a mock Polkit authority/session and real one-shot
askpass process execution.

*Acceptance criterion*: Tests verify request lifecycle, ordered conversation
responses, upstream cancellation, per-session routing, process output, and
applicable exit semantics across the C++/QML boundaries. A two-session test for
one UID verifies distinct `XDG_SESSION_ID` and Wayland environments.

**REQ-V-006** (Environment-driven)  
In a live Hyprland session with a test policy that requires authentication,
verification shall exercise `systemctl daemon-reload`, `sudo -A`, SSH
passphrase entry, SSH confirmation, a competing Polkit agent, keyboard focus,
and dark/light HoloNight styling. OpenSSH invocations shall set
`SSH_ASKPASS_REQUIRE=force` so TTY and display selection cannot bypass the
helper.

*Acceptance criterion*: The smoke record identifies the command and observed
result for every flow, confirms that conflicts are non-destructive and do not
restart-loop, and records screenshots for both appearance modes without
capturing secrets.

**REQ-V-007** (Ubiquitous)  
Implementation completion shall run the narrowest focused tests first, followed
by `task test`, `task qml-lint`, `task qmltypes-check`, and
`task architecture-check`, plus the live compositor checks in REQ-V-006.

*Acceptance criterion*: The change report records every command and outcome and
distinguishes environment limitations and pre-existing failures from feature
regressions.

## 11. Non-Functional Requirements

**REQ-NF-001** (Security)  
The frontends shall minimize secret lifetime and attack surface and shall not
trade process isolation for UI reuse.

*Acceptance criterion*: UI reuse occurs through a QML module or linked neutral
library, never by remote-controlling a shell-owned prompt.

**REQ-NF-002** (Reliability)  
Only one Polkit request shall own a given agent instance's window and response
path at a time. That instance shall serialize requests FIFO; independent
askpass processes and other session-keyed agents may coexist without sharing
state.

*Acceptance criterion*: Race-focused tests demonstrate that each upstream
request receives exactly one terminal outcome and no response is delivered to a
different request. Askpass invocations each terminate independently exactly
once, except that OpenSSH notification-mode termination has no response result.

**REQ-NF-003** (Observability)  
Lifecycle and protocol failures shall produce actionable diagnostics without
including secrets or successful askpass responses.

*Acceptance criterion*: Logs identify the frontend, operation phase, and safe
error description, while marker-secret scans remain empty.

**REQ-NF-004** (Accessibility)  
Prompt controls shall expose meaningful non-secret roles, names, state, and
focus order to accessibility clients.

*Acceptance criterion*: Accessibility inspection can distinguish request text,
identity selection, secret input, confirmation choices, progress, error, retry,
and cancellation without revealing entered characters.

**REQ-NF-005** (Responsiveness)  
Polkit, PAM, process, and session-management operations shall not block the QML
render thread.

*Acceptance criterion*: A delayed mock backend leaves animations, focus, and
cancellation responsive while the prompt reports busy state.

## 12. Constraints

**REQ-C-001**  
Installation does not replace `hyprpolkitagent`. Replacement occurs only when
the user or HoloNight-managed session explicitly activates the HoloNight agent,
and activation does not mutate the third-party agent's state.

**REQ-C-002**  
Plain terminal sudo remains TTY-driven unless the caller explicitly requests
askpass behavior.

**REQ-C-003**  
No password or passphrase cache is introduced by either frontend.

**REQ-C-004**  
Systemd password agents, disk unlocking, keyrings, browser credentials, and
application-specific login dialogs remain outside this feature.

**REQ-C-005**  
This document specifies behavior and interfaces only. Detailed architecture,
file layout, implementation sequencing, and task breakdown belong in separate
`DESIGN.md` and `TASKS.md` follow-up artifacts.

**REQ-C-006**  
The secret-handling threat model excludes a compromised compositor, input
method, operating-system kernel, root process, or same-UID process with memory
inspection privileges. The feature still minimizes ordinary clipboard,
logging, persistence, crash-dump, and application-object exposure.

## 13. Normative References

The following upstream contracts are normative. Where this specification is
more restrictive about isolation, logging, or output cleanliness, the stricter
HoloNight requirement also applies.

- [polkit: Writing Authentication Agents](https://polkit.pages.freedesktop.org/polkit/polkit-agents.html)
- [polkit: Authorization Manager](https://polkit.pages.freedesktop.org/polkit/polkit.8.html)
- [sudo(8): `--askpass` and `SUDO_ASKPASS`](https://www.sudo.ws/docs/man/sudo.man/)
- [sudo.conf(5): `Path askpass`](https://www.sudo.ws/docs/man/sudo.conf.man/)
- [OpenSSH `ssh(1)`: `SSH_ASKPASS` and `SSH_ASKPASS_REQUIRE`](https://man.openbsd.org/ssh.1)
- [OpenSSH portable 10.5p1 askpass prompt-mode contract](https://github.com/openssh/openssh-portable/blob/V_10_5_P1/readpass.c)

## Shared avatar and stable dialog presentation (2026-09-05)

The presentation contract below supersedes earlier content-driven sizing examples.

- `Holonight.Controls.HnAvatar` owns image presentation in both Shell authentication
  and Greeter. Its API is `source`, `size`, `fallbackSource`, `backgroundColor`,
  `ringColor`, `ringWidth`, and `imageInset` (measured from the outer edge).
  Images load asynchronously, crop to a circle, and decode for the rendered size
  and display scale. Empty, loading, or failed sources show the consumer's fallback;
  fallback failure leaves the background and ring. Account changes immediately hide
  the previous image. Account lookup, local URL restrictions, and fallback assets
  remain consumer responsibilities.
- The Shell identity selector derives from `Holonight.ComboBox`, preserving its
  themed popup, scrolling, highlighting, and constrained placement. Both collapsed
  and expanded rows use 56-pixel avatars; dropdown rows are 82 pixels tall. Names
  are plain text, with a secondary username when different. Selection uses stable IDs.
- Multiple eligible Polkit identities still require explicit confirmation. The
  selection stage explains the choice and offers **Continue** for the displayed
  identity. Return/keypad Enter on the closed selector also confirms it. Enter in
  an open popup confirms its highlighted row. Dropdown activation remains a
  confirmation path. Retry preserves the existing identity-selection policy.
  Single-identity requests remain automatic. Input appears only when PAM provides
  a prompt; the initial body reserves space without showing a fake password field.
- Preferred dimensions are 740 × 720 logical pixels for Polkit and 740 × 480 for
  askpass. Maximum dimensions are the associated `QScreen::availableGeometry()`
  minus 24 pixels on each edge, falling back to reported screen geometry if invalid.
  Where Wayland does not report reserved areas, available geometry may equal the
  full screen. Placement remains compositor controlled.
- A new request reapplies its preferred size within those bounds. Screen/work-area
  changes reclamp the current size, retaining smaller compositor-configured sizes.
  Identity selection, prompts, busy states, failures, and retries do not resize the
  window. Header and actions stay outside the shrinking, scrolling body; focus
  reveals the active input. Both frames follow actual window bounds.
- Both frontends remain ordinary frameless Wayland dialog windows. Compositor
  decoration guidance and the manual acceptance matrix are in
  [AVATAR-DIALOG-VERIFICATION.md](AVATAR-DIALOG-VERIFICATION.md). Personal compositor
  configuration is not modified.

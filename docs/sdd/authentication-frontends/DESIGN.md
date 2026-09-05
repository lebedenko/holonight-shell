# Authentication Frontends — Architecture Design

## 1. Overview

This feature provides HoloNight-styled graphical authentication without placing
secrets in the long-running shell. It consists of three independent targets:

1. `holonight_authentication_core`, a reusable C++ library containing the
   protocol-neutral prompt model, value validation, text normalization, and
   lifecycle rules.
2. `Holonight.Authentication`, a reusable QML presentation module consumed by
   authentication processes only.
3. Two executable implementations: persistent `holonight-polkit-agent` and a
   one-shot askpass helper installed under three entry-point names.

Neither executable links to `holonight-shell`, shell services, layer-shell, nor
shell IPC. UI reuse is in-process through the core library and QML module. The
frontends remain independently launchable and independently disposable.

The Polkit implementation deliberately combines the low-level
`libpolkit-agent-1` listener API with `PolkitQt1::Agent::Session`. The listener
preserves the authority's ordered identities, request cookie, and
`GCancellable`; each PolkitQt session handles one PAM conversation. The
PolkitQt `Listener` adapter is not used because its identity conversion
de-duplicates through unordered storage and does not retain enough
cancellation identity to satisfy deterministic ordering and stale-callback
requirements.

## 2. Architectural Boundaries

```text
Polkit authority                        sudo / OpenSSH
       |                                      |
raw libpolkit-agent listener           argv[0], argv[1], environment
       |                                      |
PolkitRequestCoordinator               AskpassController
       |                                      |
PolkitQt Agent::Session                 stdout writer
       |                                      |
       +---------- AuthenticationPromptModel-+
                              |
                    Holonight.Authentication
                              |
                 compositor-managed Qt dialog
```

The following dependency direction is enforced by target links and the
architecture check:

```text
Holonight.Authentication -> Holonight, Holonight.Core, Holonight.Controls
holonight-polkit-agent    -> authentication core, QML module, PolkitQt session,
                            libpolkit-agent-1
holonight-askpass         -> authentication core, QML module
```

There is no arrow to `holonight-shell`, `holonight_services`, shell-owned QML,
the shell control socket, or a shared authentication daemon. Independent
askpass processes and session-keyed Polkit agents share installed code but no
runtime state.

## 3. Proposed Source and Install Layout

The detailed filenames may be refined during task decomposition, but target
ownership is fixed:

```text
libs/holonight-authentication/
    CMakeLists.txt
    src/AuthenticationPromptModel.{h,cpp}
    src/AuthenticationIdentityModel.{h,cpp}
    src/AuthenticationMessageModel.{h,cpp}
    src/AuthenticationText.{h,cpp}
    src/SecretValidator.{h,cpp}

libs/holonight-authentication-qml/
    CMakeLists.txt
    qml/AuthenticationDialog.qml
    qml/AuthenticationPrompt.qml
    qml/IdentitySelector.qml
    qml/MessageList.qml

apps/polkit-agent/
    CMakeLists.txt
    main.cpp
    PolkitListenerBridge.{h,cpp}
    PolkitRequestCoordinator.{h,cpp}
    PolkitSessionAdapter.{h,cpp}

apps/askpass/
    CMakeLists.txt
    main.cpp
    AskpassController.{h,cpp}
    AskpassMode.{h,cpp}
    ProtocolWriter.{h,cpp}

data/systemd/user/holonight-polkit-agent@.service
scripts/holonight-polkit-agent-session
```

The QML module URI is `Holonight.Authentication`. Its installed resources use
their own module prefix and import only `QtQuick`, `QtQuick.Controls`,
`QtQuick.Layouts`, `Holonight`, `Holonight.Core`, and `Holonight.Controls` as
needed. It does not import `HolonightShell` or rely on a shell singleton.

## 4. Neutral Prompt Domain

### 4.1 Public model contract

`AuthenticationPromptModel` is a `QObject` facade over dedicated identity and
message `QAbstractListModel` instances. QML receives only normalized display
data and lifecycle state.

<!-- markdownlint-disable MD013 -->

```cpp
class AuthenticationPromptModel final : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString requestMessage READ requestMessage NOTIFY contextChanged FINAL)
  Q_PROPERTY(QString requestReference READ requestReference NOTIFY contextChanged FINAL)
  Q_PROPERTY(QVariantMap requesterDetails READ requesterDetails NOTIFY contextChanged FINAL)
  Q_PROPERTY(QString currentPrompt READ currentPrompt NOTIFY promptChanged FINAL)
  Q_PROPERTY(InputMode inputMode READ inputMode NOTIFY promptChanged FINAL)
  Q_PROPERTY(QAbstractItemModel* identities READ identities CONSTANT FINAL)
  Q_PROPERTY(QString selectedIdentity READ selectedIdentity NOTIFY selectedIdentityChanged FINAL)
  Q_PROPERTY(QAbstractItemModel* messages READ messages CONSTANT FINAL)
  Q_PROPERTY(LifecycleState lifecycleState READ lifecycleState NOTIFY lifecycleStateChanged FINAL)

 public:
  enum class InputMode { None, Visible, Secret, Confirmation, Notification };
  Q_ENUM(InputMode)
  enum class LifecycleState {
    Idle, SelectingIdentity, AwaitingInput, Busy, RetryableError, Completed, Cancelled
  };
  Q_ENUM(LifecycleState)

  Q_INVOKABLE void respond(const QString& value);
  Q_INVOKABLE void confirm(bool accepted);
  Q_INVOKABLE void acknowledge();
  Q_INVOKABLE void cancel();
  Q_INVOKABLE void selectIdentity(const QString& stable_id);
  Q_INVOKABLE void retry();
};
```

<!-- markdownlint-enable MD013 -->

Identity roles are `stableId` and `displayLabel`; message roles are `severity`
(`Information` or `Error`) and `text`. Identity equality, selection, and
protocol routing always use `stableId`, never the localized label.

Protocol adapters configure requests and advance backend-owned phases through
a controller-only API that is not invokable from QML. User operations are
checked against both lifecycle and input mode. An invalid operation is ignored,
reports a safe diagnostic for tests/logging, and never invokes a protocol
callback. Response delivery uses a one-shot, process-local callback; the model
does not expose a readable response property or retain a response after the
callback returns.

### 4.2 Lifecycle

The allowed state transitions are:

```text
Idle -> SelectingIdentity -> Busy -> AwaitingInput -> Busy
  |             |             |          |            |
  |             |             +----------+            +-> Completed
  |             |                    PAM sequence       +-> RetryableError
  |             +--------------------------------------------^
  +-> AwaitingInput (single/default identity or askpass)
  +-> Completed (notification termination / terminal backend result)

SelectingIdentity, AwaitingInput, Busy, RetryableError
  -> Cancelled
Completed, Cancelled -> Idle -> next request
```

Multiple Polkit identities always produce `SelectingIdentity`; authentication
does not begin until explicit selection, even though the preferred identity is
preselected. Zero identities complete unsuccessfully without showing a window.
A single identity may proceed directly to its fresh session. Retry clears the
old conversation, increments the session generation, and returns to identity
selection (or directly selects the sole identity through the same policy).

Terminal transitions are idempotent. The controller owns a completion guard so
only the first completion or cancellation reaches the upstream protocol.

### 4.3 Secret lifetime

The editable secret exists only in the QML input control while the model is in
`AwaitingInput` with `Secret` mode. It is passed directly to `respond(value)`;
that call validates/delivers it synchronously and does not mirror it to any
readable model property. The QML control clears its `text` synchronously before
calling outward on submission and whenever lifecycle, mode, request token, or
session generation changes. It also clears on cancellation, backend failure,
retry, completion, shutdown, and window destruction.

The secret control uses password echo mode, disables selection and copy/cut
actions, provides no secret-derived accessible name or description, and sets
Qt sensitive/no-predictive-text input method hints. Paste may remain available
because callers commonly use password managers; pasted content is subject to
the same validation and lifetime rules. This is logical lifetime minimization,
not a claim that Qt, allocator, kernel, compositor, or input-method copies can
be securely erased.

### 4.4 External text normalization

All external prompt strings, identity labels, action messages, and requester
metadata pass through one normalizer before entering a model:

- decode only through an explicit valid Unicode/UTF-8 boundary;
- replace NUL and disallowed C0/C1 controls with a visible replacement or
  normalized space while preserving ordinary tabs/newlines only where useful;
- normalize CRLF/CR to LF and bound consecutive blank lines;
- cap each field by encoded bytes and displayed code points, appending a
  neutral truncation marker;
- set QML text to `PlainText`, disable link activation, and never construct QML,
  URLs, resource paths, commands, or rich text from the value.

The UI also bounds line count and scroll area height, so hostile text cannot
expand the dialog off-screen. Polkit `details` are copied only from an explicit
allowlist of display-safe keys chosen during implementation (for example a
vendor/application label). Unknown keys and path-like or executable values are
omitted. The action ID is shown only as `requestReference`: inert plain text,
never an executable path or lookup URL.

## 5. Shared Dialog

Both frontends create a regular Qt `Window`/`ApplicationWindow`, yielding an
`xdg_toplevel` managed by the compositor. It requests centering, keyboard
activation, and `Qt.ApplicationModal` modality within its own process. The
compositor remains the final authority for placement and focus. No layer-shell
protocol or shell surface manager is involved.

The dialog renders request context, optional ordered identity selection,
ordered informational/error messages, prompt input, progress, retry, confirm,
and cancel controls according to model state. Initial focus enters the first
meaningful control. Tab/Backtab follows visual order; arrows navigate
identities; Enter accepts the valid primary action; Escape cancels where
cancellation is permitted. Busy state disables response, identity, retry, and
accept actions while retaining a responsive cancel action when the backend can
cancel.

Accessibility roles, names, descriptions, checked/current state, busy state,
and focus order describe the operation without incorporating entered secret
text. Palette, typography, controls, and focus treatment come from installed
HoloNight modules. Dark and light variants are exercised without importing the
shell module.

## 6. Polkit Frontend

### 6.1 Listener and registration

`PolkitListenerBridge` implements the low-level `PolkitAgentListener` vtable
from `libpolkit-agent-1` and registers against a session subject constructed
from the service instance's validated `XDG_SESSION_ID`. Its callback immediately
copies safe request metadata, preserves the authority-provided identity order,
retains the cookie and request `GCancellable`, and posts work to the Qt event
loop. GLib integration uses Qt's event dispatcher or a nonblocking bridge;
authority and PAM work never blocks the QML render thread.

The raw listener is required even though PAM conversations use
`PolkitQt1::Agent::Session`: PolkitQt's higher-level listener loses the exact
cancellation/request identity and does not preserve ordered duplicates in the
form required by the specification. Duplicate stable identities from the
authority are retained as received; the UI therefore contains every entry
exactly once by position without inventing, sorting, or de-duplicating it.

Registration conflict is classified separately from transient startup or
runtime failure. The executable writes a safe actionable diagnostic to stderr
and the journal, does not inspect or mutate the competing agent, and exits with
a dedicated constant such as `RegistrationConflictExit = 78`. The installed
unit uses the exact same value in `RestartPreventExitStatus`.

### 6.2 Request record and FIFO coordinator

Every callback creates an immutable `PolkitRequest` record containing:

- a monotonically unique, non-secret request token;
- normalized message, inert action ID, and safe requester details;
- identities in exact authority order and the selected stable identity;
- retained cookie, `GCancellable`, and authority completion callback;
- an exactly-once completion guard;
- a monotonically increasing session-generation counter.

`PolkitRequestCoordinator` owns a FIFO queue and at most one active record. A
new request is appended; the head opens only after the previous record reaches
a terminal outcome. Authority cancellation removes a queued record and
completes it unsuccessfully once, or cancels the active PolkitQt session and
closes its prompt. Completing the head schedules activation of the next item on
the Qt event loop, avoiding reentrant model/window replacement.

All callbacks capture request token and session generation. The coordinator
accepts a callback only when both match the current active record. Thus a late
message, response request, completion, or cancellation from an old request or
failed session cannot alter or complete a newer request.

### 6.3 Identity policy and PAM sessions

The default identity is selected by stable UID/Polkit identity comparison:

1. select the current process UID if an eligible matching identity exists;
2. otherwise select the first authority-provided identity;
3. if the list is empty, log a safe classification and complete unsuccessfully
   exactly once without constructing/showing the dialog.

With multiple identities, this is only the initial selection and the user must
explicitly confirm it. A fresh `PolkitQt1::Agent::Session` is created for the
chosen identity and original cookie. Its PAM callbacks append information/error
messages or set a visible/hidden prompt in order. Each answer is sent only to
that session.

An unsuccessful session while the authority request remains active destroys
the session, clears input, increments the generation, and presents
`RetryableError`. `retry()` returns to identity selection and always creates a
new session after selection. Successful session completion completes the
authority request once. User cancellation, authority cancellation, session
termination, shutdown, and unrecoverable errors cancel the conversation,
clear transient UI, close the window, and report no success.

The executable has no password command-line option, stdout protocol, D-Bus
export, control socket, crash uploader, or shell connection. Polkit success is
reported only through the retained authority callback/session contract.

## 7. Askpass Frontend

### 7.1 Entry-point dispatch

One binary implementation is installed as three real executable names or
symlinked entry points. Mode selection uses only the basename of `argv[0]` and
the exact `SSH_ASKPASS_PROMPT` value; it never inspects the parent process:

<!-- markdownlint-disable MD013 -->

| Basename | Mode selection |
| --- | --- |
| `holonight-sudo-askpass` | Always `Secret` |
| `holonight-ssh-askpass` | `confirm` -> `Confirmation`; `none` -> `Notification`; otherwise `Secret` |
| `holonight-askpass` | Recognized `confirm`/`none` first; otherwise `Secret` |

<!-- markdownlint-enable MD013 -->

The optional first argument is normalized prompt text. Extra arguments are
ignored or rejected with a safe diagnostic; they are never responses. Each
process creates at most one dialog and reaches one terminal process outcome.

### 7.2 Secret response contract

Before acceptance, `SecretValidator` converts the QString to UTF-8 and verifies
that converting those bytes back produces the original Unicode scalar
sequence. It rejects empty input, CR, LF, NUL, invalid/unpaired Unicode input,
and an encoded payload over 1022 bytes. A validation failure keeps the window
open, clears or permits correction according to the UI control policy, and
shows a generic error without writing stdout.

On valid acceptance, the QML value is synchronously cleared and a dedicated
`ProtocolWriter` writes the already validated byte array followed by exactly
one LF to file descriptor 1. It loops for partial writes, retries `EINTR`, and
treats zero-length or other failed writes as failure. No bytes are emitted
until validation has completed. Since a later write can still fail after a
partial OS-level write, the helper returns failure and emits no further bytes;
the protocol cannot roll back bytes already accepted by the pipe.

Accepted secret output exits `0`. Cancellation, display/setup failure,
validation followed by cancellation, internal failure, or write failure exits
`1`. Diagnostics go only to stderr/journal and never contain the prompt
response. stdout is never connected to Qt logging.

### 7.3 Confirmation and notification

Confirmation presents explicit accept/reject controls. Acceptance exits `0`;
rejection, cancellation, dismissal, or failure exits `1`. All confirmation
outcomes leave stdout empty.

Notification presents only inert prompt content: no editable field, response,
accept, reject, or acknowledgement control. It remains visible until OpenSSH
terminates it. `SIGTERM` is blocked/handled through a nonblocking self-pipe or
Linux `signalfd` watched by `QSocketNotifier`; the Qt-thread handler closes the
window and exits cleanly without a crash diagnostic. No exit status is treated
as acknowledgement and stdout remains empty.

### 7.4 Test-only process control

When and only when compiled with `BUILD_TESTS`, the real helper may consume an
already-open inherited control file descriptor whose number is provided by a
non-secret test convention. The pipe carries UI actions and response bytes
from the contract-test parent after process creation. It is unavailable in
production builds and never uses argv, environment values, logs, files, or
persistent storage for the secret. This seam drives acceptance, rejection,
cancellation, dismissal, and setup failure while exercising the production
mode parser, validator, dialog controller, writer, and exit path.

## 8. Process Hardening and Observability

Both `main()` functions disable core dumps with `setrlimit(RLIMIT_CORE, 0)`
before constructing `QGuiApplication`/`QApplication`, loading QML, or accepting
external prompt data. Failure to establish this boundary is a startup failure.
Packaged services reinforce it with `LimitCORE=0`. The applications do not
install a crash handler or opt into crash-report persistence.

Logs use structured safe fields only: frontend (`polkit` or `askpass`), phase,
opaque request token where applicable, and a fixed failure classification.
They exclude raw external prompt text, identity display labels when avoidable,
cookies, environment dumps, PAM messages, and all response content. The
one-shot process uses stderr/journal only; its stdout belongs exclusively to
the askpass contract.

The design prevents ordinary application-object, clipboard, IPC, log, and
crash-dump exposure. Same-UID memory inspection and compromise of the
compositor, input method, kernel, or root are explicit threat-model boundaries.

## 9. Build, Installation, and Session Integration

### 9.1 Configuration and installation

CMake requires Qt 6 Quick/QML support, the installed HoloNight QML packages,
PolkitQt6 `Agent` version 0.200.0 or newer, and `polkit-agent-1` development
metadata. Missing or old dependencies fail configuration with an actionable
message. OpenSSH 8.2+ and sudo 1.8.15+ are runtime compatibility baselines, not
libraries linked into the helper. Python 3 (standard library only) is required
for the UWSM Wayland-peer discovery helper.

Installation includes:

- `holonight-polkit-agent`;
- the askpass implementation under `holonight-sudo-askpass`,
  `holonight-ssh-askpass`, and `holonight-askpass`;
- the `Holonight.Authentication` QML module and its qmltypes metadata;
- the session-keyed systemd user template, startup wrapper, and
  `holonight-wayland-session-environment` discovery helper;
- HoloNight session activation/environment integration.

No install action reads or writes `/etc/sudo.conf`, disables a third-party
agent, or enables the Polkit unit globally.

### 9.2 Session-keyed Polkit unit

`holonight-polkit-agent@.service` is instantiated as
`holonight-polkit-agent@${XDG_SESSION_ID}.service`. It is ordered with the
HoloNight graphical-session target but has no `Requires`, `PartOf`, `BindsTo`,
or ordering dependency on `holonight-shell.service`. It contains:

```ini
Restart=on-failure
RestartSec=2s
StartLimitIntervalSec=30s
StartLimitBurst=3
RestartPreventExitStatus=78
LimitCORE=0
```

The exact conflict status is defined once in generated configuration shared by
the executable/unit packaging. Bounded restart recovers from crashes while a
registration conflict cannot restart-loop.

The `ExecStart` wrapper treats `%i` as the required session ID. It waits with a
bounded timeout for logind to expose that exact graphical session and for the
session-scoped Wayland socket/environment to be usable. It resolves and
validates the session owner, type, active state, runtime directory,
`WAYLAND_DISPLAY`, and other required Qt variables, then captures them into the
child environment immediately before `exec`.

The wrapper first checks processes inside the exact logind session scope.
When UWSM moves the compositor to a user-service cgroup, a small Python helper
connects to candidate `wayland-*` Unix sockets and obtains `SO_PEERCRED`.
The socket and peer must belong to the current UID, and the peer process's
original environment must name the required session and runtime directory.
Only one matching compositor is accepted; stale sockets, other sessions, and
ambiguous matches cannot select a display. The helper emits only the session,
runtime, verified Wayland socket name, and allowlisted Qt/cursor settings.

The peer route forces `QT_QPA_PLATFORM=wayland` and clears `DISPLAY`, because
Xwayland may start after the compositor's original environment was created.
An unrelated manager `DISPLAY` is therefore ignored on this Wayland-only route;
manager session, runtime, Wayland, and Qt-setting conflicts are still rejected.
A compositor that does not publish `XDG_SESSION_ID` in its original environment
and has no usable session-scope descendant is deliberately not guessed.

The wrapper must not choose a session by reading mutable global values from the
user manager. If global `XDG_SESSION_ID`, `WAYLAND_DISPLAY`, or related state
points to another session, it refuses startup with a safe routing diagnostic.
This prevents concurrent graphical sessions for one UID from stealing each
other's prompt. Direct/manual startup after a compositor is already available
is supported only when its explicit session ID and captured environment pass
the same validation.

### 9.3 Explicit session activation and askpass environment

Only HoloNight-managed session activation starts the template for the current
explicit `XDG_SESSION_ID`; package installation alone does not start, enable,
stop, mask, or replace any agent. A conflict is surfaced in the journal with a
manual remediation hint and the third-party process remains untouched.

The session launcher sets absolute installed paths:

```text
SUDO_ASKPASS=<libexec-or-bindir>/holonight-sudo-askpass
SSH_ASKPASS=<libexec-or-bindir>/holonight-ssh-askpass
```

It exports them to its process descendants, imports exactly these values into
the systemd user activation environment, and updates the D-Bus activation
environment. It never exports `SSH_ASKPASS_PROMPT`, never points either variable
at the compatibility entry point, and never changes sudo policy. Consequently,
plain terminal sudo remains TTY-driven unless its caller selects askpass (for
example `sudo -A`).

## 10. Test Architecture

### 10.1 Core unit tests

Table-driven tests cover every legal transition and every invalid invokable,
including lifecycle/mode mismatches. Spies assert process-local callback count,
response routing, exactly-once terminal behavior, and synchronous secret-field
clear notifications on submission, cancellation, failure, retry, completion,
shutdown, superseding request token, and generation change.

Normalization tests cover markup, URL/path/shell-like strings, CR/LF, NUL,
controls, excessive lines, code-point/byte limits, safe detail allowlisting,
and inert action IDs. Identity tests cover current-UID preference,
administrator-only lists, stable-ID equality, original order including
duplicates, explicit multi-identity selection, first-entry fallback, and empty
input.

### 10.2 Polkit tests

Injectable `AuthorityAdapter` and `SessionFactory` seams replace registration
and `PolkitQt1::Agent::Session` in deterministic tests. They drive ordered
mixed visible/hidden PAM prompts, info/error messages, successful and failed
completion, retry with identity reselection and a fresh session, user/session/
authority cancellation, FIFO arrival, queued cancellation, and stale token or
generation callbacks. Every request asserts exactly one upstream completion.

Separate integration tests cover registration conflict and distinct routing
for two session IDs belonging to one UID, including different Wayland
environments, without requiring the live system authority.

### 10.3 Askpass subprocess tests

Contract tests execute the real `BUILD_TESTS` helper through all three basenames
and capture stdout bytes, stderr bytes, and exit status separately. Cases cover
secret success, cancellation, window dismissal, display/setup/internal
failure, UTF-8 round-trip failures, empty input, CR/LF/NUL, exactly 1022 bytes,
oversize input, partial writes, and interrupted writes. Expected successful
stdout is byte-for-byte `payload + 0x0a`; every non-secret or failed path is
empty.

OpenSSH-mode cases cover recognized and unrecognized values, compatibility
precedence, confirmation accept/reject/cancel, and notification UI followed by
`SIGTERM`. Tests assert that notification has no response controls, produces
empty stdout, and terminates without a crash/error diagnostic.

### 10.4 QML and accessibility tests

The independent QML harness registers only the neutral test model plus
installed HoloNight dependencies. It verifies the absence of shell imports,
keyboard-only focus/identity/secret/confirmation/retry/cancel flows, masking,
selection and copy suppression, input-method hints, clearing on all lifecycle
edges, disabled busy controls, bounded malicious text, accessibility metadata,
and light/dark palettes.

### 10.5 Verification order

Implementation runs the narrowest core, Polkit, askpass, and QML tests first,
then:

```text
task test
task qml-lint
task qmltypes-check
task architecture-check
```

The live Hyprland record then covers service daemon reload/activation, a real
authentication-requiring Polkit test policy, `sudo -A`, SSH passphrase,
`SSH_ASKPASS_PROMPT=confirm`, notification termination, competing agent,
keyboard focus, two sessions where available, and dark/light appearance.
OpenSSH commands force helper selection with `SSH_ASKPASS_REQUIRE=force`.
Screenshots never contain typed secrets. Each command, observed result,
environment limitation, and pre-existing failure is recorded separately.

## 11. Operational Boundaries and Trade-offs

- Regular `xdg_toplevel` placement and focus are compositor-managed; centering
  and activation are requests rather than layer-shell guarantees.
- Application modality affects only the frontend process. Multiple independent
  askpass helpers or agents for different sessions may display concurrently.
- The process boundary prevents secrets from entering the shell, but cannot
  protect against same-UID memory inspection or compromised privileged/system
  components, compositor, or input method.
- The startup wrapper depends on a usable Wayland environment for the exact
  logind session. Direct activation after compositor startup is supported;
  activation before that boundary waits only for a bounded period and fails
  safely rather than selecting another session.
- No credential cache is added. Policy and any authorization caching remain
  owned by Polkit, PAM, sudo, and OpenSSH.
- Systemd password agents, disk unlock, keyrings, browser credentials, and
  application login dialogs remain outside this architecture.

## 12. Requirement Traceability

<!-- markdownlint-disable MD013 -->

| Requirement | Design coverage |
| --- | --- |
| REQ-F-001 | §2–§3 define the shared standalone QML module. |
| REQ-F-002 | §4 defines a protocol-neutral model and test controller. |
| REQ-F-003 | §4.1 defines all required properties, enums, and list roles. |
| REQ-F-004 | §4.1–§4.2 define operations, validation, and all lifecycle states. |
| REQ-F-005 | §4.3 defines transient masked entry with no readable model copy. |
| REQ-F-006 | §5 defines keyboard flows, focus, and busy behavior. |
| REQ-F-007 | §5 defines installed HoloNight styling and palette verification. |
| REQ-F-008 | §1–§2 and §6 define an independent persistent agent. |
| REQ-F-009 | §6.1 and §9.2 bind registration to the explicit session. |
| REQ-F-010 | §4.4 and §6.1 map normalized, plain-text request data and cancellation. |
| REQ-F-011 | §4.2 and §6.3 retain order and require explicit multiple-identity selection. |
| REQ-F-012 | §6.3 defines current-UID, first-entry, and empty-list policy. |
| REQ-F-013 | §6.3 defines ordered mixed-mode PAM conversation mapping. |
| REQ-F-014 | §4.2 and §6.3 define cleared retry with a fresh generation/session. |
| REQ-F-015 | §6.2–§6.3 define all cancellation sources and stale-callback rejection. |
| REQ-F-016 | §6.1 and §9.2 define the conflict status and restart prevention. |
| REQ-F-017 | §6.2 defines FIFO ownership, queued cancellation, and completion guards. |
| REQ-F-018 | §2 and §7.1 define one-shot isolated entry points. |
| REQ-F-019 | §7.2 defines sudo accepted-output bytes and status. |
| REQ-F-020 | §7.2 defines empty-output failure/cancellation behavior. |
| REQ-F-021 | §7.1–§7.2 define SSH secret fallback and output. |
| REQ-F-022 | §7.3 defines confirmation status-only behavior. |
| REQ-F-023 | §7.3 defines response-free notification and safe SIGTERM. |
| REQ-F-024 | §7.1 defines compatibility-name precedence. |
| REQ-F-025 | §7.2 defines UTF-8, character, empty, and 1022-byte validation. |
| REQ-F-026 | §7.2 and §8 reserve stdout and constrain diagnostics. |
| REQ-S-001 | §2 and §4.3 keep responses in the collecting process/protocol. |
| REQ-S-002 | §2, §4.3, and §8 exclude shell IPC, argv, logs, and persistence. |
| REQ-S-003 | §4.3 specifies clearing on every terminal/superseding transition. |
| REQ-S-004 | §6.3 excludes Polkit password output interfaces. |
| REQ-S-005 | §4.4 specifies inert normalized plain-text rendering. |
| REQ-S-006 | §4.3 specifies copy suppression and sensitive IM hints. |
| REQ-S-007 | §8 and §9.2 disable dumps before Qt and in systemd. |
| REQ-I-001 | §9.1 defines dependency baselines and installed artifacts. |
| REQ-I-002 | §9.2 defines the independent session-keyed template. |
| REQ-I-003 | §9.3 starts only from explicit managed-session activation. |
| REQ-I-004 | §9.3 exports absolute paths through all activation environments. |
| REQ-I-005 | §9.1 and §9.3 leave `/etc/sudo.conf` untouched. |
| REQ-I-006 | §6.1 and §9.2–§9.3 preserve competing agents and avoid restart loops. |
| REQ-V-001 | §10.1–§10.2 define deterministic Polkit/core coverage. |
| REQ-V-002 | §10.3 defines real-process raw-output contract tests. |
| REQ-V-003 | §10.3 covers SSH secret, confirm, and notification. |
| REQ-V-004 | §10.4 defines independent keyboard/state QML tests. |
| REQ-V-005 | §10.2–§10.3 define injectable Polkit and real askpass integration. |
| REQ-V-006 | §10.5 defines the live Hyprland verification record. |
| REQ-V-007 | §10.5 defines focused-first and full verification order. |
| REQ-NF-001 | §1–§2 and §8 preserve isolation while sharing UI code. |
| REQ-NF-002 | §6.2 and §7 define single-owner/exactly-once independent lifecycles. |
| REQ-NF-003 | §8 defines actionable, non-secret diagnostics. |
| REQ-NF-004 | §5 and §10.4 define non-secret accessibility semantics. |
| REQ-NF-005 | §5, §6.1, and §10.2 keep backend work off the render thread. |
| REQ-C-001 | §9.2–§9.3 prohibit automatic third-party replacement. |
| REQ-C-002 | §9.3 preserves TTY sudo unless askpass is requested. |
| REQ-C-003 | §4.3 and §11 introduce no response cache. |
| REQ-C-004 | §11 keeps unrelated authentication domains out of scope. |
| REQ-C-005 | This design defines architecture/file layout; sequencing remains for `TASKS.md`. |
| REQ-C-006 | §4.3, §8, and §11 state the threat-model boundary. |

<!-- markdownlint-enable MD013 -->

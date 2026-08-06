# AI Chat Integration — Design

## Architecture

```text
Hyprland keybind
  -> holonight-shell control socket
  -> AiChatService
  -> session D-Bus
  -> holonight-chat
```

`ControlServer` remains a transport parser. It recognizes a new
`chat:toggle:<monitor>` command and emits `toggleChatRequested(monitor)`.
`ShellApplication` resolves an empty argument through
`ActiveWindowService::focusedMonitorName()` and calls
`AiChatService::togglePanel()`.

The control protocol is EOF-delimited. OpenBSD netcat clients use `-N` so stdin
EOF shuts down the socket and lets `ControlServer` dispatch the accumulated
command.

`AiChatService` lives in `holonight-services`, owns no presentation state, and
uses an asynchronous D-Bus call with a bounded timeout. D-Bus activation is provided
by the chat application's installed service file. Errors are reported through the
shell logging system and otherwise ignored.

The shell does not mirror panel visibility. `TogglePanel` is the single lifecycle
operation, keeping state ownership in `holonight-chat`.

## Verification

- Pure command-decoding tests cover the new control-socket command.
- The proxy has a narrow overridable call seam so tests can observe the requested
  monitor without a compositor or a running chat application.
- A live smoke test verifies D-Bus activation and output routing.

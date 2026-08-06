# AI Chat Integration — Specification

## Objective

Allow HoloNight Shell keybindings and shell controls to toggle the standalone
`holonight-chat` quick panel without loading chat code or QML into the shell process.

## D-Bus contract

The shell calls the session-bus service `org.holonight.Chat`, object
`/org/holonight/Chat`, interface `org.holonight.Chat1`.

## Requirements

- The shell shall expose `chat:toggle:<monitor>` through its existing control socket.
- A non-empty monitor name shall be forwarded to `TogglePanel(string outputName)`.
- An empty monitor name shall target the shell's currently focused monitor.
- The D-Bus call shall be asynchronous and shall allow D-Bus activation to start
  `holonight-chat`.
- Failure to activate or call the chat service shall be logged and shall not affect
  shell stability.
- The shell shall not link chat implementation code, load chat QML, or own a chat
  surface.

## Acceptance checks

- Command decoding recognizes `chat:toggle:<monitor>` without changing existing
  commands.
- Dispatching the command sends one `TogglePanel` call with the decoded monitor, or
  the focused monitor when the command leaves it empty.
- With `holonight-chat` stopped and its D-Bus service installed, the command starts
  chat and opens the panel on the requested monitor.
- Repeating the command closes the panel and restores the workspace.
- A chat process failure leaves `holonight-shell` running.

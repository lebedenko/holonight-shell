# SPEC — active-window-icon

## Overview

Add a category-derived icon that prepends the active window title in `ActiveWindowSection`. The icon is a pure QML Canvas 2D drawing (16×16 px), single solid color, no glow or gradient. The category is resolved in C++ by scanning XDG desktop files.

---

## Functional Requirements

### Category Resolution (C++)

**REQ-F-001** (Event-driven)
When `ActiveWindowService.appClass` changes, the system shall search for a matching `.desktop` file in `~/.local/share/applications/` and `/usr/share/applications/` (in that order, no recursion).

- Acceptance: Given appClass changes to "firefox", the service reads `firefox.desktop` (or `Firefox.desktop`) from the XDG paths within one event-loop cycle after the change.

**REQ-F-002** (Ubiquitous)
The desktop file search shall perform two passes:
1. Exact filename match: `{appClass}.desktop` and `{appClass_lowercase}.desktop`.
2. If no match, case-insensitive scan of `Name=` and `Exec=` fields across all `.desktop` files in the two directories.

- Acceptance: Given appClass "Alacritty" and a file `alacritty.desktop`, pass 1 resolves it. Given appClass "term" with no exact match but a file whose `Name=Terminal`, pass 2 resolves it.

**REQ-F-003** (Conditional)
If a `.desktop` file is found, the system shall read the `Categories=` field, split on `;`, and map the first matching token against the priority list below to a category string. If no token matches, the category shall be an empty string.

Priority (first match wins):

| Token | Category |
|---|---|
| `WebBrowser` | `browser` |
| `TextEditor` | `editor` |
| `Development` | `editor` |
| `TerminalEmulator` | `terminal` |
| `FileManager` | `files` |
| `InstantMessaging` | `chat` |
| `Chat` | `chat` |
| `Audio` | `music` |
| `Music` | `music` |
| `Video` | `video` |
| `Settings` | `settings` |
| `System` | `settings` |

- Acceptance: Given `Categories=Network;WebBrowser;`, the resolved category is `browser`. Given `Categories=Utility;`, the resolved category is `""`.

**REQ-F-004** (Unwanted behaviour)
If no `.desktop` file is found, or the `Categories=` field is absent or empty, the system shall set `category` to an empty string without logging an error.

- Acceptance: Given appClass "xterm" with no matching `.desktop` file, `category` equals `""` and no error is emitted to the log.

**REQ-F-005** (Ubiquitous)
`ActiveWindowService` shall expose a read-only `category` Q_PROPERTY (QString) with a `categoryChanged` notify signal, updated whenever the resolved category changes.

- Acceptance: A QML binding on `ActiveWindowService.category` updates reactively when `appClass` changes to an app with a different category.

**REQ-F-006** (State-driven)
While `ActiveWindowService.title` is empty, `category` shall be an empty string.

- Acceptance: Given no focused window (title == ""), `ActiveWindowService.category === ""`.

---

### AppWindowIcon Component

**REQ-F-007** (Ubiquitous)
The system shall provide a `AppWindowIcon` QML component at `src/qml/Topbar/AppWindowIcon.qml` that accepts a `category` string property and renders a 16×16 px Canvas 2D icon.

- Acceptance: `AppWindowIcon { category: "browser" }` renders a visible 16×16 px item without errors in qmllint.

**REQ-F-008** (Ubiquitous)
All icon shapes shall be stroked with `HoloniightPalette.onSurface` as the sole color. The Canvas element shall have `opacity: 0.9`. No gradient, no MultiEffect glow, no fill color other than transparent.

- Acceptance: The Canvas `strokeStyle` is always `HoloniightPalette.onSurface`. There is no `MultiEffect` child and no `createLinearGradient` call in the component.

**REQ-F-009** (Event-driven)
When `category === "browser"`, the component shall draw a circle with one vertical and two horizontal ellipse arcs to suggest a globe.

- Acceptance: Rendered icon visually represents a globe outline at 16×16 px.

**REQ-F-010** (Event-driven)
When `category === "editor"`, the component shall draw `< >` angle-bracket glyphs.

- Acceptance: Rendered icon shows two opposing angle brackets.

**REQ-F-011** (Event-driven)
When `category === "terminal"`, the component shall draw a rounded rectangle frame with a `>` chevron and a short horizontal line inside (framed prompt glyph `>_`).

- Acceptance: Rendered icon shows a terminal window outline containing a `>_` prompt symbol.

**REQ-F-012** (Event-driven)
When `category === "files"`, the component shall draw an outlined folder shape (rectangular body with a tabbed top-left corner).

- Acceptance: Rendered icon shows a recognisable folder outline.

**REQ-F-013** (Event-driven)
When `category === "chat"`, the component shall draw a rounded speech bubble (rounded rectangle with a small tail at the bottom-left).

- Acceptance: Rendered icon shows a speech bubble outline.

**REQ-F-014** (Event-driven)
When `category === "music"`, the component shall draw three vertical bars of varying height to suggest a waveform.

- Acceptance: Rendered icon shows three waveform bars.

**REQ-F-015** (Event-driven)
When `category === "video"`, the component shall draw a rounded rectangle containing a right-pointing triangle (play-frame).

- Acceptance: Rendered icon shows a screen with a play triangle inside.

**REQ-F-016** (Event-driven)
When `category === "settings"`, the component shall draw a minimal cog: a small circle surrounded by evenly-spaced rectangular teeth.

- Acceptance: Rendered icon shows a recognisable gear/cog outline.

**REQ-F-017** (Conditional)
If `category` is empty or does not match any of the eight defined values, the component shall draw the generic window icon (rounded rectangle with a top divider line, matching the existing `"window"` shape in `BarIcon.qml`).

- Acceptance: Given `category === "unknown"`, the component renders the same window outline as `BarIcon { name: "window" }`.

---

### ActiveWindowSection Layout

**REQ-F-018** (Ubiquitous)
In `ActiveWindowSection.qml`, the title row shall be a `Row` containing `AppWindowIcon` (bound to `ActiveWindowService.category`) followed by the title `Label`. Row spacing shall be 6 px.

- Acceptance: The title row is a `Row` with `spacing: 6`. `AppWindowIcon` appears before the `Label` in source order.

**REQ-F-019** (State-driven)
`AppWindowIcon` shall be visible only when `ActiveWindowService.title !== ""`.

- Acceptance: Given an empty active window, `AppWindowIcon` is not rendered (visible: false or inside the existing visibility guard).

**REQ-F-020** (Event-driven)
When `ActiveWindowService.category` changes, `AppWindowIcon` shall repaint immediately without requiring a window reload.

- Acceptance: Switching focus from "firefox" to "kitty" updates the icon from globe to terminal glyph within one frame.

---

## Non-Functional Requirements

**REQ-NF-001**
The desktop file lookup shall execute on a background thread (e.g. `QtConcurrent::run`) so that the UI thread is never blocked during file I/O.

- Acceptance: The `ActiveWindowService` does not call synchronous file-read APIs on the main thread. Verified by code inspection.

**REQ-NF-002**
The `AppWindowIcon` Canvas shall repaint in under 2 ms on the render thread for the 16×16 surface size.

- Acceptance: No frame drops are observable at 60 Hz when the active window changes rapidly during manual testing.

**REQ-NF-003**
The resolved category shall be cached per `appClass` string so that repeated focus switches to the same app do not re-scan the filesystem.

- Acceptance: Switching focus back to a previously-seen app does not trigger a new file scan (verified by adding a qCDebug trace to the scan path).

---

## Constraint Requirements

**REQ-C-001**
The icon component shall be located at `src/qml/Topbar/AppWindowIcon.qml` and registered in `CMakeLists.txt` with a `QT_RESOURCE_ALIAS` that strips the `src/qml/` prefix.

- Acceptance: `import HolonightShell` exposes `AppWindowIcon` and qmllint passes without import errors.

**REQ-C-002**
The icon shall be drawn entirely with Canvas 2D API calls. No external SVG assets, no `Image` element, no Qt Quick Shapes.

- Acceptance: `AppWindowIcon.qml` contains no `Image` element, no `Shape`/`ShapePath`, and no asset file references.

**REQ-C-003**
No `MultiEffect`, `Qt5Compat.GraphicalEffects`, or any other visual effect shall be used in `AppWindowIcon`.

- Acceptance: qmllint on `AppWindowIcon.qml` shows no import of `QtQuick.Effects` or `Qt5Compat`.

**REQ-C-004**
Category resolution logic shall reside entirely in `ActiveWindowService.cpp`/`.h`. No file I/O shall occur in QML or JS.

- Acceptance: `AppWindowIcon.qml` contains no `Qt.resolvedUrl`, no `XMLHttpRequest`, and no filesystem access.

**REQ-C-005**
The desktop file scanner shall handle malformed files (missing `=`, non-UTF-8 bytes, binary files) without crashing or throwing exceptions. Such files shall be silently skipped.

- Acceptance: Placing a zero-byte file and a binary file in the search path does not cause a crash or an unhandled exception.

# DESIGN: Topbar Window Title

**SDD Session:** topbar-window-title  
**Feature:** Hyprland IPC event socket binding, `ActiveWindowService` C++ singleton, and `ActiveWindowSection` QML component  
**Status:** Design  
**Last Updated:** 2026-05-21

---

## Overview

This session adds the active-window title strip to the topbar center section. It introduces one C++ service and one QML component that follow the singleton-plus-QML-consumer pattern established in the workspaces session.

**What is added:**

1. **`ActiveWindowService`** — a `QObject` singleton that opens a `QLocalSocket` to Hyprland's IPC event socket, accumulates partial reads into a `QByteArray` line buffer, parses `activewindow>>` events, and exposes `title` and `appClass` Q_PROPERTYs to QML.
2. **`ActiveWindowSection.qml`** — a `BarSection` subtype placed in the topbar center with `Layout.fillWidth: true`. Hidden when `title` is empty; shows a comment label and the elided title text when visible.
3. **`TopBar.qml`** — modified to replace the current center `Item { Layout.fillWidth: true }` spacer with `ActiveWindowSection`.
4. **`CMakeLists.txt`** — extended with new C++ sources and a new QML file registration.
5. **`main.cpp`** — extended to construct `ActiveWindowService` and register it as a QML singleton.

After this session the topbar center shows the title of the currently-focused Hyprland window, updating in real time from IPC events, and hides entirely when no window is active.

---

## Component Map

```
holonight-shell/
├── src/
│   ├── ActiveWindowService.h          (NEW) QObject singleton — socket, buffer, title/appClass properties
│   ├── ActiveWindowService.cpp        (NEW) socket init, QSocketNotifier, line parsing
│   ├── main.cpp                       (MODIFIED) construct ActiveWindowService, qmlRegisterSingletonInstance
│   │
│   └── qml/Topbar/
│       ├── TopBar.qml                 (MODIFIED) replace center spacer with ActiveWindowSection
│       └── ActiveWindowSection.qml   (NEW) BarSection with comment label + title Text, visibility binding
│
└── CMakeLists.txt                     (MODIFIED) new C++ sources, new QML file alias + QML_FILES entry
```

No new protocol XML. No new Wayland protocol binding. No new CMake `find_package` additions (Qt6::Network is already linked; `QLocalSocket` lives in Qt6::Network).

---

## Data Flow Diagram

```
Hyprland compositor
      │
      │  (line-based text events over Unix domain socket)
      │
      ▼
/tmp/hypr/<SIG>/.socket2.sock
      │
      │  QLocalSocket (read-only, non-blocking)
      │
      ▼
QSocketNotifier (Read mode, main Qt event loop)
      │
      │  activated() signal → onSocketReadable() slot
      │
      ▼
ActiveWindowService::onSocketReadable()
      │
      ├── read up to 4096 bytes → append to buffer_
      ├── scan buffer_ for '\n'-terminated lines
      ├── for each complete line:
      │     if starts with "activewindow>>"
      │       → parse appClass, title (split on first comma)
      │       → setTitle(parsed_title)     → Q_PROPERTY titleChanged()
      │       → setAppClass(parsed_class)  → Q_PROPERTY appClassChanged()
      └── compact buffer_ (remove consumed bytes)
            │
            ▼
    QML property binding (automatic)
            │
            ▼
    ActiveWindowSection
            │
            ├── visible: ActiveWindowService.title !== ""
            └── titleText.text: ActiveWindowService.title
```

---

## `ActiveWindowService` Interface

Full class declaration for `src/ActiveWindowService.h`:

```cpp
#pragma once

#include <QByteArray>
#include <QLocalSocket>
#include <QObject>
#include <QSocketNotifier>
#include <QString>

class ActiveWindowService : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString title    READ title    NOTIFY titleChanged)
  Q_PROPERTY(QString appClass READ appClass NOTIFY appClassChanged)

 public:
  explicit ActiveWindowService(QObject* parent = nullptr);
  ~ActiveWindowService() override;

  ActiveWindowService(const ActiveWindowService&) = delete;
  ActiveWindowService& operator=(const ActiveWindowService&) = delete;
  ActiveWindowService(ActiveWindowService&&) = delete;
  ActiveWindowService& operator=(ActiveWindowService&&) = delete;

  [[nodiscard]] QString title() const { return title_; }
  [[nodiscard]] QString appClass() const { return app_class_; }

 Q_SIGNALS:
  void titleChanged();
  void appClassChanged();

 private Q_SLOTS:
  void onSocketReadable();

 private:
  void connectSocket();
  void parseLine(const QByteArray& line);
  void setTitle(const QString& value);
  void setAppClass(const QString& value);

  QLocalSocket*    socket_{nullptr};
  QSocketNotifier* notifier_{nullptr};
  QByteArray       buffer_;
  QString          title_;
  QString          app_class_;
};
```

### Key implementation notes

**`connectSocket()`** — called from the constructor. Reads `HYPRLAND_INSTANCE_SIGNATURE` via `qgetenv()`. If empty, logs a diagnostic and returns without touching the socket. Otherwise constructs the socket path as:

```cpp
QString path = QStringLiteral("/tmp/hypr/") + sig + QStringLiteral("/.socket2.sock");
```

Opens `socket_` in `QLocalSocket::ReadOnly` mode. On successful connection, creates `notifier_` with `QSocketNotifier::Read` on `socket_->socketDescriptor()` and connects its `activated` signal to `onSocketReadable()`.

**`onSocketReadable()`** — reads up to 4096 bytes per invocation into a temporary buffer and appends to `buffer_`. Then loops, scanning for `'\n'`. Each found newline marks a complete line: the line is extracted with `buffer_.left(newlinePos)`, `buffer_.remove(0, newlinePos + 1)` compacts the buffer, and `parseLine()` is called. The loop continues until no `'\n'` remains in `buffer_`.

**`parseLine()`** — converts the raw `QByteArray` line to `QString` via `QString::fromUtf8()`. Checks whether the string starts with `"activewindow>>"`. If not, the line is skipped (no log; non-activewindow events are frequent and expected). If it matches, extracts the payload after `">>"`, splits on the first comma with `indexOf(',')`. If no comma is found, both properties are set to `""`. Otherwise `appClass` = substring before the comma, `title` = substring after.

**`setTitle()` / `setAppClass()`** — guard against no-op updates:

```cpp
void ActiveWindowService::setTitle(const QString& value) {
  if (title_ == value) return;
  title_ = value;
  emit titleChanged();
}
```

**`QLocalSocket` vs `QSocketNotifier` relationship** — `QLocalSocket` itself emits a `readyRead` signal, which might appear to make `QSocketNotifier` redundant. The notifier is used instead of `readyRead` to satisfy REQ-C-008 explicitly and because `QLocalSocket::readyRead` is documented to fire only when the socket's internal read buffer has data — it can miss bytes if the internal buffer is full and not drained. `QSocketNotifier` fires whenever the kernel's socket buffer has data (edge-triggered by the event loop), making it more reliable for streaming protocols. The socket is not put into unbuffered mode; `QLocalSocket::readData()` (or `QLocalSocket::read()`) is used inside the notifier slot.

---

## `ActiveWindowSection.qml` Structure

**Path:** `src/qml/Topbar/ActiveWindowSection.qml`

The component tree:

```
ActiveWindowSection                  ← BarSection root
  visible: ActiveWindowService.title !== ""
  Layout.fillWidth: true             ← set at usage site in TopBar.qml

  Column                             ← vertical stack of the two text rows
    anchors.verticalCenter: parent.verticalCenter
    spacing: 2

    Text  (commentLabel)             ← static label
      text: "// ACTIVE WINDOW"
      font.family: "Inter"
      font.pixelSize: 14
      color: HoloniightPalette.textMuted

    Text  (titleText)                ← dynamic title
      text: ActiveWindowService.title
      font.family: "Inter"
      font.pixelSize: 20
      elide: Text.ElideRight
      width: parent.width
      color: HoloniightPalette.textPrimary
```

**`visible` binding** — `visible: ActiveWindowService.title !== ""` is placed on the root `ActiveWindowSection` element. When `visible` is `false`, the `BarSection` takes no space in the `RowLayout` because Qt's `RowLayout` collapses invisible items by default (it does not reserve space for items with `visible: false`). This satisfies REQ-F-007 and REQ-F-008 without needing to set `implicitWidth: 0` explicitly — `visible: false` on a `Layout`-managed item already removes it from layout calculations.

**`Layout.fillWidth: true`** — set at the usage site in `TopBar.qml` (as a layout attachment), not inside `ActiveWindowSection.qml` itself. This follows the same pattern as `LogoSection` and `WorkspaceSection`, which set `Layout.alignment` at the usage site. The section's own `implicitWidth` (inherited from `BarSection`: `container.implicitWidth + 16`) is overridden by `Layout.fillWidth` when present.

**Color tokens** — the design reference (session 3 notes in `TOPBAR-PLAN.md`) specifies:
- Comment label: `HoloniightPalette.textMuted` (blue-grey secondary text, `#7aa2f7` region)
- Title text: `HoloniightPalette.textPrimary` (primary foreground, `#c0caf5` region)

If those token names do not exist in the installed `Holonight` module, `task qml-lint` will report undefined-property warnings at lint time — catch before committing.

**No `MouseArea`** — the section is purely visual (non-goal: click-to-focus).

---

## TopBar.qml Integration

The current `TopBar.qml` center spacer:

```qml
Item {
    Layout.fillWidth: true
}
```

Is **replaced** by `ActiveWindowSection`:

```qml
ActiveWindowSection {
    Layout.fillWidth: true
    Layout.alignment: Qt.AlignVCenter
}
```

The `Item` spacer is removed entirely. `ActiveWindowSection` takes over its role as the center fill. When `ActiveWindowSection` is hidden (empty title), the center space collapses and the remaining sections (`WorkspaceSection` on the left, `StatusSection` on the right) may shift — this is the intended behavior per REQ-F-007 (section hides completely, no reserved space).

The import `import HolonightShell` is already present in `TopBar.qml` (needed for `WorkspaceSection`), so `ActiveWindowSection` is automatically available without a new import statement.

The final `TopBar.qml` RowLayout order:

```
LogoSection → WorkspaceSection → ActiveWindowSection (fillWidth) → StatusSection
```

---

## CMakeLists.txt Changes

### 1. New C++ sources in `qt6_add_executable()`

Add after `ExtWorkspaceManager.cpp`:

```cmake
qt6_add_executable(holonight-shell
    ...
    src/ExtWorkspaceManager.h
    src/ExtWorkspaceManager.cpp
    src/ActiveWindowService.h      # NEW
    src/ActiveWindowService.cpp    # NEW
)
```

### 2. New QML file: `set_source_files_properties` alias

Add after the existing `WorkspaceSection.qml` alias:

```cmake
set_source_files_properties(src/qml/Topbar/ActiveWindowSection.qml
    PROPERTIES QT_RESOURCE_ALIAS "Topbar/ActiveWindowSection.qml")
```

This strips `src/qml/` from the resource path so the file is served at `qrc:/HolonightShell/Topbar/ActiveWindowSection.qml`.

### 3. New QML file in `qt6_add_qml_module()` QML_FILES

```cmake
qt6_add_qml_module(holonight-shell
    URI HolonightShell
    VERSION 1.0
    QML_FILES
        src/qml/Topbar/TopBar.qml
        src/qml/Topbar/BarBackground.qml
        src/qml/Topbar/BarSection.qml
        src/qml/Topbar/LogoSection.qml
        src/qml/Topbar/StatusSection.qml
        src/qml/Topbar/WorkspacePill.qml
        src/qml/Topbar/WorkspaceSection.qml
        src/qml/Topbar/ActiveWindowSection.qml   # NEW
)
```

### 4. No new `find_package` or `target_link_libraries` additions

`QLocalSocket` is part of `Qt6::Network`, which is already listed in `find_package` and `target_link_libraries`. `QSocketNotifier` is part of `Qt6::Core`, also already linked. No new CMake package additions are required.

### Summary of CMakeLists.txt changes

| Change | Location |
|---|---|
| Add `ActiveWindowService.h/.cpp` to `qt6_add_executable` | After `ExtWorkspaceManager.cpp` |
| Add `QT_RESOURCE_ALIAS` for `ActiveWindowSection.qml` | After `WorkspaceSection.qml` alias |
| Add `ActiveWindowSection.qml` to `QML_FILES` | After `WorkspaceSection.qml` |

---

## main.cpp Changes

Construct `ActiveWindowService` alongside the existing singletons and register it with the QML engine using the same factory-lambda pattern (one instance shared across all QML engines / monitors):

```cpp
#include "ActiveWindowService.h"
// ... existing includes ...

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);

  auto* model   = new WorkspaceModel(&app);
  auto* manager = new ExtWorkspaceManager(model, &app);
  auto* aws     = new ActiveWindowService(&app);   // NEW
  Q_UNUSED(manager)

  QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
  qmlRegisterSingletonType<WorkspaceModel>("HolonightShell", 1, 0, "WorkspaceModel",
      [model](QQmlEngine*, QJSEngine*) -> QObject* { return model; });

  QQmlEngine::setObjectOwnership(aws, QQmlEngine::CppOwnership);   // NEW
  qmlRegisterSingletonType<ActiveWindowService>(                    // NEW
      "HolonightShell", 1, 0, "ActiveWindowService",
      [aws](QQmlEngine*, QJSEngine*) -> QObject* { return aws; });

  LayerShellManager lsm;
  return QGuiApplication::exec();
}
```

`ActiveWindowService` is created before `LayerShellManager` so the singleton is ready before any `QQuickView` loads `TopBar.qml`. The factory-lambda approach matches the `WorkspaceModel` pattern and is safe for multiple engines (one per monitor).

---

## Key Decisions with Rationale

### 1. `QLocalSocket` instead of raw POSIX `socket()`/`connect()`

`QLocalSocket` provides a `QObject`-based wrapper over the platform's local socket, integrating cleanly with Qt's event loop and signal/slot system. It handles cross-platform path encoding and socket descriptor management. Raw POSIX sockets would require manual `fcntl(fd, F_SETFL, O_NONBLOCK)` and manual cleanup, with no signal/slot integration. REQ-C-006 mandates `QLocalSocket`.

### 2. `QSocketNotifier` instead of `QLocalSocket::readyRead` signal

`QLocalSocket::readyRead` is a `QIODevice` signal that fires when the socket's *internal Qt read buffer* has data. If the read buffer fills up between event loop iterations, subsequent bytes may not trigger `readyRead` until the buffer is drained. `QSocketNotifier` wraps the kernel-level `select`/`epoll` notification — it fires whenever the kernel socket buffer has unread data, independent of Qt's internal buffering. For a streaming IPC protocol with frequent small messages, `QSocketNotifier` is more robust. This also explicitly satisfies REQ-C-008 without ambiguity.

### 3. `visible: false` instead of `opacity: 0`

`opacity: 0` makes an item transparent but it still participates in layout — it reserves its `implicitWidth` space in the `RowLayout`, leaving a gap in the center when no title is active. `visible: false` removes the item from layout entirely (Qt's `RowLayout` collapses items with `visible: false`). This satisfies REQ-F-007 (section hidden, no reserved space) and REQ-F-008 (section does not reserve space when hidden). The cost of `visible: false` over `opacity: 0` is that hiding/showing is instantaneous — no fade. Fade animation is an explicit non-goal for this session.

### 4. Manual `QByteArray` line buffer instead of `QTextStream`

`QTextStream` performs codec detection, locale-dependent decimal parsing, and internal buffering that adds overhead inappropriate for a single-line-per-event binary protocol. The manual buffer approach — `buffer_.append(chunk); while ((pos = buffer_.indexOf('\n')) >= 0) { ... buffer_.remove(0, pos + 1); }` — is minimal, predictable, and gives explicit control over partial-read behavior. `buffer_.remove(0, n)` compacts the buffer in-place after each consumed line (O(n) move, negligible for line-sized data). REQ-C-010 (buffer compaction) is trivially satisfied.

### 5. Split on first comma only for `activewindow>>` payload

The Hyprland IPC protocol format is `activewindow>>appClass,title`. Window titles may contain commas (e.g., `"Editing foo, bar — nvim"`). Splitting the payload on `indexOf(',')` (first occurrence) and taking everything after as the title correctly handles commas in titles. `split(',')` with default behavior would truncate at the first comma and discard the rest, producing incorrect titles. The spec's REQ-F-003 acceptance criteria explicitly require this behavior ("split on the first comma only").

### 6. Placement: replace center `Item` spacer, not add beside it

The existing `Item { Layout.fillWidth: true }` spacer was a placeholder for this exact session. Replacing it with `ActiveWindowSection { Layout.fillWidth: true }` gives the section the full center fill behavior. Adding `ActiveWindowSection` beside the spacer would require either removing the spacer (making `ActiveWindowSection` the only fill item) or sizing both — unnecessary complexity. The spacer's entire role is replaced by `ActiveWindowSection`.

---

## Known Risks

### R1: Backfill gap on startup

**Description:** The Hyprland IPC event socket delivers only events that occur *after* the socket connection is established. On `holonight-shell` startup, the service does not know which window is currently active. The title remains empty until the user switches focus to any window. The section stays hidden during this gap.

**Mitigation:** An initial query could be made via `.socket.sock` (the Hyprland IPC command socket) using `{"type":"getactivewindow"}`. This is out of scope for this session (explicitly listed as a non-goal). The empty-title hidden state is correct and acceptable behavior at startup.

### R2: Socket path race on startup

**Description:** If `holonight-shell` starts before the Hyprland event socket is created (e.g., early in a startup script), `QLocalSocket::connectToServer()` will fail with a "file not found" error. The service will remain idle.

**Mitigation:** The service logs a diagnostic on connection failure (`qWarning() << "ActiveWindowService: socket connection failed:" << socket_->errorString()`) and does not crash. Auto-reconnect is out of scope. The typical Hyprland startup order has the shell started from `exec-once` after the compositor is fully initialized, so the race is unlikely in practice.

### R3: Non-Hyprland Wayland environment

**Description:** On GNOME Wayland, KDE Plasma Wayland, or sway, `HYPRLAND_INSTANCE_SIGNATURE` is not set. The service must detect this and skip socket operations.

**Mitigation:** The `connectSocket()` method reads the env var first. If empty, it emits a single `qInfo()` diagnostic and returns. All properties remain at their initialized empty-string values. `ActiveWindowSection` stays hidden. No crash, no hang. The rest of the shell operates normally (layer shell is compositor-independent; workspace pills may show empty state if `ext-workspace-v1` is unsupported too).

### R4: Long lines / malformed socket data

**Description:** If the Hyprland IPC socket sends an extremely long line (e.g., a window title with thousands of characters), the `buffer_` will grow until a `'\n'` is received. This is bounded in practice by Hyprland's own IPC implementation (titles are from X11/Wayland window properties, typically < 256 bytes), but is not formally capped.

**Mitigation:** The 4096-byte read chunk is per-read call, not a hard cap. If a line exceeds 4096 bytes, it will be reassembled across multiple `onSocketReadable()` invocations as the kernel delivers more data. This is correct behavior. A safety cap (e.g., discard lines exceeding 8192 bytes) could be added in a future hardening session if needed.

### R5: `activewindowv2` event vs `activewindow` event

**Description:** Hyprland IPC has both `activewindow>>` and `activewindowv2>>` events. The `activewindow>>` format is `appClass,title`. The `activewindowv2>>` format is `address,class,title` (three fields). Subscribing to `.socket2.sock` delivers both. If `activewindowv2` lines are encountered, the parser ignores them (the prefix `"activewindow>>"` check does not match `"activewindowv2>>"`). This is correct.

**Mitigation:** If a future session requires the window address, `activewindowv2` parsing can be added alongside the existing `activewindow` handling. For this session, `activewindow` is the correct and sufficient event.

### R6: Token name mismatch in `HoloniightPalette`

**Description:** The design uses `HoloniightPalette.textMuted` and `HoloniightPalette.textPrimary`. If the installed `Holonight` QML module exposes these under different names, qmllint will warn and QML will log property-not-found errors at runtime (the property falls back to the default color, likely `#000000` or `transparent`).

**Mitigation:** Run `task qml-lint` after creating `ActiveWindowSection.qml`. Cross-reference failing token names against the design system's color table in `assets/dont-commit/HoloNight-Design-System.md` to identify the correct token name. Fix before committing.

---

## References

- **Spec:** `docs/sdd/topbar-window-title/SPEC.md`
- **Top-bar plan:** `docs/sdd/TOPBAR-PLAN.md`
- **Session 1 design:** `docs/sdd/topbar-skeleton/DESIGN.md`
- **Session 2 design:** `docs/sdd/topbar-workspaces/DESIGN.md`
- **Existing QML module:** `src/qml/Topbar/`
- **Existing C++ singleton pattern:** `src/WorkspaceModel.h`, `src/main.cpp`
- **Build config:** `CMakeLists.txt`, `Taskfile.yml`
- **Design system:** `assets/dont-commit/HoloNight-Design-System.md`
- **Topbar module design ref:** `assets/dont-commit/03-topbar-module.svg`
- **Code style:** `.clang-format`, `.clang-tidy`
- **Project instructions:** `CLAUDE.md`

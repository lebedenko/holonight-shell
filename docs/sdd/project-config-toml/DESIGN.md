# DESIGN: project-config-toml

**Feature:** TOML configuration file support for holonight-shell  
**Status:** Design  
**Date:** 2026-05-28

---

## 1. Component Overview

### New

| Component | Location | Role |
|---|---|---|
| `ConfigService` | `src/core/ConfigService.h/.cpp` | Singleton — owns file I/O, TOML parsing, QFileSystemWatcher, debounce timer, in-memory config structs, signals |

### Modified

| Component | Change |
|---|---|
| `ThemeService` | CONSTANT → NOTIFY on all 8 font properties; reads initial values from `ConfigService`; connects to `appearanceChanged()` |
| `WorkspaceModel` | Gains `setDisplayCount(int)` and `displayCount()` accessor; overflow threshold becomes `displayCount_` instead of literal `6` |
| `WorkspaceSection.qml` | `model: 6` in the `Repeater` becomes `model: WorkspaceModel.displayCount` |
| `TraySection.qml` | `readonly property int maxVisible: 3` becomes bound to `TrayModel.maxVisible`; `TrayModel` gains a `maxVisible` Q_PROPERTY |
| `TrayModel` | Gains `maxVisible` Q_PROPERTY (NOTIFY); reads from `ConfigService` on startup; connects to `barSystemTrayChanged()` |
| `ShellApplication` | Constructs `ConfigService` before all other services; passes pointer to constructors that need it |
| `CMakeLists.txt` | Adds `ConfigService` to `holonight_services`; `tomlplusplus` is already linked to `holonight_core` — move/add link to `holonight_services` |

---

## 2. ConfigService Design

### 2.1 Header Sketch

```cpp
// src/services/ConfigService.h
#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

struct AppearanceConfig {
    QString uiFont       {"Inter"};
    int     uiFontSize   {12};
    QString fixedFont    {"JetBrains Mono"};
    int     fixedFontSize{12};
    QString clockFont    {"Rajdhani"};
    int     clockFontSize{24};
    QString titleFont    {"Audiowide"};
    int     titleFontSize{8};
};

struct BarWorkspacesConfig {
    int count{5};
    static constexpr int kMinCount{3};
    static constexpr int kMaxCount{10};
};

struct BarSystemTrayConfig {
    int maxItems{3};
    static constexpr int kMinMaxItems{2};
    static constexpr int kMaxMaxItems{5};
};

class ConfigService : public QObject {
    Q_OBJECT

public:
    explicit ConfigService(QObject* parent = nullptr);
    ~ConfigService() override = default;

    ConfigService(const ConfigService&)            = delete;
    ConfigService& operator=(const ConfigService&) = delete;
    ConfigService(ConfigService&&)                 = delete;
    ConfigService& operator=(ConfigService&&)      = delete;

    // Static getter for C++ consumers that don't receive an injected pointer.
    // Returns nullptr if ConfigService was never constructed (unit tests etc.).
    static ConfigService* instance();

    [[nodiscard]] const AppearanceConfig&    appearance()    const { return appearance_; }
    [[nodiscard]] const BarWorkspacesConfig& barWorkspaces() const { return bar_workspaces_; }
    [[nodiscard]] const BarSystemTrayConfig& barSystemTray() const { return bar_system_tray_; }

    [[nodiscard]] QString configFilePath() const { return config_path_; }

Q_SIGNALS:
    void appearanceChanged();
    void barWorkspacesChanged();
    void barSystemTrayChanged();

private:
    void resolveConfigPath();
    void ensureDirectoryExists();
    void loadOrCreateConfig();
    void writeDefaultConfig();
    void writeMissingDefaults(const MissingConfigKeys& missing);
    void parseFile();
    void startWatcher();

    void onFileChanged(const QString& path);

    static ConfigService* s_instance_;

    QString              config_path_;
    AppearanceConfig     appearance_;
    BarWorkspacesConfig  bar_workspaces_;
    BarSystemTrayConfig  bar_system_tray_;
    QFileSystemWatcher   watcher_;
    QTimer               debounce_timer_;

    static constexpr int kDebounceMs{200};
};
```

### 2.2 Internal Data Layout

Three plain structs carry all values. Defaults are declared at the struct definition level so they also serve as the source of truth for `writeDefaultConfig()` and for unit-test assertions. Range constants (`kMinCount`, `kMaxCount`, `kMinMaxItems`, `kMaxMaxItems`) live in the structs so that `writeDefaultConfig()` can reference them when writing inline comments.

```
AppearanceConfig     — 4 QString + 4 int
BarWorkspacesConfig  — 1 int
BarSystemTrayConfig  — 1 int
```

The structs are **value types** (not QObject). ConfigService owns one instance of each and exposes them as const-reference getters. On a successful reload the structs are replaced atomically (local parse → swap-on-success pattern described in §2.5).

### 2.3 File Path Resolution

```cpp
void ConfigService::resolveConfigPath() {
    QString xdg = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (xdg.isEmpty()) {
        xdg = QDir::homePath() + QLatin1String("/.config");
    }
    config_path_ = xdg + QLatin1String("/holonight/config.toml");
}
```

`QDir::homePath()` is used instead of `~` expansion — Qt handles it correctly across all environments.

### 2.3.1 Default Config File Content

`writeDefaultConfig()` writes the file as a raw string (not via tomlplusplus serialization, which does not support comments). The written content includes inline comments for range-constrained keys:

```toml
[appearance]
ui_font = "Inter"
ui_font_size = 12
fixed_font = "JetBrains Mono"
fixed_font_size = 12
clock_font = "Rajdhani"
clock_font_size = 24
title_font = "Audiowide"
title_font_size = 8

[bar.workspaces]
count = 5 # accepted: 3-10

[bar.systemtray]
max_items = 3 # accepted: 2-5
```

The same key/comment text is reused when adding newly introduced missing keys to an existing config.

---

### 2.4 Startup Sequence

```
ConfigService::ConfigService()
  1. s_instance_ = this
  2. resolveConfigPath()
  3. ensureDirectoryExists()   — QDir::mkpath; warns on failure, continues
  4. loadOrCreateConfig()
       a. if !QFile::exists(config_path_)  → writeDefaultConfig() → qCInfo
       b. parseFile()  — reads disk, builds local structs, writes missing defaults, swaps into members
  5. startWatcher()            — QFileSystemWatcher::addPath(config_path_)
                                 connect fileChanged → onFileChanged
```

Note: `startWatcher()` is called **after** the initial load so that the initial file write (step 4a) does not trigger a spurious reload.

### 2.5 Live-Reload — Debounce and Atomic Parse

```
onFileChanged(path):
  1. If path != config_path_, ignore
  2. Re-add path to watcher if it exists; otherwise keep the directory watch active
  3. debounce_timer_.start(kDebounceMs)   — restarts if already running

debounce_timer_ timeout:
  4. Call parseFile()

parseFile():
  5. Open file; if open fails → qCWarning, return (keep previous values)
  6. toml::parse(file_stream) in try/catch(toml::parse_error)
  7. Build local AppearanceConfig, BarWorkspacesConfig, BarSystemTrayConfig from parsed table:
       - missing keys use defaults and are recorded for write-back
       - type mismatches use defaults in memory and are logged
       - positive-only integer violations use defaults in memory and are logged
  8. Validate ranges and clamp to boundary on violation:
       - count: clamp to [kMinCount=3, kMaxCount=10]; log qCWarning with out-of-range value
       - maxItems: clamp to [kMinMaxItems=2, kMaxMaxItems=5]; log qCWarning with out-of-range value
  9. If missing keys were detected, append or insert those defaults into the config file.
     Invalid present values are not rewritten only because they are invalid.
 10. Compare local structs against current members:
       - if (local_appearance != appearance_) { appearance_ = local_appearance; emit appearanceChanged(); }
       - if (local_workspaces != bar_workspaces_) { bar_workspaces_ = local_workspaces; emit barWorkspacesChanged(); }
       - if (local_tray != bar_system_tray_) { bar_system_tray_ = local_tray; emit barSystemTrayChanged(); }
 11. qCInfo on success; qCDebug merged values
```

The swap-on-success pattern guarantees that members are never partially updated. If `toml::parse` throws, the catch block logs and returns before any member assignment.

**Editor atomic-write pattern (rename-on-save):** editors like vim and helix write to a temp file and rename it over the original. This causes `QFileSystemWatcher` to drop the path. ConfigService watches both the file and its parent directory, then re-adds the file path when it exists so continuous editing and delete/recreate flows work without restarting.

### 2.6 Error Handling

| Failure Mode | Response |
|---|---|
| `QDir::mkpath` fails | `qCWarning`; proceed without creating file; in-memory defaults used |
| File write fails (`writeDefaultConfig`) | `qCWarning`; in-memory defaults still valid; watcher not started (file doesn't exist) |
| `toml::parse_error` during initial load | `qCWarning` with error description and file path; all defaults used |
| Missing key or section | Default is used in memory; missing default key is written to disk with comment if range-constrained |
| Type mismatch (e.g., `ui_font_size = "abc"`) | Default is used in memory; `qCWarning` with key name and expected type; present invalid value is not rewritten solely for being invalid |
| Positive integer violation (e.g., `clock_font_size = 0`) | Default is used in memory; `qCWarning` with key name |
| Range violation (e.g., count = 1) | Clamped to nearest boundary (3 or 10 for count; 2 or 5 for max_items); `qCWarning` with value and range |
| File unreadable during live-reload | `qCWarning`; members unchanged |
| Parse error during live-reload | `qCWarning`; members unchanged (swap-on-success) |

### 2.7 Singleton Access Pattern

`ConfigService` is constructed in `ShellApplication` and stored as `config_service_` before all other services. It is passed by pointer to services that need it at construction time (`ThemeService`, `TrayModel`). A static `instance()` getter is also provided as a fallback for the few places where pointer injection is not practical, but the injection path is preferred.

```cpp
// ShellApplication construction order:
config_service_  = new ConfigService(this);  // first
theme_           = new ThemeService(config_service_, this);
tray_model_      = new TrayModel(config_service_, this);
manager_         = new ExtWorkspaceManager(model_, config_service_, this);
// ... remaining services unchanged
```

`ConfigService` must never be exposed to QML (no `QML_ELEMENT`, no `Q_INVOKABLE`).

---

## 3. ThemeService Changes

### 3.1 Property Declaration

```cpp
// Before
Q_PROPERTY(QString uiFont READ uiFont CONSTANT)

// After
Q_PROPERTY(QString uiFont READ uiFont NOTIFY uiFontChanged)
```

All 8 properties (`uiFont`, `fixedFont`, `clockFont`, `titleFont`, `uiFontSize`, `fixedFontSize`, `clockFontSize`, `titleFontSize`) get individual NOTIFY signals.

Private member variables replace the inline return literals:

```cpp
private:
    QString ui_font_;
    QString fixed_font_;
    QString clock_font_;
    QString title_font_;
    int     ui_font_size_;
    int     fixed_font_size_;
    int     clock_font_size_;
    int     title_font_size_;
```

### 3.2 Constructor

```cpp
explicit ThemeService(ConfigService* config, QObject* parent = nullptr);

ThemeService::ThemeService(ConfigService* config, QObject* parent)
    : QObject(parent)
{
    applyAppearance(config->appearance());
    connect(config, &ConfigService::appearanceChanged, this, [this, config]() {
        applyAppearance(config->appearance());
    });
}

void ThemeService::applyAppearance(const AppearanceConfig& cfg) {
    if (ui_font_       != cfg.uiFont)        { ui_font_        = cfg.uiFont;        emit uiFontChanged(); }
    if (fixed_font_    != cfg.fixedFont)     { fixed_font_     = cfg.fixedFont;     emit fixedFontChanged(); }
    if (clock_font_    != cfg.clockFont)     { clock_font_     = cfg.clockFont;     emit clockFontChanged(); }
    if (title_font_    != cfg.titleFont)     { title_font_     = cfg.titleFont;     emit titleFontChanged(); }
    if (ui_font_size_  != cfg.uiFontSize)    { ui_font_size_   = cfg.uiFontSize;    emit uiFontSizeChanged(); }
    if (fixed_font_size_ != cfg.fixedFontSize) { fixed_font_size_ = cfg.fixedFontSize; emit fixedFontSizeChanged(); }
    if (clock_font_size_ != cfg.clockFontSize) { clock_font_size_ = cfg.clockFontSize; emit clockFontSizeChanged(); }
    if (title_font_size_ != cfg.titleFontSize) { title_font_size_ = cfg.titleFontSize; emit titleFontSizeChanged(); }
}
```

QML consumers (`ThemeService.uiFont`, `ThemeService.clockFontSize`, etc.) require no changes. Bindings using these properties will re-evaluate automatically when the NOTIFY signals fire.

### 3.3 QML_SINGLETON Registration

`ThemeService` currently uses `qmlRegisterSingletonType` in `ShellApplication::registerQmlTypes()`. This is unchanged. The NOTIFY signals make the properties live-bindable.

---

## 4. TrayModel Changes

### 4.1 New Property and Member

```cpp
// TrayModel.h — add:
Q_PROPERTY(int maxVisible READ maxVisible NOTIFY maxVisibleChanged)

int maxVisible() const { return max_visible_; }

Q_SIGNALS:
    void maxVisibleChanged();   // (existing signals kept)

private:
    int max_visible_{3};        // default matches BarSystemTrayConfig::maxItems default; range 2–5 enforced in ConfigService
```

### 4.2 Constructor

```cpp
explicit TrayModel(ConfigService* config, QObject* parent = nullptr);

TrayModel::TrayModel(ConfigService* config, QObject* parent)
    : QAbstractListModel(parent),
      max_visible_(config->barSystemTray().maxItems)
{
    connect(config, &ConfigService::barSystemTrayChanged, this, [this, config]() {
        const int updated = config->barSystemTray().maxItems;
        if (max_visible_ != updated) {
            max_visible_ = updated;
            emit maxVisibleChanged();
        }
    });
}
```

### 4.3 TraySection.qml Change

```qml
// Before
readonly property int maxVisible: 3

// After
readonly property int maxVisible: TrayModel.maxVisible
```

The `slotItems` and `overflowCount` derived properties already reference `root.maxVisible`, so all overflow logic propagates automatically. No other QML changes are needed.

---

## 5. Workspace Count Changes

### 5.1 Where the Count Lives

The workspace display count is split across two files:

- `src/qml/Topbar/WorkspaceSection.qml` line 99: `model: 6` — controls how many `WorkspacePill` items the `Repeater` creates
- `src/core/WorkspaceModel.cpp` — overflow logic uses literal `6` as the threshold: `entry.id > 6`

Both must be parameterised together. The authoritative count is owned by `WorkspaceModel` because it is the C++ model that calculates overflow; QML reads it from there.

### 5.2 WorkspaceModel Changes

```cpp
// WorkspaceModel.h — add:
Q_PROPERTY(int displayCount READ displayCount NOTIFY displayCountChanged)

void setDisplayCount(int count);
[[nodiscard]] int displayCount() const { return display_count_; }

Q_SIGNALS:
    void displayCountChanged();   // (existing signals kept)

private:
    int display_count_{5};        // default matches BarWorkspacesConfig::count default; range 3–10 enforced in ConfigService
```

The literal `6` in overflow methods (`entry.id > 6`) becomes `entry.id > display_count_` (using the non-const member directly; the methods are already const so they read `display_count_`).

```cpp
void WorkspaceModel::setDisplayCount(int count) {
    if (display_count_ == count) return;
    display_count_ = count;
    ++revision_;
    emit revisionChanged();
    emit displayCountChanged();
}
```

Emitting `revisionChanged()` causes QML to re-evaluate all `WorkspaceModel.revision`-dependent expressions, including the Repeater model and the overflow pills.

### 5.3 ExtWorkspaceManager Wires the Signal

`ExtWorkspaceManager` already holds a pointer to `WorkspaceModel`. It receives `ConfigService*` at construction and sets the initial count:

```cpp
ExtWorkspaceManager::ExtWorkspaceManager(WorkspaceModel* model, ConfigService* config, QObject* parent)
    : ..., model_(model)
{
    model_->setDisplayCount(config->barWorkspaces().count);
    connect(config, &ConfigService::barWorkspacesChanged, this, [this, config]() {
        model_->setDisplayCount(config->barWorkspaces().count);
    });
    // ... existing Wayland init unchanged
}
```

### 5.4 WorkspaceSection.qml Change

```qml
// Before
Repeater {
    model: 6
    ...
}

// After
Repeater {
    model: WorkspaceModel.displayCount
    ...
}
```

The SPEC default count is 5. The current QML uses 6. After the change, default behavior produces 5 pills matching the SPEC. This is a single-pill visual difference from the current code; it is intentional per REQ-C-005 (defaults match ThemeService/TrayModel current hardcoded values — the workspace default was declared as 5 in REQ-F-006).

---

## 6. Data Flow Diagram

```
┌─────────────┐  startup: resolveConfigPath()
│             │  ensureDirectoryExists()
│             │  loadOrCreateConfig()
│ ConfigService│◄──── ~/.config/holonight/config.toml
│             │
│  appearance_│────────────────────────────────────────┐
│  bar_ws_    │────────────────────────────┐           │
│  bar_tray_  │──────────────┐            │           │
└──────┬──────┘              │            │           │
       │                     │            │           │
  QFileSystemWatcher         ▼            ▼           ▼
  ──────────────────  TrayModel     WorkspaceModel  ThemeService
  file changed event  .max_visible_  .display_count_  .ui_font_
       │              (Q_PROPERTY)   (Q_PROPERTY)      .clock_font_size_
  200ms QTimer        NOTIFY:        NOTIFY:            NOTIFY signals
  debounce            maxVisibleChg  displayCountChg    ──────────────►
       │                             + revisionChg       QML bindings
  parseFile()                        │                   ThemeService.uiFont
       │                             │                   ThemeService.clockFontSize
  emit appearanceChanged()     WorkspaceSection.qml      etc.
  emit barWorkspacesChanged()  model: WorkspaceModel.displayCount
  emit barSystemTrayChanged()
       │
       └─► TraySection.qml
           maxVisible: TrayModel.maxVisible
```

---

## 7. CMakeLists.txt Changes

### 7.1 tomlplusplus Linking

`tomlplusplus` is already declared and linked in `CMakeLists.txt`:

```cmake
pkg_check_modules(TOMLPLUSPLUS REQUIRED tomlplusplus)
```

It is currently linked to `holonight_core` only:

```cmake
target_link_libraries(holonight_core PUBLIC
    ...
    ${TOMLPLUSPLUS_LIBRARIES}
)
target_compile_options(holonight_core PUBLIC
    ${TOMLPLUSPLUS_CFLAGS_OTHER}
)
target_include_directories(holonight_core PUBLIC
    ...
    ${TOMLPLUSPLUS_INCLUDE_DIRS}
)
```

`holonight_services` links `holonight_core` via `PUBLIC`, so `TOMLPLUSPLUS_INCLUDE_DIRS` and compile flags already propagate transitively. No additional `pkg_check_modules` call is needed. The only required change is adding `ConfigService` sources to `holonight_services`.

### 7.2 Add ConfigService to holonight_services

```cmake
add_library(holonight_services STATIC
    src/services/ConfigService.h         # add
    src/services/ConfigService.cpp       # add
    src/services/ActiveWindowService.h
    src/services/ActiveWindowService.cpp
    # ... existing entries unchanged ...
)
```

No new `target_link_libraries` or `target_include_directories` entries are needed for `ConfigService` specifically — it uses only Qt Core and tomlplusplus, both of which are already available to `holonight_services` through the existing dependency chain.

### 7.3 ShellApplication Wiring (holonight_app)

`ShellApplication.h/.cpp` gain a `ConfigService*` member. No CMake changes are needed for `holonight_app` because it already links all four libs.

---

## 8. Key Decisions with Rationale

### Why not QSettings?

`QSettings` uses an INI-style format that does not support nested sections (`[bar.workspaces]` vs `[bar.systemtray]`). It also uses a Qt-specific binary registry backend on some platforms. TOML is a well-specified format with clear semantics for tables, is human-readable, and is already listed in the constraints (REQ-C-001).

### Why per-section signals rather than a single `configChanged()`?

Each consumer only cares about one section. `ThemeService` must not re-render fonts when only `max_items` changed. Per-section signals give consumers the ability to connect to exactly what they need, avoiding spurious updates and making the signal topology legible. With only 3 sections the cost is negligible.

### Why 200ms debounce?

Editors (vim, helix, emacs) commonly write files in multiple steps or via atomic rename. A 200ms window absorbs typical multi-write patterns from a single save event without being long enough to feel unresponsive to the user. The interval is named `kDebounceMs` and can be adjusted without rebuilding consumers.

### Why C++-only (no QML exposure)?

Config values flow into QML exclusively via existing QML-visible singletons (`ThemeService`, `WorkspaceModel`, `TrayModel`). Exposing `ConfigService` to QML would create a second writable pathway that complicates reasoning about which values are authoritative. REQ-C-002 explicitly prohibits QML exposure in this iteration.

### Why constructor injection rather than a global singleton everywhere?

Constructor injection makes the dependency explicit and testable — unit tests can construct a `ThemeService` with a stub `ConfigService` without relying on global state. The static `instance()` getter exists only as a convenience fallback for code paths where injection would require propagating the pointer through many layers. In practice only `ShellApplication`-constructed services use injection; the static getter is not used in production code.

### Why is WorkspaceModel the owner of displayCount rather than WorkspaceSection.qml?

The overflow threshold (`entry.id > N`) lives in `WorkspaceModel::overflowWorkspaceId()` and related C++ methods. If `displayCount` were only a QML property, the C++ overflow logic would still use a hardcoded literal. Putting `displayCount` in `WorkspaceModel` keeps the single source of truth in C++ and avoids a situation where the Repeater model and the overflow threshold diverge.

---

## 9. Alternatives Considered

### JSON

Qt includes `QJsonDocument` with no extra dependency. Rejected because JSON lacks comments (users cannot annotate their config), requires more verbose syntax for nested sections, and provides no first-class differentiation between integers and floats. TOML is better suited to hand-edited config files.

### INI / QSettings

Handled in §8. Rejected due to lack of nested table support matching the `[bar.workspaces]` / `[bar.systemtray]` structure.

### YAML

Well-supported by the community but no system package for C++ YAML parsing is as header-only and lightweight as tomlplusplus. YAML's indentation-sensitive syntax is also more fragile for hand editing. The SPEC mandates TOML explicitly.

### Watching the directory instead of the file

`QFileSystemWatcher` can watch a directory and detect renames into it. This would handle the atomic-write case without re-adding the path. Rejected because directory watching fires on every file created in `~/.config/holonight/`, not just `config.toml`. The re-add-on-signal pattern (§2.5) is simpler and well-established in Qt applications.

---

## 10. Known Risks

### QFileSystemWatcher and atomic writes

**Risk:** Editors that write via rename-on-save (vim `:w`, helix, most editors using `O_TMPFILE`) cause `QFileSystemWatcher` to emit `fileChanged` with the old path removed from the watch list. Subsequent saves are silently ignored.

**Mitigation:** `onFileChanged` unconditionally calls `watcher_.addPath(config_path_)` before starting the debounce timer. This is the standard fix documented in Qt's own examples. See §2.5.

### Initialization order between ConfigService and ThemeService

**Risk:** If `ThemeService` is constructed before `ConfigService` (e.g., wrong order in `ShellApplication`), the constructor read of `config->appearance()` would be a null-pointer dereference.

**Mitigation:** `ShellApplication` constructs `ConfigService` first. `ConfigService::ConfigService()` completes the full startup sequence (load + defaults) before returning, so by the time `ThemeService` is constructed, `appearance()` is populated. A static assert or `Q_ASSERT(config != nullptr)` in each consumer's constructor catches misuse during development.

### Config file written during reload window

**Risk:** If the user saves the config file while a reload is already in flight (between `fileChanged` emission and the end of `parseFile()`), a second `fileChanged` arrives. The debounce timer resets, causing a second reload after 200ms.

**Mitigation:** This is the correct and intended behavior — the second reload picks up the latest state. No special handling is needed.

### First-run default file write race

**Risk:** If two instances of holonight-shell start simultaneously on the same user account (unlikely but possible), both may try to create `config.toml` at the same time.

**Mitigation:** `QFile::open(QIODevice::WriteOnly | QIODevice::NewOnly)` fails atomically on the second writer. The second instance logs a warning and proceeds with in-memory defaults. On the next startup both instances will find the file created by the first.

### tomlplusplus header-only parse on large files

**Risk:** tomlplusplus parses the entire file eagerly on each reload. Config files are tiny (<1 KB) so this is not a practical concern, but it is worth noting that there is no streaming or lazy evaluation.

**Mitigation:** None needed. For a file of this size, full eager parse completes in well under 1ms (REQ-NF-005 threshold is 50ms).

### WorkspaceModel displayCount was previously hardcoded at 6

**Note (resolved):** The previous hardcoded value in `WorkspaceSection.qml` was `model: 6`. The default config value is 5. This is intentional — the hardcoded value was arbitrary; 5 is the chosen user-configurable default. Users who want 6 can set `count = 6` in their config. No action needed.

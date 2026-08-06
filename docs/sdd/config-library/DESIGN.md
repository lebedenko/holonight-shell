# holonight_config – Design Document

**Feature:** Static library extraction of config structs, parsing, and atomic writing.
**Spec:** `docs/sdd/config-library/SPEC.md`
**Status:** Design (pre-implementation)

---

## 1. Component Map

```
┌─────────────────────────────────────────────────────────────┐
│  holonight_config  (new static lib, no QObject/watcher)     │
│                                                             │
│  include/holonight_config/                                  │
│    config_structs.h   — all structs, enums, free fns        │
│    config_parsers.h   — MissingDefaults, ParsedConfig,      │
│                         parseConfigTable(), tomlQuote(),    │
│                         writeMissingDefaults()              │
│    config_writer.h    — ConfigWriter::write()               │
│                                                             │
│  src/core/                                                  │
│    ConfigParsers.cpp  — implements parsers + validators     │
│    ConfigWriter.cpp   — implements writer                   │
│                                                             │
│  deps: Qt6::Core, toml++ (PUBLIC — header exposes toml::)  │
└──────────────────────────────┬──────────────────────────────┘
                               │ link + include
             ┌─────────────────▼────────────────────┐
             │  holonight_services                   │
             │    ConfigService  (QObject wrapper)   │
             │      – resolveConfigPath()            │
             │      – QFileSystemWatcher + debounce  │
             │      – applyParsedConfig() + signals  │
             │      – delegates parsing/writing to   │
             │        holonight_config functions     │
             └───────────────┬──────────────────────┘
                             │ link
             ┌───────────────▼──────────────────────┐
             │  holonight-shell (executable)         │
             │  holonight-settings (future binary)   │
             │    – call ConfigWriter::write()        │
             │      directly for full rewrite        │
             └──────────────────────────────────────┘
```

**Responsibility split:**

| Concern | Owner |
|---|---|
| Struct definitions & defaults | `holonight_config` |
| TOML parsing & validation | `holonight_config` |
| Atomic TOML writing | `holonight_config` |
| XDG path resolution | `ConfigService` |
| File watching & debounce | `ConfigService` |
| Change detection & signals | `ConfigService` |
| QML singleton registration | `ConfigService` |

---

## 2. Public Header Layout

```
include/
└── holonight_config/
    ├── config_structs.h
    ├── config_parsers.h
    └── config_writer.h
```

### `config_structs.h`

Dependencies: `<QString>`, `<QStringList>`, `<QList>`, `<QDateTime>`, `<cstdint>`, `<optional>` — no QObject, no toml++.

Declares (in order):
- `struct AppearanceConfig`
- `struct ThemeConfig`
- `struct BarWorkspacesConfig`
- `struct BarSystemTrayConfig`
- `struct TrayIconOverrideConfig`
- `struct TrayIconOverridesConfig`
- `struct BackgroundConfig` (+ `static QString imageForMonitor(...)`)
- `struct NotificationsConfig`
- `struct NotificationHistoryConfig`
- `enum class WeekStartDay : std::uint8_t`
- `struct CalendarCaldavAccountConfig`
- `struct CalendarIcsAccountConfig`
- `struct CalendarConfig`
- `struct WeatherConfig`
- `enum class WidgetPosition : std::uint8_t`
- `[[nodiscard]] std::optional<WidgetPosition> widgetPositionFromString(const QString&)`
- `[[nodiscard]] QString widgetPositionToString(WidgetPosition)`
- `[[nodiscard]] bool widgetPositionIsTopAnchored(WidgetPosition)`
- `enum class WidgetType : std::uint8_t`
- `struct TimeToEventConfig`
- `struct ClockConfig`
- `struct WidgetDefinition`
- `struct WidgetsConfig`

All structs carry in-class member initializers and `bool operator==(const T&) const = default;`.

### `config_parsers.h`

Dependencies: `<holonight_config/config_structs.h>`, `<toml++/toml.h>`, `<QString>` — no QObject.

```cpp
struct MissingDefaults {
  // one bool per top-level field that can be absent
  bool ui_font{false};
  // ... (all fields from current ConfigParsers.h)
  [[nodiscard]] bool any() const;
};

struct ParsedConfig {
  AppearanceConfig       appearance;
  ThemeConfig            theme;
  BarWorkspacesConfig    bar_workspaces;
  BarSystemTrayConfig    bar_system_tray;
  TrayIconOverridesConfig tray_icon_overrides;
  BackgroundConfig       background;
  WeatherConfig          weather;
  NotificationsConfig    notifications;
  NotificationHistoryConfig notification_history;
  WidgetsConfig          widgets;
  CalendarConfig         calendar;
};

[[nodiscard]] QString tomlQuote(const QString& value);
[[nodiscard]] ParsedConfig parseConfigTable(const toml::table& table, MissingDefaults& missing);
bool writeMissingDefaults(const QString& path, const MissingDefaults& missing);
```

**Note on toml++ visibility:** `parseConfigTable` takes a `const toml::table&`, so `config_parsers.h` must include `<toml++/toml.h>`. This means callers who include `config_parsers.h` also pull in toml++ headers. The CMake target therefore links toml++ as **PUBLIC** (not PRIVATE) — see §4.

### `config_writer.h`

Dependencies: `<holonight_config/config_parsers.h>`, `<QString>` — no QObject.

```cpp
class ConfigWriter {
 public:
  // Atomically serializes config to a TOML file at path.
  // Creates parent directories if needed. Returns false + qCWarning on failure.
  [[nodiscard]] static bool write(const ParsedConfig& config, const QString& path);

 private:
  ConfigWriter() = delete;
};
```

---

## 3. Source File Layout

```
src/core/
├── ConfigService.h          ← stripped: remove all structs; add includes below
├── ConfigService.cpp        ← minimal changes; delegates to holonight_config
├── ConfigParsers.h          ← DELETED (declarations move to include/holonight_config/)
├── ConfigParsers.cpp        ← KEPT; include path updated to new public headers
└── ConfigWriter.cpp         ← NEW; implements ConfigWriter::write()
```

### Changes per file

**`src/core/ConfigService.h`** — remove all struct/enum declarations; replace with:
```cpp
#include <holonight_config/config_structs.h>
#include <holonight_config/config_parsers.h>
#include <holonight_config/config_writer.h>
// QObject, QFileSystemWatcher, QTimer includes remain
// Forward declaration of ParsedConfig removed (now provided by config_parsers.h)
```

**`src/core/ConfigParsers.h`** — deleted. Consumers updated to include `<holonight_config/config_parsers.h>`.

**`src/core/ConfigParsers.cpp`** — include directive changes:
```cpp
// Before: #include "ConfigParsers.h"
// After:
#include <holonight_config/config_parsers.h>
#include <holonight_config/config_structs.h>
// Keep: #include <toml++/toml.h> (for implementation use)
// Add logging category: Q_LOGGING_CATEGORY(lcConfig, "holonight.config")
```

**`src/core/ConfigWriter.cpp`** — new file; see §5.

**`src/core/ConfigService.cpp`** — two targeted changes; see §7.

---

## 4. CMake Structure

### New `holonight_config` target

Add before the existing `holonight_services` target in `CMakeLists.txt`:

```cmake
add_library(holonight_config STATIC
  src/core/ConfigParsers.cpp
  src/core/ConfigWriter.cpp
)

target_include_directories(holonight_config
  PUBLIC  include/          # exposes include/holonight_config/*.h
  PRIVATE src/core/         # internal use only (if any private headers)
)

# toml++ is PUBLIC because config_parsers.h exposes toml::table in its signature.
target_link_libraries(holonight_config
  PUBLIC  Qt6::Core
  PUBLIC  TOMLPLUSPLUS::TOMLPLUSPLUS
)
```

### Updated `holonight_services` target

```cmake
target_link_libraries(holonight_services
  PUBLIC  holonight_config   # add this line
  # ... existing deps unchanged
)
```

`holonight_config` propagates `Qt6::Core` and `TOMLPLUSPLUS::TOMLPLUSPLUS` transitively, so `holonight_services` and `holonight-shell` do not need separate toml++ links.

### `holonight-settings` (future)

```cmake
target_link_libraries(holonight-settings
  PRIVATE holonight_config
)
```

No additional include path setup needed — `target_include_directories(PUBLIC include/)` on the library handles it.

---

## 5. ConfigWriter Design

**File:** `src/core/ConfigWriter.cpp`

```cpp
#include <holonight_config/config_writer.h>
#include <holonight_config/config_parsers.h>   // tomlQuote
#include <QDir>
#include <QSaveFile>
#include <QTextStream>

Q_LOGGING_CATEGORY(lcConfigWriter, "holonight.config.writer")

bool ConfigWriter::write(const ParsedConfig& config, const QString& path) {
  QDir dir = QFileInfo(path).absoluteDir();
  if (!dir.mkpath(".")) {
    qCWarning(lcConfigWriter) << "Cannot create config directory:" << dir.absolutePath();
    return false;
  }

  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qCWarning(lcConfigWriter) << "Cannot open for writing:" << path << file.errorString();
    return false;
  }

  QTextStream out(&file);
  // Write sections in canonical order (appearance → theme → bar → background →
  // weather → notifications → widgets → calendar)
  // All string values go through tomlQuote().
  // std::optional<double> lat/lon written only when present.
  // [[calendar.caldav_accounts]] and [[calendar.ics_accounts]] as array-of-tables.

  if (!file.commit()) {
    qCWarning(lcConfigWriter) << "Commit failed for:" << path << file.errorString();
    return false;
  }
  return true;
}
```

**Section order in output file:**

```
[appearance]
[theme]
[bar.workspaces]
[bar.system_tray]
[bar.tray_icon_overrides.<name>]   (one section per override)
[background]
[weather]
[notifications]
[notification_history]
[widgets]
[[widgets.definitions]]            (array-of-tables, one per WidgetDefinition)
[calendar]
[[calendar.caldav_accounts]]       (array-of-tables)
[[calendar.ics_accounts]]          (array-of-tables)
```

**Key rules:**
- Every string scalar → `tomlQuote()`
- `std::optional<double>` lat/lon: written only when `has_value()`; never written when absent
- `bool` → `"true"` / `"false"` (unquoted TOML booleans)
- `int` → unquoted integer literal
- `QStringList` → `["a", "b"]` inline array; empty list → `[]`
- On any `QTextStream` write failure (checked via `out.status()`): log + return false before commit

---

## 6. Validation Architecture

All validators are **private static lambdas or file-scope functions** inside `src/core/ConfigParsers.cpp`. They have no public API.

```cpp
// Reads an integer from the TOML table, clamping to [min_val, max_val].
// Logs qCWarning on missing key or out-of-range value.
static int readInt(const toml::table& tbl, std::string_view key,
                   int min_val, int max_val, int default_val);

// Reads a string; rejects empty/whitespace-only, returns default_val with warning.
static QString readStr(const toml::table& tbl, std::string_view key,
                       const QString& default_val, bool allow_empty = false);

// Reads a string and validates it is in valid_values. Returns default_val with warning
// if unknown.
static QString readEnum(const toml::table& tbl, std::string_view key,
                        const QStringList& valid_values, const QString& default_val);

// Reads a bool; returns default_val if key absent or wrong type.
static bool readBool(const toml::table& tbl, std::string_view key, bool default_val);

// Reads an optional double (lat/lon). Returns std::nullopt if key absent.
static std::optional<double> readOptDouble(const toml::table& tbl, std::string_view key);
```

Warning format: `"Config: <key> '<value>' <reason>; using default '<default>'"`.

**Constraint bounds** (match current `static constexpr` values in structs):
- `bar.workspaces.count` ∈ [3, 10]
- `bar.system_tray.max_items` ∈ [2, 5]
- `notifications.max_visible` ∈ [1, 10]

---

## 7. ConfigService Delegation Pattern

### `writeConfig()` — before → after

**Before** (inline `QTextStream` serialization in `ConfigService.cpp`):
```cpp
void ConfigService::writeConfig() {
  QFile file(config_path_);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { ... }
  QTextStream out(&file);
  out << "[appearance]\n";
  out << "ui_font = " << tomlQuote(appearance_.ui_font) << "\n";
  // ... hundreds of lines ...
}
```

**After** (one-liner delegation):
```cpp
void ConfigService::writeConfig() {
  ParsedConfig current = buildParsedConfig();   // assembles from member fields
  if (!ConfigWriter::write(current, config_path_)) {
    qCWarning(lcConfigService) << "Failed to write config to" << config_path_;
  }
}
```

`buildParsedConfig()` is a private helper that constructs a `ParsedConfig` from `ConfigService`'s member fields (`appearance_`, `theme_`, etc.). It stays in `ConfigService.cpp`.

Alternatively, if `writeConfig()` is only ever called at startup to create a default file, it can be replaced with:
```cpp
ConfigWriter::write(ParsedConfig{}, config_path_);  // all defaults
```
Audit before choosing: verify `writeConfig()` is never called after `applyParsedConfig()` applies user values.

### `parseFile()` — minimal change

`parseFile()` already calls `parseConfigTable()`. The only change is the include:
- Remove `#include "ConfigParsers.h"`
- Add `#include <holonight_config/config_parsers.h>`

The TOML parse error catch (`try { toml::parse() } catch (...)`) stays in `ConfigService::parseFile()` per REQ-F-026 — the library receives an already-parsed `toml::table&`.

### `ConfigService` private member changes

`writeConfig()` is removed as an independent serialization body; `buildParsedConfig()` is added as a private helper if needed. The `loadOrCreateConfig()` call site is updated accordingly.

---

## 8. Key Decisions with Rationale

| Decision | Rationale |
|---|---|
| **Static library, not object library** | Object libraries don't transitively propagate `target_include_directories` to link consumers, so downstream targets would need manual include path setup. Static library avoids this. |
| **Static library, not shared** | No runtime loader overhead; no symbol visibility management (`__declspec(dllexport)` / `-fvisibility`); no SONAME versioning. Single-process build — shared offers no benefit. |
| **Headers under `include/holonight_config/`** | Angle-bracket includes (`#include <holonight_config/config_structs.h>`) work regardless of where the caller's translation unit lives. Avoids relative path ambiguity. Future `holonight-settings` can link against the installed target without knowing project internals. |
| **`.cpp` stays in `src/core/`** | REQ-C-003; minimizes diff. Moving `.cpp` to a new directory would require touching every include path and CMake glob. Deferred to a later cycle if directory structure is revisited. |
| **`ConfigService` retains path resolution** | Library is path-agnostic (REQ-NF-005). XDG logic (`$XDG_CONFIG_HOME`, `QStandardPaths`) is application-layer concern; it changes between binaries (shell vs. settings app). |
| **`ConfigWriter` is a class (static method), not a free function** | Provides a named extension point for future injection (e.g., mock writer in tests). Consistent with existing codebase pattern of wrapping stateless utilities in classes. |
| **toml++ linked PUBLIC on `holonight_config`** | `config_parsers.h` exposes `const toml::table&` in `parseConfigTable`'s signature. Any translation unit including `config_parsers.h` must see toml++ headers. PRIVATE would hide it from the CMake propagation but not from the actual compiler, causing confusing ODR errors. PUBLIC is honest. |
| **No `ConfigService` factory in the library** | `ConfigService` is a `QObject` singleton. Moving it into the library would add a QObject dependency, violating REQ-NF-001. |
| **`MissingDefaults` in `config_parsers.h`, not `config_structs.h`** | It is a parsing artifact, not a config model type. Keeping it alongside its consumer (`parseConfigTable`) and producer (`writeMissingDefaults`) makes the separation clearer. |

---

## 9. Alternatives Considered

- **Object library instead of static**: rejected — `OBJECT` libraries do not transitively propagate `target_include_directories` via CMake; every consumer would need a manual `target_include_directories` call.
- **Header-only library**: rejected — `Q_LOGGING_CATEGORY` expands to a variable definition, requiring a `.cpp` TU. Validation helpers are non-trivial and would bloat every translation unit if header-only.
- **Moving `.cpp` to `src/holonight_config/`**: considered; deferred. REQ-C-003 says stay in `src/core/` for this cycle. Moving adds CMake churn and increases diff scope without functional benefit now.
- **Using toml++'s serializer for `ConfigWriter`**: toml++ provides `operator<<` for `toml::table`, but round-tripping a `ParsedConfig` through a `toml::table` construction adds indirection and loses our canonical ordering and comment placement. Manual `QTextStream` is more controllable.
- **Separate logging categories per file**: considered `lcConfigParser` + `lcConfigWriter`. Rejected for now — single `lcConfig` category keeps filtering simple; split if log volume warrants it.

---

## 10. Risks

### Include cycle — `ConfigParsers.h` currently includes `ConfigService.h`

**Current state:** `ConfigParsers.h` does `#include "ConfigService.h"` (to access structs). After extraction this dependency reverses: `config_parsers.h` includes `config_structs.h`, not `ConfigService.h`.

**Risk:** Any TU that currently includes `ConfigParsers.h` expecting to get `ConfigService.h` transitively will break.

**Mitigation:** Grep for all `#include "ConfigParsers.h"` and `#include "ConfigService.h"` before starting. Update each to the new public header. CI catches compile failures immediately.

### Forward declaration residue

Any file that forward-declares `struct AppearanceConfig` (or other config structs) to avoid a full include will fail once the struct moves headers — the forward declaration in `ConfigService.h` scope becomes stale.

**Mitigation:** `grep -r "struct AppearanceConfig\|struct ThemeConfig\|struct BarWorkspaces" src/` before starting. Fix all forward declarations to include the new header.

### toml++ PUBLIC propagation may surprise future consumers

Any binary that links `holonight_config` (e.g., `holonight-settings`) gets toml++ headers on its include path, even if it never calls `parseConfigTable`. This is harmless but non-obvious.

**Mitigation:** Document in the library's CMakeLists.txt comment that toml++ is PUBLIC due to the public header signature. If a future consumer wants to avoid the dependency, introduce an opaque `ParseTableResult` type and move `toml::table` behind an implementation detail.

### `writeConfig()` caller audit

`ConfigService::writeConfig()` is currently called from `loadOrCreateConfig()` to create a fresh default file. Verify it has no other callers before removal:
```bash
grep -n "writeConfig" src/core/ConfigService.cpp
```
If there is exactly one call site, removal is safe. If multiple — the delegation pattern in §7 applies to each.

### Struct equality (`operator==`) with `QList<T>` members

`CalendarConfig` and `TrayIconOverridesConfig` contain `QList<T>` fields. `= default` equality on those structs recursively calls `operator==` on list elements. This works in C++20 with `= default` only if `T` itself has `operator==`. All nested structs already carry `= default` — confirm this holds for `CalendarCaldavAccountConfig`, `CalendarIcsAccountConfig`, `TrayIconOverrideConfig`, `WidgetDefinition` before removing any explicit definitions.

### Ninja clock-skew on partial reconfigure

Deleting only `build/CMakeCache.txt` to pick up new `add_library` targets triggers Ninja's "manifest still dirty" loop. After adding `holonight_config` to CMakeLists.txt, do a full `rm -rf build && task configure` (not a cache-only delete).

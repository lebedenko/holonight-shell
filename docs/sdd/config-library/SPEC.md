# holonight_config – Requirements Specification

**Feature:** A C++23 static library providing config struct definitions, TOML parsing, and atomic file writing for holonight configuration (`$XDG_CONFIG_HOME/holonight/config.toml`).

**Status:** SDD Session 1

**Baseline:** Existing `ConfigService.h`, `ConfigParsers.h`, and `ConfigParsers.cpp` in `src/core/`; new static library target `holonight_config` extracted from `holonight_services`.

---

## Overview

The `holonight_config` library encapsulates all configuration reading, writing, and validation logic in a linkable, non-Qt (no QObject, no QFileSystemWatcher, no signals) static library. This enables code reuse between the shell (`holonight-shell`) and future binaries (e.g., `holonight-settings`) without repeating config parsing or writer logic. The library depends on Qt Core (QString, QList, QDir, QSaveFile) and toml++ (header-only parser) but contains no scene-graph or platform-specific code.

---

## Functional Requirements

### REQ-F-001: CMake static library target definition

**Statement:** The build system shall define a CMake target `holonight_config` as a static library (`STATIC`, not `OBJECT` or `SHARED`) that links against `Qt6::Core` and toml++.

**Acceptance Criterion:**
- `cmake -B build && cmake --build build` completes without errors
- `libholonight_config.a` (or `.lib` on Windows) is present in the build artifacts
- The target is exported in CMakeLists.txt with proper PUBLIC includes for consumption by other targets
- `holonight_services` and the main `holonight-shell` executable link against `holonight_config` successfully

---

### REQ-F-002: Config struct declarations in holonight_config

**Statement:** The library shall declare all config structs currently in `ConfigService.h` as public types: `AppearanceConfig`, `ThemeConfig`, `BarWorkspacesConfig`, `BarSystemTrayConfig`, `TrayIconOverrideConfig`, `TrayIconOverridesConfig`, `BackgroundConfig`, `WeatherConfig`, `NotificationsConfig`, `NotificationHistoryConfig`, `WidgetsConfig`, `CalendarConfig`, `WidgetDefinition`, `TimeToEventConfig`, `ClockConfig`, plus enums `WidgetPosition`, `WidgetType`, and `WeekStartDay`.

**Acceptance Criterion:**
- A public header `holonight_config/config_structs.h` declares all structs with member defaults matching the current implementation
- Each struct has `operator==()` defined (either `= default` or explicit) for comparison
- The header includes only Qt Core headers and `<cstdint>`, `<optional>`, not QObject or platform headers
- Callers can `#include <holonight_config/config_structs.h>` and instantiate structs without QObject

---

### REQ-F-003: ParsedConfig aggregate struct

**Statement:** The library shall provide a `ParsedConfig` struct that aggregates all config section structs (one field per section) and is populated by the parser function.

**Acceptance Criterion:**
- `ParsedConfig` declares fields: `appearance`, `theme`, `bar_workspaces`, `bar_system_tray`, `tray_icon_overrides`, `background`, `weather`, `notifications`, `notification_history`, `widgets`, `calendar`
- Each field type matches the corresponding struct (e.g., `AppearanceConfig appearance`)
- The struct is default-constructible (all fields initialize to their struct defaults)

---

### REQ-F-004: MissingDefaults tracking struct

**Statement:** The library shall provide a `MissingDefaults` struct that tracks which top-level config fields were absent from the TOML file (and thus need default write-back).

**Acceptance Criterion:**
- `MissingDefaults` declares a boolean field for each top-level key that can be missing (e.g., `ui_font`, `workspace_count`, `weather_api_key`)
- A member function `bool any() const` returns true if any field is true
- The struct is populated during parsing and passed to `writeMissingDefaults()`

---

### REQ-F-005: parseConfigTable() function

**Statement:** The library shall provide a function `ParsedConfig parseConfigTable(const toml::table& table, MissingDefaults& missing)` that parses a toml++ table and returns a `ParsedConfig` with all valid values or defaults, and updates the `missing` tracker.

**Acceptance Criterion:**
- On a valid TOML table, the function returns a `ParsedConfig` with all fields populated
- For any missing top-level key, the corresponding struct uses its default member values and `missing.<key>` is set to true
- For any malformed value (wrong type, out-of-range), the function logs a `qCWarning` and uses the default value
- The function does not throw exceptions; all error handling is via logging and fallback defaults
- Floating-point geolocation fields (`weather.latitude`, `weather.longitude`) remain `std::optional` and are not written back when absent

---

### REQ-F-006: Per-field validators during parsing

**Statement:** The parser shall apply validators to integer and string fields during `parseConfigTable()`, clamping out-of-range integers and rejecting structurally invalid strings, with warnings logged for each violation.

**Acceptance Criterion:**
- Integer fields with declared min/max bounds (e.g., `bar.workspaces.count` ∈ [3, 10]) are clamped; a `qCWarning` is logged showing the clamped value
- String enum fields (e.g., `theme.mode` ∈ {dark, light, system}) fall back to default with a warning if an unknown value is provided
- Non-empty string fields (e.g., `theme.variant`, `theme.accent`) reject empty or whitespace-only strings, log a warning, and use the default
- Positive-integer fields reject values ≤ 0, log a warning, and use the default
- No field validation throws an exception

---

### REQ-F-007: tomlQuote() utility function

**Statement:** The library shall provide a utility function `QString tomlQuote(const QString& value)` that escapes and quotes a QString value for safe embedding in TOML output.

**Acceptance Criterion:**
- Strings containing quotes, backslashes, or newlines are properly escaped (e.g., `"foo\"bar"` becomes `"foo\\\"bar"` in TOML output)
- The returned string is a valid TOML string literal suitable for direct output
- Empty strings are represented as `""`
- The function does not modify non-special characters

---

### REQ-F-008: writeMissingDefaults() function

**Statement:** The library shall provide a function `bool writeMissingDefaults(const QString& path, const MissingDefaults& missing)` that appends all missing config keys with their default values to the TOML file at the given path.

**Acceptance Criterion:**
- For each missing field in the `missing` struct, a TOML assignment is appended to the file (e.g., `theme.variant = "Storm"`)
- Assignments are organized by section (e.g., all `[appearance]` keys grouped together)
- The file is created with `QDir::mkpath()` if the directory does not exist
- The function returns true on success and false on I/O failure
- On I/O failure, a `qCWarning` is logged with the error details
- If no fields are missing (`missing.any()` is false), the file is not modified

---

### REQ-F-009: ConfigWriter class and write() method

**Statement:** The library shall provide a `ConfigWriter` class with a static method `bool write(const ParsedConfig& config, const QString& path)` that serializes the entire config to TOML and atomically writes the file.

**Acceptance Criterion:**
- The method creates a complete, valid TOML file containing all config sections (appearance, theme, bar, background, weather, notifications, widgets, calendar)
- All fields are serialized in their canonical form (e.g., integers as unquoted numbers, strings as quoted, arrays as `[…]`, tables as `[section]`)
- Each TOML section header (e.g., `[appearance]`) appears exactly once, followed by all key-value pairs for that section
- The method uses `QSaveFile` to atomically write: the file is written to a temporary location and then moved to the target path, ensuring no partial writes on crash
- The method returns true on success and false on I/O or validation failure
- On I/O failure, a `qCWarning` is logged with the error details and the temporary file is cleaned up

---

### REQ-F-010: Atomic file write via QSaveFile

**Statement:** `ConfigWriter::write()` shall use `QSaveFile` to ensure atomic writes: if the write or commit fails, the target file remains unchanged.

**Acceptance Criterion:**
- A successful write creates the destination file with the new config content
- If `commit()` fails (e.g., out of disk space), the original file (if it existed) is unchanged
- The temporary file is cleaned up (not left behind) on failure
- The directory structure is created with `QDir::mkpath()` if needed

---

### REQ-F-011: File paths are tilde-expanded for background images

**Statement:** During parsing, tilde (`~`) in background image paths shall be expanded to the home directory via `QDir::homePath()`.

**Acceptance Criterion:**
- A config entry `background.images = ["~/pictures/bg1.png"]` is expanded to the absolute home path (e.g., `/home/user/pictures/bg1.png`)
- A path `~/path` becomes `$HOME/path`, and `~` alone becomes `$HOME`
- Non-tilde paths are passed through unchanged

---

### REQ-F-012: Config structs ship with private equality operators

**Statement:** All config structs shall declare `bool operator==(const T&) const = default;` or an explicit definition to enable comparison.

**Acceptance Criterion:**
- Structs can be compared with `==` and `!=` operators
- Two structs with identical field values compare equal
- Struct equality is used by `ConfigService` to detect changes

---

### REQ-F-013: Widget enums: WidgetPosition and WidgetType

**Statement:** The library shall declare `enum class WidgetPosition : std::uint8_t` with nine values (LeftTop, CenterTop, RightTop, LeftCenter, CenterCenter, RightCenter, LeftBottom, CenterBottom, RightBottom) and `enum class WidgetType : std::uint8_t` with values TimeToEvent and Clock.

**Acceptance Criterion:**
- `WidgetPosition` has exactly nine named values, each representing a screen corner or edge
- `WidgetType` has two values for the two supported widget types
- Both enums are serializable to/from strings via dedicated functions

---

### REQ-F-014: widgetPositionFromString() function

**Statement:** The library shall provide `std::optional<WidgetPosition> widgetPositionFromString(const QString& value)` that maps position strings (e.g., "center-top") to enum values.

**Acceptance Criterion:**
- Input "left-top" maps to `WidgetPosition::LeftTop`
- Input "center-top" maps to `WidgetPosition::CenterTop`
- All nine position strings are supported
- Unknown strings return `std::nullopt`

---

### REQ-F-015: widgetPositionToString() function

**Statement:** The library shall provide `QString widgetPositionToString(WidgetPosition position)` that converts enum values back to config strings (e.g., `LeftTop` → "left-top").

**Acceptance Criterion:**
- Each `WidgetPosition` value maps to its canonical position string
- The output can be round-tripped: `widgetPositionToString(widgetPositionFromString(s)) == s` for valid position strings

---

### REQ-F-016: widgetPositionIsTopAnchored() function

**Statement:** The library shall provide `bool widgetPositionIsTopAnchored(WidgetPosition position)` that returns true for the three top-anchored positions (LeftTop, CenterTop, RightTop).

**Acceptance Criterion:**
- Returns true for LeftTop, CenterTop, RightTop
- Returns false for the other six positions
- Used by the shell to clear space below the top bar for top-anchored widgets

---

### REQ-F-017: CalendarConfig account management

**Statement:** `CalendarConfig` shall contain two separate lists: `caldav_accounts` (list of `CalendarCaldavAccountConfig`) and `ics_accounts` (list of `CalendarIcsAccountConfig`), each parsed from a TOML array-of-tables.

**Acceptance Criterion:**
- TOML `[[calendar.caldav_accounts]]` sections are parsed into `CalendarCaldavAccountConfig` structs in the `caldav_accounts` list
- TOML `[[calendar.ics_accounts]]` sections are parsed into `CalendarIcsAccountConfig` structs in the `ics_accounts` list
- An empty list (no array-of-tables sections) is valid and represents the account type being disabled
- Each account struct retains its required fields: `account_name`, `url`, `username`, `password_keyring_key` (CalDAV) or `label` (ICS)

---

### REQ-F-018: TrayIconOverrides parsing and validation

**Statement:** Tray icon overrides are parsed from a TOML table of tables (`[tray.icon_overrides.<name>]`), with structural validation requiring each override to have at least one matcher (id, service, object_path, or title) and a non-empty icon.

**Acceptance Criterion:**
- Each `[tray.icon_overrides.<name>]` section is parsed as a `TrayIconOverrideConfig`
- An override with an empty icon field is skipped with a warning
- An override with no matchers (all four fields empty) is skipped with a warning
- Valid overrides are added to the `items` list
- The list can be empty if no valid overrides are present

---

### REQ-F-019: Default config file location

**Statement:** The library functions accept a path parameter; all defaults and lookups use `$XDG_CONFIG_HOME/holonight/config.toml` (or `~/.config/holonight/config.toml` if `XDG_CONFIG_HOME` is unset).

**Acceptance Criterion:**
- No path is hardcoded in the library; the caller (`ConfigService`) determines the file location
- The library functions work with any path passed by the caller
- Path construction respects XDG Base Directory Specification conventions

---

### REQ-F-020: No schema migration in holonight_config

**Statement:** The library shall not implement or attempt any config file schema migration; all versioning and backward compatibility are the responsibility of the caller.

**Acceptance Criterion:**
- The library contains no migration logic, version checks, or field remapping
- If a config file uses an old schema, parsing falls back to defaults; no error is raised
- The caller (ConfigService or holonight-settings) is responsible for any schema evolution

---

## Non-Functional Requirements

### REQ-NF-001: No QObject or Wayland dependencies

**Statement:** The `holonight_config` library shall not depend on QObject, Qt signals/slots, QFileSystemWatcher, or any Wayland protocol headers; it shall contain only pure C++ data structures and functions.

**Acceptance Criterion:**
- The library does not include `<QObject>`, `<QtDBus>`, or any Wayland protocol header
- A compiler build with `-fno-rtti` or similar restrictions does not break the library (though Qt may still require RTTI)
- The library can be linked into non-Qt applications (though this is not a planned use case)

---

### REQ-NF-002: No file watching in holonight_config

**Statement:** The library shall not include file system watching, reloading, or live update logic; parsing is a synchronous, one-time operation per file read.

**Acceptance Criterion:**
- The library does not include QFileSystemWatcher or any inotify/FSEvents binding
- `parseConfigTable()` is synchronous and returns immediately
- File monitoring and re-parse triggering are handled by ConfigService

---

### REQ-NF-003: Logging uses Qt logging infrastructure

**Statement:** All warnings and errors in holonight_config shall use `qCWarning()` or `qCCritical()` with a dedicated logging category `lcConfig` (or similar).

**Acceptance Criterion:**
- A `Q_LOGGING_CATEGORY()` is declared in the `.cpp` implementation file
- All parse errors, validation failures, and I/O errors log via `qCWarning(lcConfig) << ...`
- Logs are human-readable and include the config key and the problem (e.g., "Config: theme.mode 'invalid' is not dark, light, or system")

---

### REQ-NF-004: Validation never throws exceptions

**Statement:** The library shall not throw exceptions for invalid config data; all error handling shall use logging and fallback defaults.

**Acceptance Criterion:**
- No `std::exception` or derived types are thrown by public functions
- No `assert()` statements are used for validation; all failures are logged and handled gracefully
- Callers can safely invoke parsing on untrusted TOML files without wrapping in try-catch blocks

---

### REQ-NF-005: No hardcoded paths in library

**Statement:** The library shall not contain hardcoded file paths (e.g., `~/.config/holonight/config.toml`); all paths are passed by the caller.

**Acceptance Criterion:**
- Functions accept path parameters and make no assumptions about file locations
- The library does not call `XDG_CONFIG_HOME` or `HOME` environment variables directly
- Path resolution is entirely delegated to the caller

---

### REQ-NF-006: Performance: parsing completes in < 50ms

**Statement:** `parseConfigTable()` shall complete in less than 50 milliseconds on a typical desktop system, including all validation and logging.

**Acceptance Criterion:**
- A benchmark test parses a typical config file with 10+ sections and measures < 50ms wall-clock time
- Parsing does not allocate excessive memory or perform unnecessary copies

---

### REQ-NF-007: ConfigWriter performance: write completes in < 100ms

**Statement:** `ConfigWriter::write()` shall complete in less than 100 milliseconds on a typical desktop system, including file I/O.

**Acceptance Criterion:**
- A benchmark test serializes a full `ParsedConfig` and writes to a temp file in < 100ms wall-clock time
- The write includes QSaveFile overhead

---

### REQ-NF-008: Code style and formatting

**Statement:** All code in holonight_config shall pass `task format-check`, `task tidy`, and `task qml-lint` without errors or warnings related to the new code.

**Acceptance Criterion:**
- `task format-check` reports the code is correctly formatted (clang-format compliance)
- `task tidy` reports no warnings for code in holonight_config sources
- Variable names are ≥ 3 characters; no single-letter loop counters or abbreviations (per clang-tidy `readability-identifier-length`)
- No hardcoded strings; magic numbers are named constants with clear intent

---

### REQ-NF-009: No external dependencies outside Qt + toml++

**Statement:** The library shall not depend on any third-party libraries beyond Qt6::Core and toml++ (header-only).

**Acceptance Criterion:**
- CMakeLists.txt links only to Qt6::Core and declares toml++ (via find_package or local include)
- No libconfig, jansson, or other config-parsing libraries are used
- The library is self-contained and does not require additional system packages beyond Qt

---

## Constraint Requirements

### REQ-C-001: Static library, not shared or object library

**Statement:** The `holonight_config` target shall be configured as `STATIC` (not `SHARED` or `OBJECT`) to enable simple linking into both the shell and future tools.

**Acceptance Criterion:**
- `add_library(holonight_config STATIC ...)` is declared in CMakeLists.txt
- The build produces `libholonight_config.a` (Linux/macOS) or `holonight_config.lib` (Windows)

---

### REQ-C-002: Public headers under include/holonight_config/

**Statement:** All public headers shall be installed under `include/holonight_config/` (e.g., `config_structs.h`, `config_writer.h`, `config_parsers.h`) and set via CMakeLists.txt `target_include_directories(...PUBLIC)`.

**Acceptance Criterion:**
- Public headers are in a `holonight_config/` subdirectory (not at the root or in `src/`)
- Callers use `#include <holonight_config/config_structs.h>` (angle-bracket include path)
- Internal implementation headers use `#include "ConfigParsers.h"` (relative includes)

---

### REQ-C-003: Source files remain in src/core/

**Statement:** Implementation files (`ConfigParsers.cpp`, `ConfigWriter.cpp`, etc.) shall remain in `src/core/` for now; only public headers are moved to `include/holonight_config/`.

**Acceptance Criterion:**
- `.cpp` files are at `src/core/ConfigParsers.cpp`, `src/core/ConfigWriter.cpp`, etc.
- Public headers are at `src/holonight_config/config_structs.h`, etc., or copied to `include/holonight_config/` during install
- CMakeLists.txt `target_sources()` includes the `.cpp` files from `src/core/`

---

### REQ-C-004: ConfigService delegates to holonight_config functions

**Statement:** After the library is extracted, `ConfigService` (in `holonight_services`) shall call `parseConfigTable()` and `ConfigWriter::write()` from holonight_config; no parsing logic is duplicated.

**Acceptance Criterion:**
- `ConfigService::loadOrCreateConfig()` calls `parseConfigTable()` instead of inline parsing
- `ConfigService::writeConfig()` calls `ConfigWriter::write()` instead of inline serialization
- All validation and default handling is delegated to the library
- ConfigService retains only the QObject wrapper, file watching, and signal emission logic

---

### REQ-C-005: toml++ is a header-only dependency

**Statement:** toml++ shall be linked as a header-only library (no compiled `.a` or `.so`); it is included via CMakeLists.txt `find_package(tomlplusplus CONFIG REQUIRED)` or as a vendored header.

**Acceptance Criterion:**
- toml++ headers are located in `third_party/toml/` or fetched via package manager
- CMakeLists.txt declares `find_package(tomlplusplus CONFIG REQUIRED)` or includes the vendor path
- No separate compilation or linking of toml++ source files is required

---

### REQ-C-006: No binary data serialization

**Statement:** All config writing shall use text TOML format; no binary serialization (msgpack, protobuf, JSON) is used.

**Acceptance Criterion:**
- `ConfigWriter::write()` produces human-readable TOML text
- The output file is directly editable by users with a text editor
- No binary magic numbers or encoded blobs are present

---

### REQ-C-007: Struct defaults are hardcoded, not externalized

**Statement:** Default field values for all config structs shall be hardcoded as member initializers (e.g., `QString variant{"Storm"};`); no external defaults file is loaded.

**Acceptance Criterion:**
- Each struct declares its default values as in-class member initializers
- No defaults.toml, defaults.json, or similar file is read
- Struct definitions are the single source of truth for defaults

---

### REQ-C-008: Calendar password storage via libsecret

**Statement:** Calendar account passwords are not stored in the TOML config file; instead, a `password_keyring_key` field (e.g., "holonight-shell/caldav/work") references a libsecret lookup key, and the caller retrieves the password separately.

**Acceptance Criterion:**
- `CalendarCaldavAccountConfig::password_keyring_key` is a QString (not the password itself)
- The TOML config file stores only the keyring lookup key, not the password
- Password retrieval (via libsecret) is the caller's responsibility

---

### REQ-C-009: Weather geolocation coordinates are optional

**Statement:** `WeatherConfig::latitude` and `WeatherConfig::longitude` are `std::optional<double>` (not required fields), and absent coordinates trigger IP-geolocation fallback.

**Acceptance Criterion:**
- Both fields are `std::optional<double>{}`
- If neither is present in the TOML file, the weather service falls back to IP-geolocation
- If one is present and the other absent, the present value is used; absence does not fall back (per current behavior)
- The `writeMissingDefaults()` function does not write back latitude/longitude; they are never auto-populated

---

### REQ-C-010: No external network calls in holonight_config

**Statement:** The library shall not make any network calls (HTTP, DNS, or otherwise); all geolocation and API calls are deferred to the service layer.

**Acceptance Criterion:**
- The library contains no networking code
- Weather API key validation (presence/absence) is syntactic only, not functional
- The caller (WeatherService) is responsible for validating API keys and fetching data

---

### REQ-C-011: Enum variants are immutable at runtime

**Statement:** `WidgetPosition` and `WidgetType` enum values are immutable; no runtime mutation or extension is supported.

**Acceptance Criterion:**
- Enums are declared with `enum class` (strongly typed, no implicit conversion to int)
- No new variants are added dynamically
- Serialization/deserialization maps are fixed at compile time

---

### REQ-C-012: No config modification during parsing

**Statement:** `parseConfigTable()` is a pure function with no side effects; it does not modify the input TOML table or any global state.

**Acceptance Criterion:**
- The function takes a `const toml::table&` (immutable reference)
- The function returns a new `ParsedConfig` instance; it does not mutate any existing objects
- Calling the function multiple times with the same input yields identical results

---

## Event-Driven Requirements

### REQ-F-021: writeMissingDefaults() triggers only when defaults are absent

**Statement:** When `ConfigService` loads the config file and `missing.any()` returns true, ConfigService shall call `writeMissingDefaults()` to append missing keys to the file.

**Acceptance Criterion:**
- If the config file is complete and no fields are missing, `writeMissingDefaults()` is not called
- If one or more fields are missing, `writeMissingDefaults()` is called once per load/reload cycle
- The file is not modified if all fields are already present

---

### REQ-F-022: ConfigWriter::write() produces a new file or overwrites existing file

**Statement:** When `holonight-settings` (or another tool) calls `ConfigWriter::write()`, the library shall atomically write a complete new config file, preserving all sections and replacing the old file.

**Acceptance Criterion:**
- The old file (if present) is replaced in-place
- No partial writes occur (QSaveFile ensures atomicity)
- All fields in the input `ParsedConfig` are serialized to the output file
- User-configured values are preserved; no data loss occurs

---

## State-Driven Requirements

### REQ-F-023: Validation occurs during parsing, not after

**Statement:** Field validation (range clamping, type checking, enum validation) shall occur within `parseConfigTable()` as each field is read, not in a separate validation pass.

**Acceptance Criterion:**
- Invalid values are clamped or rejected immediately when `readInt()`, `readStr()`, or similar is called
- A warning is logged for each violation
- The returned `ParsedConfig` contains only valid, normalized values
- No further validation is needed after parsing returns

---

## Conditional Requirements

### REQ-F-024: writeMissingDefaults() creates the directory if needed

**Statement:** If the config directory does not exist when `writeMissingDefaults()` is called, the function shall create it using `QDir::mkpath()`.

**Acceptance Criterion:**
- `writeMissingDefaults("~/.config/holonight/config.toml", missing)` creates `~/.config/holonight/` if it does not exist
- `QDir::mkpath()` returns true and the directory is created with appropriate permissions

---

### REQ-F-025: ConfigWriter::write() creates the directory if needed

**Statement:** If the config directory does not exist when `ConfigWriter::write()` is called, the function shall create it using `QDir::mkpath()`.

**Acceptance Criterion:**
- `ConfigWriter::write(config, "~/.config/holonight/config.toml")` creates the directory if needed
- The function does not fail due to missing parent directories

---

### REQ-F-026: If TOML parsing fails, the library returns defaults

**Statement:** If toml++ fails to parse the TOML file (syntax error, malformed structure), the caller receives a `ParsedConfig` with all default values and a `qCWarning` is logged.

**Acceptance Criterion:**
- Invalid TOML syntax is caught (e.g., unclosed quotes, invalid numbers)
- The function logs a warning with the parse error details
- A `ParsedConfig` with all defaults is returned; the caller can proceed without crashing

---

## Unwanted Behaviour Requirements

### REQ-F-027: The library shall NOT throw exceptions

**Statement:** The library shall not throw exceptions for any reason (invalid input, I/O errors, memory exhaustion); all failures shall be logged and handled with fallback values.

**Acceptance Criterion:**
- No `throw` statements are present in the code
- No STL containers or functions that throw (e.g., `std::vector::at()`) are used without catching and converting to warnings
- The library is exception-safe and returns a valid result (or false) on error

---

### REQ-F-028: The library shall NOT modify the config file during parsing

**Statement:** `parseConfigTable()` shall not write to the config file; only `writeMissingDefaults()` and `ConfigWriter::write()` may modify files.

**Acceptance Criterion:**
- No `QFile::open(WriteOnly)` or `QSaveFile::open()` occurs in parsing code
- The config file is read-only during parsing
- Parsing is idempotent: calling it multiple times produces identical results

---

### REQ-F-029: The library shall NOT silently drop invalid config entries

**Statement:** When an invalid config entry is encountered (wrong type, out-of-range, malformed structure), the library shall log a warning and use a default value; the invalid entry shall not be silently ignored.

**Acceptance Criterion:**
- Every invalid field triggers a `qCWarning()` with details of the problem
- The warning includes the key name and the reason for rejection (e.g., "integer expected but got string")
- The default value used is logged or implied by the warning message

---

### REQ-F-030: The library shall NOT require online connectivity

**Statement:** Config parsing and writing shall not require any network connectivity; all operations are offline and local-file-only.

**Acceptance Criterion:**
- No DNS lookups, HTTP requests, or other network operations occur during parsing or writing
- The library works correctly in an offline environment (e.g., no internet connection)

---

### REQ-F-031: The library shall NOT persist mutable state

**Statement:** The library shall not store mutable state between calls; each call to `parseConfigTable()` or `ConfigWriter::write()` is independent and stateless.

**Acceptance Criterion:**
- No static mutable variables or global state are modified by public functions
- Calling a function twice with the same input produces identical results (idempotent)
- Thread safety is achieved through immutability, not locking

---

## Acceptance Criteria Summary

**Library structure:**
- [ ] `holonight_config` CMake target builds as a static library
- [ ] Public headers are in `include/holonight_config/` (or similar)
- [ ] ConfigService links against `holonight_config` and calls its functions
- [ ] `holonight_services` CMakeLists.txt adds `holonight_config` to its dependencies

**Parsing functionality:**
- [ ] `parseConfigTable()` correctly parses all config sections (appearance, theme, bar, background, weather, notifications, widgets, calendar)
- [ ] Out-of-range integers are clamped with warnings
- [ ] Invalid enum strings are rejected with warnings
- [ ] Empty or whitespace strings are rejected for non-empty fields
- [ ] MissingDefaults struct tracks which fields were absent
- [ ] Parsing completes in < 50ms for a typical config file

**Writing functionality:**
- [ ] `ConfigWriter::write()` produces a valid, complete TOML file
- [ ] The file can be parsed back by toml++ without errors
- [ ] QSaveFile ensures atomic writes (no partial writes on crash)
- [ ] Directory is created if it does not exist
- [ ] Writing completes in < 100ms

**Default handling:**
- [ ] `writeMissingDefaults()` appends only the missing keys to the file
- [ ] If no fields are missing, the file is not modified
- [ ] Default values in the appended keys match the struct defaults exactly

**Code quality:**
- [ ] `task build` succeeds without warnings
- [ ] `task format-check` passes (clang-format)
- [ ] `task tidy` passes (clang-tidy)
- [ ] Variable names are ≥ 3 characters
- [ ] No hardcoded paths in the library

**ConfigService integration:**
- [ ] ConfigService calls `parseConfigTable()` instead of inline parsing
- [ ] ConfigService calls `ConfigWriter::write()` instead of inline serialization
- [ ] ConfigService retains QObject wrapper and file watching logic
- [ ] ConfigService emits signals on config changes (unchanged behavior)
- [ ] Shell builds and runs without crashes after extraction

---

## Glossary

- **TOML**: Tom's Obvious, Minimal Language (configuration file format)
- **toml++**: Header-only C++17 TOML parser library
- **XDG Base Directory**: Standard for config file locations on Unix-like systems
- **libsecret**: Desktop password storage service (used for calendar credentials)
- **QSaveFile**: Qt class for atomic file writing
- **Tilde expansion**: Converting `~` to the user's home directory path
- **MissingDefaults**: Tracker struct marking fields absent from the config file


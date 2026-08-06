# SDD Tasks — config-library

- [x] T-001: CMake: add `holonight_config` target
  - REQs: REQ-C-001, REQ-F-001
  - Check: `libholonight_config.a` exists in build/ after `cmake -B build && cmake --build build`, and `holonight_services` links successfully.

- [x] T-002: Create `include/holonight_config/` directory structure
  - REQs: REQ-C-002, REQ-F-001
  - Check: Three header files (`config_structs.h`, `config_parsers.h`, `config_writer.h`) exist in `include/holonight_config/` with correct `#pragma once` and includes.

- [x] T-003: Move config structs to `config_structs.h`
  - REQs: REQ-F-002, REQ-F-013, REQ-F-014, REQ-F-015, REQ-F-016, REQ-C-007
  - Check: All config structs (AppearanceConfig, ThemeConfig, BarWorkspacesConfig, BarSystemTrayConfig, TrayIconOverridesConfig, BackgroundConfig, WeatherConfig, NotificationsConfig, NotificationHistoryConfig, WidgetsConfig, CalendarConfig, WidgetDefinition, TimeToEventConfig, ClockConfig) are declared in `config_structs.h` with `= default` operator==, and enum declarations (WidgetPosition, WidgetType, WeekStartDay) are present with free function declarations.

- [x] T-004: Move parser declarations to `config_parsers.h`
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-007, REQ-F-008, REQ-C-002
  - Check: ParsedConfig, MissingDefaults, and function declarations (tomlQuote, parseConfigTable, writeMissingDefaults) are declared in `config_parsers.h` with correct includes (`#include <holonight_config/config_structs.h>` and toml++/toml.h).

- [x] T-005: Create `config_writer.h`
  - REQs: REQ-F-009, REQ-F-010, REQ-C-002
  - Check: ConfigWriter class with static method `bool write(const ParsedConfig& config, const QString& path)` is declared in `config_writer.h`.

- [x] T-006: Update ConfigParsers.cpp includes and move free function implementations
  - REQs: REQ-F-014, REQ-F-015, REQ-F-016, REQ-NF-003, REQ-C-003
  - Check: ConfigParsers.cpp includes changed to `#include <holonight_config/config_parsers.h>`, Q_LOGGING_CATEGORY declared, and widgetPositionFromString, widgetPositionToString, widgetPositionIsTopAnchored, BackgroundConfig::imageForMonitor implementations are present in ConfigParsers.cpp.

- [x] T-007: Create ConfigWriter.cpp
  - REQs: REQ-F-009, REQ-F-010, REQ-F-025, REQ-NF-003, REQ-NF-007, REQ-C-006
  - Check: ConfigWriter::write() is fully implemented with TOML serialization for all 8+ sections (appearance, theme, bar.workspaces, bar.system_tray, tray.icon_overrides, background, weather, notifications, calendar), uses QSaveFile for atomic writes, logs warnings on I/O failure, and includes Q_LOGGING_CATEGORY(lcConfigWriter, "holonight.config.writer").

- [x] T-008: Update ConfigService.h
  - REQs: REQ-F-002, REQ-C-004
  - Check: All config struct declarations removed, `struct ParsedConfig;` forward declaration removed, and three new includes added (`#include <holonight_config/config_structs.h>`, `#include <holonight_config/config_parsers.h>`, `#include <holonight_config/config_writer.h>`).

- [x] T-009: Update ConfigService.cpp
  - REQs: REQ-F-014, REQ-F-015, REQ-F-016, REQ-C-004
  - Check: writeConfig() body delegates to `ConfigWriter::write(ParsedConfig{}, config_path_)`, widgetPosition* free function implementations removed, BackgroundConfig::imageForMonitor implementation removed, and includes updated.

- [x] T-010: Delete ConfigParsers.h
  - REQs: REQ-C-002, REQ-C-003
  - Check: ConfigParsers.h is deleted from `src/core/`, and no unresolved include errors in build.

- [x] T-011: Link holonight_config from holonight_services
  - REQs: REQ-C-004, REQ-F-001
  - Check: holonight_services CMakeLists.txt target_link_libraries includes holonight_config, and holonight-shell binary links successfully without unresolved symbols.

- [x] T-012: Full rebuild and verify
  - REQs: REQ-NF-008, REQ-NF-006, REQ-NF-007
  - Check: `rm -rf build && task configure && task build && task format-check && task tidy` all pass with zero errors and zero new warnings in holonight_config source files.

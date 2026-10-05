#include <algorithm>
#include <cmath>
#include <holonight_shell_config/config_schema.h>
#include <limits>

namespace HoloNight::ShellConfig {
namespace {
using Config::Diagnostic;
using Config::KeyPath;
using Config::Value;

Diagnostic invalid(std::string message) {
  return {
      .code = Config::ErrorCode::ValidationError,
      .severity = Config::Severity::Error,
      .message = std::move(message),
      .path = std::nullopt,
      .position = std::nullopt,
  };
}

std::string label(const KeyPath& key) {
  std::string result;
  for (const auto& segment : key) {
    if (!result.empty()) {
      result += '.';
    }
    result += segment;
  }
  return result;
}

std::vector<SettingMetadata> makeMetadata() {
  const ProductConfig defaults;
  std::vector<SettingMetadata> result;
  auto add = [&result](KeyPath key, Value value, std::string description, std::optional<double> minimum = {},
                       std::optional<double> maximum = {}, std::vector<std::string> choices = {}) {
    result.push_back({
        .key = std::move(key),
        .default_value = std::move(value),
        .description = std::move(description),
        .minimum = minimum,
        .maximum = maximum,
        .choices = std::move(choices),
    });
  };
  auto integer = [&add](KeyPath key, int value, std::string description, int minimum,
                        int maximum = std::numeric_limits<int>::max()) {
    add(std::move(key), std::int64_t{value}, std::move(description), minimum, maximum);
  };
  auto text = [&add](KeyPath key, const QString& value, std::string description,
                     std::vector<std::string> choices = {}) {
    add(std::move(key), value.toStdString(), std::move(description), {}, {}, std::move(choices));
  };
  add({"bar", "taskbar", "enabled"}, defaults.taskbar.enabled, "Show the taskbar");
  add({"bar", "taskbar", "grouped"}, defaults.taskbar.grouped, "Group windows by application");
  add({"bar", "taskbar", "overview_access"}, defaults.taskbar.overview_access, "Enable window overview access");
  add({"bar", "taskbar", "desktop_menu"}, defaults.taskbar.desktop_menu, "Enable the desktop menu");
  integer({"bar", "workspaces", "count"}, defaults.bar_workspaces.count, "Visible workspace count",
          BarWorkspacesConfig::kMinCount, BarWorkspacesConfig::kMaxCount);
  integer({"bar", "systemtray", "max_items"}, defaults.bar_system_tray.max_items, "Visible tray item count",
          BarSystemTrayConfig::kMinMaxItems, BarSystemTrayConfig::kMaxMaxItems);
  add({"background", "images"}, Config::AggregateValue{"[]"}, "Ordered monitor wallpaper paths");
  text({"weather", "provider"}, defaults.weather.provider, "Weather provider", {"open-meteo", "openweathermap"});
  text({"weather", "location_source"}, defaults.weather.location_source, "Weather location source", {"manual", "auto"});
  text({"weather", "api_key"}, defaults.weather.api_key, "Weather provider API key");
  text({"weather", "geo_api_key"}, defaults.weather.geo_api_key, "Geolocation API key");
  text({"weather", "city"}, defaults.weather.city, "Location city");
  text({"weather", "country"}, defaults.weather.country, "Location country");
  text({"weather", "units"}, defaults.weather.units, "Legacy weather units", {"metric", "imperial", "standard"});
  text({"weather", "temp_unit"}, defaults.weather.temp_unit, "Temperature unit", {"celsius", "fahrenheit"});
  text({"weather", "wind_unit"}, defaults.weather.wind_unit, "Wind speed unit", {"kmh", "ms", "mph", "knots"});
  text({"weather", "pressure_unit"}, defaults.weather.pressure_unit, "Pressure unit", {"hpa", "mmhg", "inhg", "bar"});
  text({"weather", "lang"}, defaults.weather.lang, "Weather language");
  add({"weather", "show_in_bar"}, defaults.weather.show_in_bar, "Show weather in the bar");
  add({"weather", "compact_mode"}, defaults.weather.compact_mode, "Use compact weather presentation");
  add({"weather", "show_feels_like"}, defaults.weather.show_feels_like, "Show feels-like temperature");
  add({"weather", "show_location"}, defaults.weather.show_location, "Show the weather location");
  integer({"weather", "refresh_interval"}, defaults.weather.refresh_interval, "Weather refresh interval in seconds", 1);
  result.push_back({
      .key = {"weather", "latitude"},
      .default_value = std::nullopt,
      .description = "Latitude",
      .minimum = -90.0,
      .maximum = 90.0,
      .choices = {},
  });
  result.push_back({
      .key = {"weather", "longitude"},
      .default_value = std::nullopt,
      .description = "Longitude",
      .minimum = -180.0,
      .maximum = 180.0,
      .choices = {},
  });
  integer({"notifications", "default_timeout_ms"}, defaults.notifications.default_timeout_ms,
          "Notification timeout in milliseconds", 1);
  integer({"notifications", "max_visible"}, defaults.notifications.max_visible, "Visible notification count",
          NotificationsConfig::kMinVisible, NotificationsConfig::kMaxVisible);
  add({"notifications", "history", "enabled"}, defaults.notification_history.enabled, "Keep notification history");
  integer({"notifications", "history", "max_items"}, defaults.notification_history.max_items, "History item limit", 1);
  integer({"notifications", "history", "max_age_days"}, defaults.notification_history.max_age_days,
          "History age limit in days", 1);
  add({"notifications", "history", "persist_body"}, defaults.notification_history.persist_body,
      "Persist notification body text");
  text({"calendar", "week_start_day"},
       defaults.calendar.week_start_day == WeekStartDay::Monday ? QStringLiteral("Mon") : QStringLiteral("Sun"),
       "First weekday", {"Mon", "Sun"});
  text({"logo", "file"}, defaults.logo.file, "Logo file path");
  result.back().reload = Config::ReloadPolicy::Restart;
  add({"logo", "generic"}, defaults.logo.generic, "Use the generic logo");
  result.back().reload = Config::ReloadPolicy::Restart;
  integer({"widgets", "margin"}, defaults.widgets.margin, "Desktop widget margin", 0);
  add({"osd", "enabled"}, defaults.osd.enabled, "Enable on-screen display");
  integer({"osd", "timeout"}, defaults.osd.timeout_ms, "On-screen display timeout in milliseconds",
          OsdConfig::kMinTimeoutMs, OsdConfig::kMaxTimeoutMs);
  text({"osd", "position"}, widgetPositionToString(defaults.osd.position), "On-screen display position",
       {
           "left-top",
           "center-top",
           "right-top",
           "left-center",
           "center-center",
           "right-center",
           "left-bottom",
           "center-bottom",
           "right-bottom",
       });
  add({"osd", "volume", "enabled"}, defaults.osd.volume.enabled, "Show volume changes");
  add({"osd", "brightness", "enabled"}, defaults.osd.brightness.enabled, "Show brightness changes");
  add({"osd", "keyboard_layout", "enabled"}, defaults.osd.keyboard_layout.enabled, "Show keyboard layout changes");
  return result;
}

std::vector<Diagnostic> validateValue(const Value& value, const SettingMetadata& metadata) {
  const auto expected = metadata.default_value ? metadata.default_value->index() : Value{0.0}.index();
  const bool numeric = expected == Value{0.0}.index();
  if (value.index() != expected && !(numeric && std::holds_alternative<std::int64_t>(value))) {
    return {invalid(label(metadata.key) + ": invalid value type")};
  }
  double number = 0.0;
  if (const auto* integer = std::get_if<std::int64_t>(&value)) {
    number = static_cast<double>(*integer);
  }
  if (const auto* real = std::get_if<double>(&value)) {
    number = *real;
  }
  if ((metadata.minimum && number < *metadata.minimum) || (metadata.maximum && number > *metadata.maximum) ||
      ((metadata.minimum || metadata.maximum) && !std::isfinite(number))) {
    return {invalid(label(metadata.key) + ": value is outside the allowed range")};
  }
  if (!metadata.choices.empty()) {
    const auto* text = std::get_if<std::string>(&value);
    if ((text == nullptr) || std::ranges::find(metadata.choices, *text) == metadata.choices.end()) {
      return {invalid(label(metadata.key) + ": unsupported choice")};
    }
  }
  return {};
}

std::optional<Value> nodeValue(const toml::node& node) {
  if (node.is_boolean()) {
    return *node.value<bool>();
  }
  if (node.is_integer()) {
    return *node.value<std::int64_t>();
  }
  if (node.is_floating_point()) {
    return *node.value<double>();
  }
  if (node.is_string()) {
    return *node.value<std::string>();
  }
  if (node.is_array()) {
    return Config::AggregateValue{"[]"};
  }
  return std::nullopt;
}

void checkStrings(const toml::node* node, const std::string& name, std::vector<Diagnostic>& errors) {
  if (node == nullptr) {
    return;
  }
  const auto* array = node->as_array();
  if ((array == nullptr) || !std::ranges::all_of(*array, [](const toml::node& item) { return item.is_string(); })) {
    errors.push_back(invalid(name + ": expected an array of strings"));
  }
}

void checkFields(const toml::table& table, const std::string& name, std::initializer_list<const char*> strings,
                 std::initializer_list<const char*> booleans, std::vector<Diagnostic>& errors) {
  for (const auto* key : strings) {
    if (const auto* node = table.get(key); (node != nullptr) && !node->is_string()) {
      errors.push_back(invalid(name + "." + key + ": expected a string"));
    }
  }
  for (const auto* key : booleans) {
    if (const auto* node = table.get(key); (node != nullptr) && !node->is_boolean()) {
      errors.push_back(invalid(name + "." + key + ": expected a boolean"));
    }
  }
}

bool hasText(const toml::table& table, const char* key) {
  const auto text = table[key].value<std::string>();
  return text && !text->empty();
}

void validateWidget(const toml::table& table, std::vector<Diagnostic>& errors) {
  checkFields(table, "widget", {"type", "position", "title", "deadline", "date_format", "locale"},
              {"enabled", "show_seconds"}, errors);
  checkStrings(table.get("monitors"), "widget.monitors", errors);
  const auto type = table["type"].value_or(std::string{});
  if (type != "time-to-event" && type != "clock" && type != "mpris") {
    errors.push_back(invalid("widget.type: unknown or missing widget type"));
  }
  const auto position = table["position"].value<std::string>();
  if (position && !widgetPositionFromString(QString::fromStdString(*position))) {
    errors.push_back(invalid("widget.position: unsupported position"));
  }
  if (const auto* value = table.get("pause_hide_minutes")) {
    const auto minutes = value->value<std::int64_t>();
    if (!value->is_integer() || !minutes || *minutes < MprisWidgetConfig::kMinPauseHideMinutes ||
        *minutes > MprisWidgetConfig::kMaxPauseHideMinutes) {
      errors.push_back(invalid("widget.pause_hide_minutes: expected an integer in the allowed range"));
    }
  }
  // Disabled time-to-event entries may retain incomplete drafts, as before.
  if (type == "time-to-event" && table["enabled"].value_or(true)) {
    if (!hasText(table, "title")) {
      errors.push_back(invalid("widget.title: required for an enabled countdown"));
    }
    const auto deadline = QString::fromStdString(table["deadline"].value_or(std::string{}));
    const bool valid = deadline.contains(QLatin1Char('T')) ? QDateTime::fromString(deadline, Qt::ISODate).isValid()
                                                           : QDate::fromString(deadline, Qt::ISODate).isValid();
    if (!valid) {
      errors.push_back(invalid("widget.deadline: required ISO date or date-time"));
    }
  }
}

void validateWidgets(const toml::node* node, std::vector<Diagnostic>& errors) {
  if (node == nullptr) {
    return;
  }
  const auto* array = node->as_array();
  if (array == nullptr) {
    errors.push_back(invalid("widget: expected an array of tables"));
    return;
  }
  for (const auto& item : *array) {
    const auto* table = item.as_table();
    if (table == nullptr) {
      errors.push_back(invalid("widget: expected a table entry"));
      continue;
    }
    validateWidget(*table, errors);
  }
}

void validateCollection(const toml::node* node, const std::string& name, std::vector<Diagnostic>& errors) {
  if (node == nullptr) {
    return;
  }
  const auto* collection = node->as_table();
  if (collection == nullptr) {
    errors.push_back(invalid(name + ": expected a table"));
    return;
  }
  for (const auto& [key, item] : *collection) {
    const auto entry_name = name + "." + std::string{key.str()};
    const auto* entry = item.as_table();
    if (entry == nullptr) {
      errors.push_back(invalid(entry_name + ": expected a table"));
      continue;
    }
    if (name == "tray.icon_overrides") {
      checkFields(*entry, entry_name, {"id", "service", "object_path", "title", "icon", "attention_icon"}, {}, errors);
      if (!hasText(*entry, "icon") || !(hasText(*entry, "id") || hasText(*entry, "service") ||
                                        hasText(*entry, "object_path") || hasText(*entry, "title"))) {
        errors.push_back(invalid(entry_name + ": requires an icon and at least one match selector"));
      }
    } else {
      checkFields(*entry, entry_name, {"url", "username", "password_keyring_key", "label"}, {}, errors);
      if (!hasText(*entry, "url") || (name == "calendar.caldav" && !hasText(*entry, "username"))) {
        errors.push_back(invalid(entry_name + ": missing required account fields"));
      }
      checkStrings(entry->get("include"), entry_name + ".include", errors);
      checkStrings(entry->get("exclude"), entry_name + ".exclude", errors);
    }
  }
}
}  // namespace

const std::vector<SettingMetadata>& settingMetadata() {
  static const auto metadata = makeMetadata();
  return metadata;
}

std::vector<Config::Diagnostic> validateConfigTable(const toml::table& table) {
  std::vector<Diagnostic> errors;
  for (const auto& field : settingMetadata()) {
    const toml::node* node = &table;
    for (const auto& segment : field.key) {
      const auto* parent = node->as_table();
      if (parent == nullptr) {
        errors.push_back(invalid(label(field.key) + ": parent must be a table"));
        node = nullptr;
        break;
      }
      node = parent->get(segment);
      if (node == nullptr) {
        break;
      }
    }
    if (node == nullptr) {
      continue;
    }
    const auto value = nodeValue(*node);
    if (!value) {
      errors.push_back(invalid(label(field.key) + ": invalid value type"));
      continue;
    }
    auto diagnostics = validateValue(*value, field);
    errors.insert(errors.end(), diagnostics.begin(), diagnostics.end());
  }
  checkStrings(table["background"]["images"].node(), "background.images", errors);
  validateWidgets(table.get("widget"), errors);
  validateCollection(table["tray"]["icon_overrides"].node(), "tray.icon_overrides", errors);
  validateCollection(table["calendar"]["caldav"].node(), "calendar.caldav", errors);
  validateCollection(table["calendar"]["ics"].node(), "calendar.ics", errors);
  if (const auto* tray = table.get("tray"); (tray != nullptr) && !tray->is_table()) {
    errors.push_back(invalid("tray: expected a table"));
  }
  return errors;
}

Config::DocumentSchema documentSchema() {
  Config::DocumentSchema schema;
  for (const auto& metadata : settingMetadata()) {
    schema.fields.push_back({
        .key = metadata.key,
        .default_value = metadata.default_value,
        .description = metadata.description,
        .reload = metadata.reload,
        .validate = [metadata](const Value& value) { return validateValue(value, metadata); },
    });
  }
  schema.validate_domain = [](const Config::DocumentSnapshot& snapshot) {
    try {
      return validateConfigTable(toml::parse(snapshot.revision.bytes));
    } catch (const toml::parse_error&) {
      return std::vector<Diagnostic>{invalid("Shell document syntax is invalid")};
    }
  };
  return schema;
}

Config::Result<ProductConfig> decodeDocument(const Config::DocumentSnapshot& snapshot) {
  toml::table table;
  try {
    table = toml::parse(snapshot.revision.bytes);
  } catch (const toml::parse_error&) {
    return Config::Result<ProductConfig>::failure({invalid("Shell document syntax is invalid")});
  }
  auto diagnostics = validateConfigTable(table);
  for (auto& diagnostic : diagnostics) {
    diagnostic.path = snapshot.path;
  }
  if (!diagnostics.empty()) {
    return Config::Result<ProductConfig>::failure(std::move(diagnostics));
  }
  MissingDefaults missing;
  return Config::Result<ProductConfig>::success(parseConfigTable(table, missing));
}
}  // namespace HoloNight::ShellConfig

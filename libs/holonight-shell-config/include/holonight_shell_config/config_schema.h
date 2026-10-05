#pragma once

#include <holonight/config/document.h>
#include <holonight_shell_config/config_parsers.h>

namespace HoloNight::ShellConfig {

// Shell owns these declarations. Editors use the same bounds/choices as readers.
struct SettingMetadata {
  Config::KeyPath key;
  std::optional<Config::Value> default_value;
  std::string description;
  std::optional<double> minimum;
  std::optional<double> maximum;
  std::vector<std::string> choices;
  Config::ReloadPolicy reload{Config::ReloadPolicy::Live};
};

[[nodiscard]] const std::vector<SettingMetadata>& settingMetadata();
[[nodiscard]] Config::DocumentSchema documentSchema();
[[nodiscard]] std::vector<Config::Diagnostic> validateConfigTable(const toml::table& table);
[[nodiscard]] Config::Result<ProductConfig> decodeDocument(const Config::DocumentSnapshot& snapshot);

}  // namespace HoloNight::ShellConfig

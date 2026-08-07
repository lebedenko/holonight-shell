#pragma once

#include <QString>

#include <holonight_shell_config/config_parsers.h>

namespace HoloNight::ShellConfig {

class ProductConfigWriter {
 public:
  ProductConfigWriter() = delete;

  // Serializes config to TOML in canonical section order and writes it atomically
  // via QSaveFile. Creates parent directories if absent. Returns false on I/O failure.
  [[nodiscard]] static bool write(const ProductConfig& config, const QString& path);
};

}  // namespace HoloNight::ShellConfig

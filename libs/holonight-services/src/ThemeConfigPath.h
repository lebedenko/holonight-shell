#pragma once

#include <QString>

struct ThemeConfigPaths {
  QString dir_path;   // e.g. "<xdg-config-home>/holonight"
  QString file_path;  // e.g. "<xdg-config-home>/holonight/theme.conf"
};

// Resolves the HoloNight theme config directory/file paths from $XDG_CONFIG_HOME (falling back to
// ~/.config when unset).
[[nodiscard]] ThemeConfigPaths resolveHolonightThemeConfigPaths();

#include "ThemeConfigPath.h"

#include <QDir>

ThemeConfigPaths resolveHolonightThemeConfigPaths() {
  QString xdg = qEnvironmentVariable("XDG_CONFIG_HOME");
  if (xdg.isEmpty()) {
    xdg = QDir::homePath() + QLatin1String("/.config");
  }
  return {.dir_path = xdg + QLatin1String("/holonight"), .file_path = xdg + QLatin1String("/holonight/theme.conf")};
}

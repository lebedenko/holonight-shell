#include <QDir>

#include <holonight_shell_config/config_path.h>

namespace HoloNight::ShellConfig {
namespace {
constexpr auto kRelativeProductPath = "/holonight/config.toml";
}

QString resolveProductConfigPath(const ProductConfigPathEnvironment& environment) {
  QString base = environment.value(QStringLiteral("XDG_CONFIG_HOME"));
  if (base.isEmpty()) {
    const QString home = environment.value(QStringLiteral("HOME"));
    if (home.isEmpty()) {
      return {};
    }
    base = home + QStringLiteral("/.config");
  }
  return QDir::cleanPath(base + QLatin1String(kRelativeProductPath));
}

QString resolveProductConfigPath() {
  return resolveProductConfigPath({
      {QStringLiteral("XDG_CONFIG_HOME"), qEnvironmentVariable("XDG_CONFIG_HOME")},
      {QStringLiteral("HOME"), qEnvironmentVariable("HOME")},
  });
}

}  // namespace HoloNight::ShellConfig

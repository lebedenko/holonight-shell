#pragma once

#include <QMap>
#include <QString>

namespace HoloNight::ShellConfig {

using ProductConfigPathEnvironment = QMap<QString, QString>;

[[nodiscard]] QString resolveProductConfigPath(const ProductConfigPathEnvironment& environment);
[[nodiscard]] QString resolveProductConfigPath();

}  // namespace HoloNight::ShellConfig

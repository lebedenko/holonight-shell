#pragma once

#include <QString>

#include <optional>

[[nodiscard]] std::optional<int> readSysfsInteger(const QString& path);

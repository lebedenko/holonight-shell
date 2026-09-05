#pragma once

#include <QMap>
#include <QString>
#include <QVariantMap>

namespace Holonight::Authentication {

struct TextLimits {
  qsizetype max_bytes;
  qsizetype max_code_points;
  qsizetype max_lines;
};

inline constexpr TextLimits kRequestMessageLimits{.max_bytes = 8192, .max_code_points = 4096, .max_lines = 12};
inline constexpr TextLimits kPromptLimits{.max_bytes = 4096, .max_code_points = 2048, .max_lines = 8};
inline constexpr TextLimits kReferenceLimits{.max_bytes = 1024, .max_code_points = 512, .max_lines = 1};
inline constexpr TextLimits kDetailLimits{.max_bytes = 2048, .max_code_points = 1024, .max_lines = 4};

QString normalizeExternalText(const QString& value, TextLimits limits);
QVariantMap safeRequesterDetails(const QMap<QString, QString>& details);

}  // namespace Holonight::Authentication

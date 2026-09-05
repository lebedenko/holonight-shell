#include "ExternalText.h"

#include <QStringList>

namespace Holonight::Authentication {
namespace {
constexpr QChar kReplacement{0xFFFD};
constexpr QChar kTruncation{0x2026};

bool isDisallowedControl(QChar character) {
  const ushort value = character.unicode();
  return value == 0 || (value < 0x20 && value != '\n' && value != '\t') || (value >= 0x7F && value <= 0x9F);
}

bool isPathLike(const QString& value) {
  const QString lowered = value.toLower();
  return value.startsWith('/') || value.startsWith('~') || value.contains("\\") || value.contains("../") ||
         lowered.startsWith("file:") || lowered.endsWith(".exe") || lowered.endsWith(".sh") ||
         lowered.contains("/bin/");
}

QString normalizedCharacter(QChar character, const QString& value, qsizetype& index) {
  QString result(character);
  if (character.isHighSurrogate() && index < value.size() && value.at(index).isLowSurrogate()) {
    result += value.at(index++);
  } else if (character.isSurrogate()) {
    result = kReplacement;
  }
  return result;
}

void appendTruncationMarker(QString& value, TextLimits limits) {
  while (!value.isEmpty() && (value.toUcs4().size() + 1 > limits.max_code_points ||
                              (value + kTruncation).toUtf8().size() > limits.max_bytes)) {
    value.chop(1);
    if (!value.isEmpty() && value.back().isHighSurrogate()) {
      value.chop(1);
    }
  }
  if (limits.max_bytes >= 3 && limits.max_code_points >= 1) {
    value += kTruncation;
  }
}
}  // namespace

QString normalizeExternalText(const QString& value, TextLimits limits) {
  if (limits.max_bytes <= 0 || limits.max_code_points <= 0 || limits.max_lines <= 0) {
    return {};
  }
  QString normalized;
  normalized.reserve(qMin(value.size(), limits.max_code_points));
  bool previous_blank = false;
  qsizetype lines = 1;
  for (qsizetype index = 0; index < value.size();) {
    QChar character = value.at(index++);
    if (character == '\r') {
      if (index < value.size() && value.at(index) == '\n') {
        ++index;
      }
      character = '\n';
    }
    if (isDisallowedControl(character)) {
      character = kReplacement;
    }
    if (character == '\n') {
      const bool blank = normalized.isEmpty() || normalized.endsWith('\n');
      if (lines >= limits.max_lines) {
        appendTruncationMarker(normalized, limits);
        break;
      }
      if (blank && previous_blank) {
        continue;
      }
      previous_blank = blank;
      ++lines;
    } else if (!character.isSpace()) {
      previous_blank = false;
    }

    const QString character_text = normalizedCharacter(character, value, index);
    QString candidate = normalized + character_text;
    if (candidate.toUcs4().size() > limits.max_code_points || candidate.toUtf8().size() > limits.max_bytes) {
      appendTruncationMarker(normalized, limits);
      break;
    }
    normalized = std::move(candidate);
  }
  return normalized;
}

QVariantMap safeRequesterDetails(const QMap<QString, QString>& details) {
  QVariantMap result;
  for (const QString& key : {QStringLiteral("application"), QStringLiteral("vendor")}) {
    const auto found = details.constFind(key);
    if (found == details.cend() || isPathLike(*found)) {
      continue;
    }
    result.insert(key, normalizeExternalText(*found, kDetailLimits));
  }
  return result;
}
}  // namespace Holonight::Authentication

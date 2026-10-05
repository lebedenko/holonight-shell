#include "ShellHyprlandIpc.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QLoggingCategory>
#include <QRegularExpression>

#include <cmath>
#include <limits>

Q_LOGGING_CATEGORY(lcShellHyprlandIpc, "holonight.hyprland.ipc")

std::optional<ShellHyprlandKeyboardLayout> parseShellHyprlandKeyboardLayoutEvent(const QByteArray& line) {
  constexpr QByteArrayView kPrefix{"activelayout>>"};
  if (!line.startsWith(kPrefix)) {
    return std::nullopt;
  }

  const QByteArray payload = line.sliced(kPrefix.size());
  const qsizetype comma = payload.indexOf(',');
  if (comma < 0) {
    return std::nullopt;
  }

  return ShellHyprlandKeyboardLayout{
      .keyboard_name = QString::fromUtf8(payload.left(comma)),
      .layout_name = QString::fromUtf8(payload.sliced(comma + 1)),
  };
}

std::optional<QString> parseShellHyprlandKeyboardLayoutDevicesJson(const QByteArray& response) {
  const QJsonDocument doc = QJsonDocument::fromJson(response);
  if (!doc.isObject()) {
    qCWarning(lcShellHyprlandIpc) << "parseShellHyprlandKeyboardLayoutDevicesJson: expected JSON object";
    return std::nullopt;
  }

  const QJsonArray keyboards = doc.object().value(QStringLiteral("keyboards")).toArray();
  QString first_layout;
  for (const auto& value : keyboards) {
    if (!value.isObject()) {
      continue;
    }

    const QJsonObject keyboard = value.toObject();
    QString layout = keyboard.value(QStringLiteral("active_keymap")).toString();
    if (layout.isEmpty()) {
      continue;
    }
    if (first_layout.isEmpty()) {
      first_layout = layout;
    }
    if (keyboard.value(QStringLiteral("main")).toBool(false)) {
      return layout;
    }
  }

  if (!first_layout.isEmpty()) {
    return first_layout;
  }
  return std::nullopt;
}

QString shellKeyboardLayoutCode(const QString& layout_name) {
  const QString normalized = layout_name.trimmed().toLower();
  if (normalized.isEmpty()) {
    return {};
  }

  static const QHash<QString, QString> kKnownCodes = {
      {QStringLiteral("arabic"), QStringLiteral("AR")},     {QStringLiteral("armenian"), QStringLiteral("HY")},
      {QStringLiteral("belarusian"), QStringLiteral("BE")}, {QStringLiteral("bulgarian"), QStringLiteral("BG")},
      {QStringLiteral("chinese"), QStringLiteral("ZH")},    {QStringLiteral("croatian"), QStringLiteral("HR")},
      {QStringLiteral("czech"), QStringLiteral("CS")},      {QStringLiteral("danish"), QStringLiteral("DA")},
      {QStringLiteral("dutch"), QStringLiteral("NL")},      {QStringLiteral("english"), QStringLiteral("EN")},
      {QStringLiteral("estonian"), QStringLiteral("ET")},   {QStringLiteral("finnish"), QStringLiteral("FI")},
      {QStringLiteral("french"), QStringLiteral("FR")},     {QStringLiteral("georgian"), QStringLiteral("KA")},
      {QStringLiteral("german"), QStringLiteral("DE")},     {QStringLiteral("greek"), QStringLiteral("EL")},
      {QStringLiteral("hebrew"), QStringLiteral("HE")},     {QStringLiteral("hungarian"), QStringLiteral("HU")},
      {QStringLiteral("italian"), QStringLiteral("IT")},    {QStringLiteral("japanese"), QStringLiteral("JA")},
      {QStringLiteral("kazakh"), QStringLiteral("KK")},     {QStringLiteral("korean"), QStringLiteral("KO")},
      {QStringLiteral("latvian"), QStringLiteral("LV")},    {QStringLiteral("lithuanian"), QStringLiteral("LT")},
      {QStringLiteral("norwegian"), QStringLiteral("NO")},  {QStringLiteral("polish"), QStringLiteral("PL")},
      {QStringLiteral("portuguese"), QStringLiteral("PT")}, {QStringLiteral("romanian"), QStringLiteral("RO")},
      {QStringLiteral("russian"), QStringLiteral("RU")},    {QStringLiteral("serbian"), QStringLiteral("SR")},
      {QStringLiteral("slovak"), QStringLiteral("SK")},     {QStringLiteral("spanish"), QStringLiteral("ES")},
      {QStringLiteral("swedish"), QStringLiteral("SV")},    {QStringLiteral("turkish"), QStringLiteral("TR")},
      {QStringLiteral("ukrainian"), QStringLiteral("UK")},
  };

  static const QRegularExpression kWordBoundary(QStringLiteral("[\\s(]"));
  const QString first_word = normalized.section(kWordBoundary, 0, 0);
  const auto known = kKnownCodes.constFind(first_word);
  if (known != kKnownCodes.constEnd()) {
    return *known;
  }

  QString letters;
  for (const QChar chr : normalized) {
    if (chr.isLetter()) {
      letters.append(chr);
      if (letters.size() == 2) {
        return letters.toUpper();
      }
    }
  }
  return normalized.left(2).toUpper();
}

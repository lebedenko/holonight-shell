#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace CategoryMapper {

[[nodiscard]] inline QString map(const QString& categories) {
  static const QHash<QString, QString> kTable = {
      {QStringLiteral("AudioVideo"), QStringLiteral("Multimedia")},
      {QStringLiteral("Audio"), QStringLiteral("Multimedia")},
      {QStringLiteral("Video"), QStringLiteral("Multimedia")},
      {QStringLiteral("Development"), QStringLiteral("Development")},
      {QStringLiteral("Education"), QStringLiteral("Education")},
      {QStringLiteral("Game"), QStringLiteral("Games")},
      {QStringLiteral("Graphics"), QStringLiteral("Graphics")},
      {QStringLiteral("Network"), QStringLiteral("Internet")},
      {QStringLiteral("Internet"), QStringLiteral("Internet")},
      {QStringLiteral("Office"), QStringLiteral("Office")},
      {QStringLiteral("Science"), QStringLiteral("Science")},
      {QStringLiteral("Settings"), QStringLiteral("Settings")},
      {QStringLiteral("System"), QStringLiteral("System")},
      {QStringLiteral("Utility"), QStringLiteral("System")},
      {QStringLiteral("Utilities"), QStringLiteral("System")},
  };

  for (const QString& token : categories.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
    const auto found = kTable.find(token.trimmed());
    if (found != kTable.end()) {
      return found.value();
    }
  }
  return QStringLiteral("Other");
}

[[nodiscard]] inline QStringList curatedOrder() {
  return {
      QStringLiteral("All"),      QStringLiteral("Development"), QStringLiteral("Education"),  QStringLiteral("Games"),
      QStringLiteral("Graphics"), QStringLiteral("Internet"),    QStringLiteral("Multimedia"), QStringLiteral("Office"),
      QStringLiteral("Other"),    QStringLiteral("Science"),     QStringLiteral("Settings"),   QStringLiteral("System"),
  };
}

}  // namespace CategoryMapper

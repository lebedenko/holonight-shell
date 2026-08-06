#include "SysfsValueReader.h"

#include <QFile>
#include <QLoggingCategory>
#include <QTextStream>

Q_LOGGING_CATEGORY(lcSysfsValueReader, "holonight.brightness")

std::optional<int> readSysfsInteger(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qCWarning(lcSysfsValueReader) << "Cannot read sysfs value from" << path;
    return std::nullopt;
  }

  int value = 0;
  QTextStream stream(&file);
  stream >> value;
  if (stream.status() != QTextStream::Ok) {
    qCWarning(lcSysfsValueReader) << "Invalid numeric sysfs value in" << path;
    return std::nullopt;
  }

  return value;
}

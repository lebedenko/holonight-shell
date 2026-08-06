#include "DesktopFileUtils.h"

#include <QDir>
#include <QDirIterator>

namespace DesktopFileUtils {

bool containsDesktopFiles(const QString& directory_path) {
  QDirIterator iter(directory_path, {QStringLiteral("*.desktop")}, QDir::Files, QDirIterator::Subdirectories);
  return iter.hasNext();
}

}  // namespace DesktopFileUtils

#include "ApplicationCacheRebuilder.h"

#include "DesktopFileUtils.h"

#include <QDir>
#include <QFileDevice>
#include <QFileInfo>

namespace {
bool hasWritePermissionBit(const QFileInfo& info) {
  const QFileDevice::Permissions permissions = info.permissions();
  return permissions.testFlag(QFileDevice::WriteOwner) || permissions.testFlag(QFileDevice::WriteGroup) ||
         permissions.testFlag(QFileDevice::WriteOther);
}

ApplicationCacheRebuildStep missingExecutableStep(const QString& program) {
  return {.program = program,
          .arguments = {},
          .exit_code = -1,
          .stderr_text = QStringLiteral("executable not found"),
          .success = false};
}
}  // namespace

ApplicationCacheRebuilder::ApplicationCacheRebuilder(const ISessionIntegrationCommandRunner& command_runner)
    : command_runner_(&command_runner) {}

QVector<ApplicationCacheRebuildStep> ApplicationCacheRebuilder::rebuild(const QStringList& application_dirs) const {
  QVector<ApplicationCacheRebuildStep> steps;
  const QStringList writable_dirs = writableApplicationDirs(application_dirs);

  if (!writable_dirs.isEmpty() && !command_runner_->executableExists(QStringLiteral("update-desktop-database"))) {
    steps.append(missingExecutableStep(QStringLiteral("update-desktop-database")));
  } else {
    for (const QString& dir_path : writable_dirs) {
      const QStringList arguments{dir_path};
      const SessionIntegrationCommandResult result =
          command_runner_->run(QStringLiteral("update-desktop-database"), arguments);
      steps.append({.program = QStringLiteral("update-desktop-database"),
                    .arguments = arguments,
                    .exit_code = result.exit_code,
                    .stderr_text = result.stderr_text,
                    .success = result.exit_code == 0});
    }
  }

  if (command_runner_->executableExists(QStringLiteral("kbuildsycoca6"))) {
    const QStringList arguments{QStringLiteral("--noincremental")};
    const SessionIntegrationCommandResult result = command_runner_->run(QStringLiteral("kbuildsycoca6"), arguments);
    steps.append({.program = QStringLiteral("kbuildsycoca6"),
                  .arguments = arguments,
                  .exit_code = result.exit_code,
                  .stderr_text = result.stderr_text,
                  .success = result.exit_code == 0});
  }

  return steps;
}

QStringList ApplicationCacheRebuilder::writableApplicationDirs(const QStringList& application_dirs) {
  QStringList dirs;
  for (const QString& dir_path : application_dirs) {
    const QFileInfo info(dir_path);
    if (!info.exists() || !info.isDir() || !info.isWritable() || !hasWritePermissionBit(info) ||
        !DesktopFileUtils::containsDesktopFiles(dir_path)) {
      continue;
    }
    const QString clean_path = QDir::cleanPath(info.absoluteFilePath());
    if (!dirs.contains(clean_path)) {
      dirs.append(clean_path);
    }
  }
  return dirs;
}

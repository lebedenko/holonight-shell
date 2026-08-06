#pragma once

#include "SessionIntegrationService.h"

#include <QString>
#include <QStringList>
#include <QVector>

class ApplicationCacheRebuilder {
 public:
  explicit ApplicationCacheRebuilder(const ISessionIntegrationCommandRunner& command_runner);

  [[nodiscard]] QVector<ApplicationCacheRebuildStep> rebuild(const QStringList& application_dirs) const;

 private:
  [[nodiscard]] static QStringList writableApplicationDirs(const QStringList& application_dirs);

  const ISessionIntegrationCommandRunner* command_runner_;
};

#pragma once

#include "DesktopEntryScanner.h"

#include <QString>
#include <QStringList>

struct LauncherCommand {
  QString program;
  QStringList arguments;
  QString working_dir;

  [[nodiscard]] bool isValid() const { return !program.isEmpty(); }
};

[[nodiscard]] LauncherCommand commandForDesktopEntry(const DesktopEntry& entry, const QString& terminal_emulator = {});
[[nodiscard]] LauncherCommand commandForExec(const QString& exec, const QString& working_dir);

#include "LauncherCommand.h"

#include <QChar>

namespace {

QStringList splitExecCommand(const QString& command) {
  QStringList result;
  QString current;
  QChar quote;

  for (int index = 0; index < command.size(); ++index) {
    const QChar chr = command.at(index);
    if (chr == QLatin1Char('\\') && index + 1 < command.size()) {
      current.append(command.at(index + 1));
      ++index;
      continue;
    }

    if (quote.isNull() && (chr == QLatin1Char('"') || chr == QLatin1Char('\''))) {
      quote = chr;
      continue;
    }
    if (!quote.isNull() && chr == quote) {
      quote = {};
      continue;
    }

    if (quote.isNull() && chr.isSpace()) {
      if (!current.isEmpty()) {
        result.append(current);
        current.clear();
      }
      continue;
    }

    current.append(chr);
  }

  if (!current.isEmpty()) {
    result.append(current);
  }
  return result;
}

LauncherCommand commandFromExecString(const QString& exec, const QString& working_dir) {
  const QString command = stripDesktopExecFieldCodes(exec);
  QStringList arguments = splitExecCommand(command);
  if (arguments.isEmpty()) {
    return {};
  }

  LauncherCommand result;
  result.program = arguments.takeFirst();
  result.arguments = arguments;
  result.working_dir = working_dir;
  return result;
}

}  // namespace

LauncherCommand commandForDesktopEntry(const DesktopEntry& entry, const QString& terminal_emulator) {
  LauncherCommand command = commandFromExecString(entry.exec, entry.path);
  if (!command.isValid() || !entry.terminal || terminal_emulator.isEmpty()) {
    return command;
  }

  QStringList terminal_args;
  terminal_args << QStringLiteral("-e") << command.program;
  terminal_args.append(command.arguments);
  command.program = terminal_emulator;
  command.arguments = terminal_args;
  return command;
}

LauncherCommand commandForExec(const QString& exec, const QString& working_dir) {
  return commandFromExecString(exec, working_dir);
}

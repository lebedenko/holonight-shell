#include "Logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>

#include <iostream>
#include <memory>
#include <print>
#include <unistd.h>

namespace holonight::logger {

namespace {

struct LogContext {
  std::unique_ptr<QFile> log_file;
  QMutex log_mutex;
  QString log_file_path;
  QtMsgType console_log_level{QtWarningMsg};
  bool is_console_colored{false};
};

LogContext& getContext() {
  static LogContext context;
  return context;
}

QString msgTypeToString(QtMsgType type) {
  switch (type) {
    case QtDebugMsg:
      return QStringLiteral("DEBUG");
    case QtInfoMsg:
      return QStringLiteral("INFO");
    case QtWarningMsg:
      return QStringLiteral("WARNING");
    case QtCriticalMsg:
      return QStringLiteral("CRITICAL");
    case QtFatalMsg:
      return QStringLiteral("FATAL");
  }
  return QStringLiteral("UNKNOWN");
}

QString msgTypeColorCode(QtMsgType type) {
  switch (type) {
    case QtDebugMsg:
      return QStringLiteral("\033[90m");  // Gray
    case QtInfoMsg:
      return QStringLiteral("\033[34m");  // Blue
    case QtWarningMsg:
      return QStringLiteral("\033[33m");  // Yellow
    case QtCriticalMsg:
      return QStringLiteral("\033[31m");  // Red
    case QtFatalMsg:
      return QStringLiteral("\033[1;31m");  // Bold Red
  }
  return QStringLiteral("\033[0m");
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
  LogContext& ctx = getContext();

  const QString timeStr = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
  const QString levelStr = msgTypeToString(type);
  const QString categoryStr = QString::fromUtf8(context.category != nullptr ? context.category : "default");

  // Format line/file information cleanly
  QString fileLineStr;
  if (context.file != nullptr) {
    const QString file = QString::fromUtf8(context.file);
    const qsizetype lastSlash = file.lastIndexOf(QLatin1Char('/'));
    const QString fileName = lastSlash == -1 ? file : file.mid(static_cast<int>(lastSlash) + 1);
    fileLineStr = QStringLiteral(" [%1:%2]").arg(fileName).arg(context.line);
  }

  // Format message for file output
  const QString fileLogMsg =
      QStringLiteral("[%1] [%2] [%3]%4: %5\n").arg(timeStr).arg(levelStr).arg(categoryStr).arg(fileLineStr).arg(msg);

  // Write to the log file (thread-safe)
  {
    QMutexLocker locker(&ctx.log_mutex);
    if (ctx.log_file != nullptr && ctx.log_file->isOpen()) {
      QTextStream stream(ctx.log_file.get());
      stream << fileLogMsg;
      stream.flush();
    }
  }

  // Convert QtMsgType to a sequential severity integer for filtering comparison
  auto severity = [](QtMsgType msg_type) -> int {
    switch (msg_type) {
      case QtDebugMsg:
        return 0;
      case QtInfoMsg:
        return 1;
      case QtWarningMsg:
        return 2;
      case QtCriticalMsg:
        return 3;
      case QtFatalMsg:
        return 4;
    }
    return 0;
  };

  if (severity(type) >= severity(ctx.console_log_level)) {
    QString consoleLogMsg;
    if (ctx.is_console_colored) {
      const QString color = msgTypeColorCode(type);
      const QString reset = QStringLiteral("\033[0m");
      consoleLogMsg = QStringLiteral("%1[%2]%3 [%4]%5: %6")
                          .arg(color)
                          .arg(levelStr)
                          .arg(reset)
                          .arg(categoryStr)
                          .arg(fileLineStr)
                          .arg(msg);
    } else {
      consoleLogMsg = QStringLiteral("[%1] [%2]%3: %4").arg(levelStr).arg(categoryStr).arg(fileLineStr).arg(msg);
    }

    if (type == QtDebugMsg || type == QtInfoMsg) {
      std::println(stdout, "{}", consoleLogMsg.toStdString());
    } else {
      std::println(stderr, "{}", consoleLogMsg.toStdString());
    }
  }
}

void applyLogLevel(LogContext& ctx, const QString& level) {
  if (level == QStringLiteral("debug")) {
    ctx.console_log_level = QtDebugMsg;
  } else if (level == QStringLiteral("info")) {
    ctx.console_log_level = QtInfoMsg;
  } else if (level == QStringLiteral("warning") || level == QStringLiteral("warn")) {
    ctx.console_log_level = QtWarningMsg;
  } else if (level == QStringLiteral("critical") || level == QStringLiteral("error")) {
    ctx.console_log_level = QtCriticalMsg;
  } else if (level == QStringLiteral("fatal")) {
    ctx.console_log_level = QtFatalMsg;
  }
}

void parseArgLogLevel(LogContext& ctx, int argc, char** argv) {
  for (int index = 1; index < argc; ++index) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const QString arg = QString::fromLocal8Bit(argv[index]);
    if (arg == QStringLiteral("--verbose") || arg == QStringLiteral("-v")) {
      ctx.console_log_level = QtInfoMsg;
    } else if (arg == QStringLiteral("--debug") || arg == QStringLiteral("-d")) {
      ctx.console_log_level = QtDebugMsg;
    } else if (arg.startsWith(QStringLiteral("--log-level="))) {
      applyLogLevel(ctx, arg.mid(12).toLower());
    }
  }
}

QString parseArgLogFile(int argc, char** argv, bool& file_logging_enabled) {
  QString custom_log_path;
  for (int index = 1; index < argc; ++index) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const QString arg = QString::fromLocal8Bit(argv[index]);
    if (arg.startsWith(QStringLiteral("--log-file="))) {
      custom_log_path = arg.mid(11);
    } else if (arg == QStringLiteral("--no-log-file")) {
      file_logging_enabled = false;
    }
  }
  return custom_log_path;
}

void openLogFile(LogContext& ctx, const QString& log_file_path) {
  ctx.log_file = std::make_unique<QFile>(log_file_path);
  if (ctx.log_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    QTextStream stream(ctx.log_file.get());
    stream << "\n=== Holonight Shell session started at "
           << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")) << " ===\n";
    stream.flush();
  } else {
    std::cerr << "Failed to open log file for writing: " << log_file_path.toStdString() << '\n';
    ctx.log_file = nullptr;
  }
}

}  // namespace

void initialize(int argc, char** argv) {
  LogContext& ctx = getContext();

  bool file_logging_enabled = true;
  parseArgLogLevel(ctx, argc, argv);
  const QString custom_log_path = parseArgLogFile(argc, argv, file_logging_enabled);

  // Fallback to environment variable if set
  const QByteArray env_log_level = qgetenv("HOLONIGHT_LOG_LEVEL");
  if (!env_log_level.isEmpty()) {
    applyLogLevel(ctx, QString::fromLocal8Bit(env_log_level).toLower());
  }

  // Detect whether console is interactive and supports colors
  ctx.is_console_colored = (isatty(STDERR_FILENO) != 0);

  // Setup file logging if requested
  if (file_logging_enabled) {
    if (custom_log_path.isEmpty()) {
      // Ensure application metadata is set so standard paths resolve nicely
      QCoreApplication::setApplicationName(QStringLiteral("holonight-shell"));
      QCoreApplication::setOrganizationName(QStringLiteral("holonight"));

      const QString log_dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
      ctx.log_file_path = log_dir + QStringLiteral("/holonight.log");
    } else {
      ctx.log_file_path = custom_log_path;
    }

    // Ensure the log directory exists
    const QDir log_dir(QFileInfo(ctx.log_file_path).absolutePath());
    if (!log_dir.exists()) {
      log_dir.mkpath(QStringLiteral("."));
    }

    // Rotate existing log file if it exceeds 10MB
    QFileInfo file_info(ctx.log_file_path);
    if (file_info.exists() && file_info.size() > 10 * 1024 * 1024) {
      const QString old_path = ctx.log_file_path + QStringLiteral(".old");
      QFile::remove(old_path);
      QFile::rename(ctx.log_file_path, old_path);
    }

    openLogFile(ctx, ctx.log_file_path);
  }

  qInstallMessageHandler(messageHandler);
}

void shutdown() {
  LogContext& ctx = getContext();
  QMutexLocker locker(&ctx.log_mutex);

  qInstallMessageHandler(nullptr);

  if (ctx.log_file != nullptr) {
    if (ctx.log_file->isOpen()) {
      QTextStream stream(ctx.log_file.get());
      stream << "=== Holonight Shell session ended at "
             << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")) << " ===\n";
      stream.flush();
      ctx.log_file->close();
    }
    ctx.log_file = nullptr;
  }
}

QString logFilePath() { return getContext().log_file_path; }

}  // namespace holonight::logger

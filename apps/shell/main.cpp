#include "Logger.h"
#include "app/ControlServer.h"
#include "app/ShellApplication.h"
#include "version.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocalSocket>

#include <print>

namespace {
// Qt application entry points require the traditional argc/argv pair.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
int sendControlCommand(int argc, char* argv[], const QByteArray& command) {
  QCoreApplication app(argc, argv);
  QLocalSocket socket;
  socket.connectToServer(ControlServer::socketPath(), QIODevice::WriteOnly);
  if (!socket.waitForConnected(500)) {
    qWarning("holonight-shell: failed to connect to running shell control socket");
    return 1;
  }
  socket.write(command);
  socket.flush();
  socket.waitForBytesWritten(500);
  socket.disconnectFromServer();
  return 0;
}
}  // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
int main(int argc, char* argv[]) {
  // Parse help and version before initializing the full log system to keep outputs simple
  for (int index = 1; index < argc; ++index) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const QString arg = QString::fromLocal8Bit(argv[index]);
    if (arg == QStringLiteral("--help") || arg == QStringLiteral("-h")) {
      std::println(stdout, "Usage: holonight-shell [options]");
      std::println(stdout, "Options:");
      std::println(stdout, "  -h, --help             Show this help message");
      std::println(stdout, "  -V, --version          Show version information");
      std::println(stdout, "  --toggle-launcher      Toggle the desktop launcher popup");
      std::println(stdout, "  -v, --verbose          Show info and warning logs on terminal");
      std::println(stdout, "  -d, --debug            Show all logs (including debug) on terminal");
      std::println(stdout, "  --log-level=<level>    Set terminal log level (debug, info, warning, critical, fatal)");
      std::println(stdout, "  --log-file=<path>      Write logs to the specified file");
      std::println(stdout, "  --no-log-file          Disable logging to file");
      return 0;
    }
  }

  // Initialize the logging system early
  holonight::logger::initialize(argc, argv);

  for (int index = 1; index < argc; ++index) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const QString arg = QString::fromLocal8Bit(argv[index]);
    if (arg == QStringLiteral("--toggle-launcher")) {
      int result = sendControlCommand(argc, argv, QByteArrayLiteral("toggle-launcher"));
      holonight::logger::shutdown();
      return result;
    }
    if (arg == QStringLiteral("--version") || arg == QStringLiteral("-V")) {
      std::println(stdout, "holonight-shell {}", holonight::shellVersion);
      holonight::logger::shutdown();
      return 0;
    }
  }

  QGuiApplication app(argc, argv);
  ShellApplication shell(&app);
  shell.registerQmlTypes();
  shell.startServices();
  shell.startShell();

  int result = QGuiApplication::exec();
  holonight::logger::shutdown();
  return result;
}

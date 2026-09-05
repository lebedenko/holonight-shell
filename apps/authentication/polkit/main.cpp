#include "AccountProfileResolver.h"
#include "PolkitListenerBridge.h"
#include "PolkitSessionAdapter.h"
#include "generated/AuthenticationConfig.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QSocketNotifier>

#include <csignal>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/signalfd.h>
#include <unistd.h>

using namespace Holonight::Authentication;

namespace {
bool disableCoreDumps() {
  const rlimit limit{.rlim_cur = 0, .rlim_max = 0};
  // RLIMIT_CORE does not stop piped crash collectors on Linux.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) -- POSIX/Linux API boundary.
  return setrlimit(RLIMIT_CORE, &limit) == 0 && prctl(PR_SET_DUMPABLE, 0L) == 0;
}
}  // namespace

int main(int argc, char* argv[]) {
  if (!disableCoreDumps()) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) -- POSIX/Linux API boundary.
    dprintf(STDERR_FILENO, "frontend=polkit phase=startup classification=core-limit\n");
    return 1;
  }
  const QByteArray session_id = qgetenv("XDG_SESSION_ID");
  if (session_id.isEmpty() || session_id.contains('\n') || session_id.contains('\0')) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) -- POSIX/Linux API boundary.
    dprintf(STDERR_FILENO, "frontend=polkit phase=startup classification=invalid-session\n");
    return 1;
  }

  sigset_t mask{};
  sigemptyset(&mask);
  sigaddset(&mask, SIGTERM);
  const int signal_fd =
      sigprocmask(SIG_BLOCK, &mask, nullptr) == 0 ? signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK) : -1;
  if (signal_fd < 0) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) -- POSIX/Linux API boundary.
    dprintf(STDERR_FILENO, "frontend=polkit phase=startup classification=signal-setup\n");
    return 1;
  }
  QGuiApplication application(argc, argv);
  QGuiApplication::setQuitOnLastWindowClosed(false);
  QSocketNotifier notifier(signal_fd, QSocketNotifier::Read);
  QObject::connect(&notifier, &QSocketNotifier::activated, &application, [&] {
    signalfd_siginfo info{};
    (void)read(signal_fd, &info, sizeof(info));
    // Window-manager close requests are vetoed to preserve the persistent
    // dialog. Service termination must still exit and run aboutToQuit cleanup.
    QGuiApplication::exit(0);
  });
  AuthenticationPromptModel model;
  PolkitRequestCoordinator coordinator(&model, static_cast<uint>(getuid()), createPolkitSession);
  AccountProfileResolver profiles(&model);
  QObject::connect(&coordinator, &PolkitRequestCoordinator::requestPresented, &profiles,
                   &AccountProfileResolver::resolveCurrentRequest);
  PolkitListenerBridge bridge(
      [&coordinator](const PolkitRequest& request) {
        if (coordinator.enqueue(request)) {
          return;
        }
        request.complete(false);
      },
      [&coordinator](const QString& token) { coordinator.cancel(token); });

  QString registration_error;
  if (!bridge.registerForSession(session_id, &registration_error)) {
    const bool conflict = PolkitListenerBridge::registrationErrorIsConflict(registration_error);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) -- POSIX/Linux API boundary.
    dprintf(STDERR_FILENO, "frontend=polkit phase=registration classification=%s\n", conflict ? "conflict" : "failure");
    if (conflict) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) -- POSIX/Linux API boundary.
      dprintf(STDERR_FILENO,
              "Another authentication agent owns this session; leave it running or choose another session.\n");
    }
    return conflict ? kHolonightPolkitConflictExitStatus : 1;
  }

  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("promptModel"), QVariant::fromValue(&model)}});
  engine.load(QUrl(QStringLiteral("qrc:/qt-project.org/imports/Holonight/Authentication/AuthenticationDialog.qml")));
  if (engine.rootObjects().isEmpty()) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) -- POSIX/Linux API boundary.
    dprintf(STDERR_FILENO, "frontend=polkit phase=ui classification=load-failure\n");
    return 1;
  }
  QObject::connect(&application, &QCoreApplication::aboutToQuit, &coordinator, &PolkitRequestCoordinator::shutdown);
  return QGuiApplication::exec();
}

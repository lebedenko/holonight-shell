#include "AskpassMode.h"
#include "ExternalText.h"
#include "ProtocolWriter.h"
#include "SecretValidator.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSocketNotifier>

#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <span>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/signalfd.h>
#include <unistd.h>

using Holonight::Authentication::AuthenticationPromptModel;

namespace {
bool disableCoreDumps() {
  const rlimit limit{.rlim_cur = 0, .rlim_max = 0};
  // RLIMIT_CORE does not stop piped crash collectors on Linux.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) -- POSIX/Linux API boundary.
  return setrlimit(RLIMIT_CORE, &limit) == 0 && prctl(PR_SET_DUMPABLE, 0L) == 0;
}
int setupSignalFd() {
  sigset_t mask{};
  sigemptyset(&mask);
  sigaddset(&mask, SIGTERM);
  return sigprocmask(SIG_BLOCK, &mask, nullptr) == 0 ? signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK) : -1;
}
#ifdef HOLONIGHT_ASKPASS_TEST_CONTROL
constexpr int kTestControlFd = 3;

class TestControl final : public QObject {
 public:
  explicit TestControl(AuthenticationPromptModel* model) : model_(model) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) -- POSIX descriptor flags.
    const int flags = fcntl(kTestControlFd, F_GETFL);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) -- POSIX descriptor flags.
    if (flags < 0 || fcntl(kTestControlFd, F_SETFL, flags | O_NONBLOCK) < 0) {
      return;
    }
    notifier_ = std::make_unique<QSocketNotifier>(kTestControlFd, QSocketNotifier::Read, this);
    connect(notifier_.get(), &QSocketNotifier::activated, this, [this] { consume(); });
  }

 private:
  void consume() {
    std::array<char, 4096> chunk{};
    while (true) {
      const ssize_t count = read(kTestControlFd, chunk.data(), chunk.size());
      if (count > 0) {
        buffer_.append(chunk.data(), count);
        continue;
      }
      if (count < 0 && errno == EINTR) {
        continue;
      }
      break;
    }
    while (buffer_.size() >= 9) {
      if (std::memcmp(buffer_.constData(), "HNAP", 4) != 0) {
        QCoreApplication::exit(1);
        notifier_->setEnabled(false);
        return;
      }
      const auto byte = [this](qsizetype index) {
        return static_cast<quint32>(static_cast<unsigned char>(buffer_.at(index)));
      };
      const quint32 size = (byte(5) << 24U) | (byte(6) << 16U) | (byte(7) << 8U) | byte(8);
      if (size > 4096) {
        QCoreApplication::exit(1);
        notifier_->setEnabled(false);
        return;
      }
      if (buffer_.size() < 9 + static_cast<qsizetype>(size)) {
        return;
      }
      const char command = buffer_.at(4);
      const QByteArray payload = buffer_.mid(9, size);
      buffer_.remove(0, 9 + size);
      if (command == 'T') {
        model_->respond(QString::fromUtf8(payload));
      } else if (command == 'A') {
        model_->confirm(true);
      } else if (command == 'R') {
        model_->confirm(false);
      } else if (command == 'C') {
        model_->cancel();
      } else if (command == 'K') {
        model_->acknowledge();
      } else if (command == 'X') {
        std::abort();
      } else if (command == 'Y') {
        model_->retry();
      } else {
        QCoreApplication::exit(1);
      }
    }
  }

  AuthenticationPromptModel* model_;
  QByteArray buffer_;
  std::unique_ptr<QSocketNotifier> notifier_;
};
#endif
}  // namespace

int main(int argc, char* argv[]) {
  if (!disableCoreDumps()) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) -- POSIX/Linux API boundary.
    dprintf(STDERR_FILENO, "frontend=askpass phase=startup classification=core-limit\n");
    return 1;
  }
  if (argc > 2) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) -- POSIX/Linux API boundary.
    dprintf(STDERR_FILENO, "frontend=askpass phase=arguments classification=extra-arguments\n");
    return 1;
  }
  std::signal(SIGPIPE, SIG_IGN);
  const int signal_fd = setupSignalFd();
  if (signal_fd < 0) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) -- POSIX/Linux API boundary.
    dprintf(STDERR_FILENO, "frontend=askpass phase=startup classification=signal-setup\n");
    return 1;
  }
  QGuiApplication application(argc, argv);
  const std::span<char*> arguments(argv, static_cast<size_t>(argc));
  const QString basename = QFileInfo(QString::fromLocal8Bit(arguments.front())).fileName();
  const auto mode = Holonight::Authentication::askpassMode(basename, qEnvironmentVariable("SSH_ASKPASS_PROMPT"));
  AuthenticationPromptModel model;
  bool finished = false;
  int result = 1;
  const QString prompt = argc == 2 ? QString::fromLocal8Bit(arguments[1]) : QString{};
  model.beginRequest({.token = QStringLiteral("askpass"),
                      .message = prompt,
                      .input_mode = mode,
                      .frontend_kind = Holonight::Authentication::askpassFrontendKind(basename)},
                     [&](AuthenticationPromptModel::ResponseKind kind, const QString& value) {
                       if (finished) {
                         return;
                       }
                       if (kind == AuthenticationPromptModel::ResponseKind::Text) {
                         const auto validated = Holonight::Authentication::validateSecret(value);
                         if (!validated) {
                           model.markRetryableError({{.severity = Holonight::Authentication::MessageSeverity::Error,
                                                      .text = QStringLiteral("Invalid response")}});
                           return;
                         }
                         result =
                             Holonight::Authentication::writeProtocolSecret(STDOUT_FILENO, validated.bytes) ? 0 : 1;
                       } else if (kind == AuthenticationPromptModel::ResponseKind::Confirmation) {
                         result = value == QStringLiteral("accepted") ? 0 : 1;
                       } else if (kind == AuthenticationPromptModel::ResponseKind::Retry) {
                         model.presentPrompt({}, mode);
                         return;
                       } else {
                         result = 1;
                       }
                       finished = true;
                       model.complete();
                       QGuiApplication::exit(result);
                     });

  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("promptModel"), QVariant::fromValue(&model)}});
  engine.load(QUrl(QStringLiteral("qrc:/qt-project.org/imports/Holonight/Authentication/AuthenticationDialog.qml")));
  if (engine.rootObjects().isEmpty()) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) -- POSIX/Linux API boundary.
    dprintf(STDERR_FILENO, "frontend=askpass phase=ui classification=load-failure\n");
    return 1;
  }
#ifdef HOLONIGHT_ASKPASS_TEST_CONTROL
  TestControl test_control(&model);
#endif
  QSocketNotifier notifier(signal_fd, QSocketNotifier::Read);
  QObject::connect(&notifier, &QSocketNotifier::activated, &application, [&] {
    signalfd_siginfo info{};
    (void)read(signal_fd, &info, sizeof(info));
    model.shutdown();
    QGuiApplication::exit(mode == AuthenticationPromptModel::InputMode::Notification ? 0 : 1);
  });
  const int exit_code = QGuiApplication::exec();
  close(signal_fd);
  return exit_code;
}

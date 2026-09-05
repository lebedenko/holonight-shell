#include <QByteArray>
#include <QString>

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <gtest/gtest.h>
#include <poll.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {
using namespace std::chrono_literals;

struct ProcessResult {
  QByteArray output;
  QByteArray error;
  int status{-1};
  bool core_dumped{};
};

struct ChildProcess {
  pid_t pid{-1};
  int control{-1};
  int output{-1};
  int error{-1};
};

void closeUnless(int descriptor, int preserved) {
  if (descriptor >= 0 && descriptor != preserved) {
    close(descriptor);
  }
}

ChildProcess startAskpass(const char* basename, const char* hint = nullptr, bool extra_argument = false,
                          const char* executable = TEST_ASKPASS_PATH) {
  std::array<int, 2> control_pipe{};
  std::array<int, 2> output_pipe{};
  std::array<int, 2> error_pipe{};
  EXPECT_EQ(pipe(control_pipe.data()), 0);
  EXPECT_EQ(pipe(output_pipe.data()), 0);
  EXPECT_EQ(pipe(error_pipe.data()), 0);
  const pid_t pid = fork();
  EXPECT_GE(pid, 0);
  if (pid == 0) {
    dup2(control_pipe[0], 3);
    dup2(output_pipe[1], STDOUT_FILENO);
    dup2(error_pipe[1], STDERR_FILENO);
    closeUnless(control_pipe[0], 3);
    closeUnless(control_pipe[1], 3);
    closeUnless(output_pipe[0], 3);
    closeUnless(output_pipe[1], 3);
    closeUnless(error_pipe[0], 3);
    closeUnless(error_pipe[1], 3);
    setenv("QT_QPA_PLATFORM", "offscreen", 1);
    setenv("QML_IMPORT_PATH", "/tmp/holonight-qt-prefix/lib/qt6/qml", 1);
    if (hint != nullptr) {
      setenv("SSH_ASKPASS_PROMPT", hint, 1);
    } else {
      unsetenv("SSH_ASKPASS_PROMPT");
    }
    QByteArray entry_name(basename);
    QByteArray prompt("Test prompt");
    QByteArray extra("extra");
    std::array<char*, 4> arguments{entry_name.data(), prompt.data(), extra_argument ? extra.data() : nullptr, nullptr};
    execv(executable, arguments.data());
    _exit(127);
  }
  close(control_pipe[0]);
  close(output_pipe[1]);
  close(error_pipe[1]);
  return {.pid = pid, .control = control_pipe[1], .output = output_pipe[0], .error = error_pipe[0]};
}

QByteArray frame(char command, const QByteArray& payload = {}) {
  const auto size = static_cast<uint32_t>(payload.size());
  QByteArray bytes("HNAP", 4);
  bytes.append(command);
  bytes.append(static_cast<char>((size >> 24U) & 0xffU));
  bytes.append(static_cast<char>((size >> 16U) & 0xffU));
  bytes.append(static_cast<char>((size >> 8U) & 0xffU));
  bytes.append(static_cast<char>(size & 0xffU));
  bytes.append(payload);
  return bytes;
}

void sendFrame(const ChildProcess& child, char command, const QByteArray& payload = {}) {
  const QByteArray bytes = frame(command, payload);
  ASSERT_EQ(write(child.control, bytes.constData(), static_cast<size_t>(bytes.size())), bytes.size());
}

QByteArray readAll(int descriptor) {
  QByteArray result;
  std::array<char, 4096> buffer{};
  while (true) {
    const ssize_t count = read(descriptor, buffer.data(), buffer.size());
    if (count > 0) {
      result.append(buffer.data(), count);
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    break;
  }
  close(descriptor);
  return result;
}

ProcessResult finish(ChildProcess child) {
  close(child.control);
  int wait_status{};
  bool reaped = false;
  for (int attempt = 0; attempt < 500; ++attempt) {
    if (waitpid(child.pid, &wait_status, WNOHANG) == child.pid) {
      reaped = true;
      break;
    }
    std::this_thread::sleep_for(10ms);
  }
  if (!reaped) {
    kill(child.pid, SIGKILL);
    waitpid(child.pid, &wait_status, 0);
  }
  return {.output = readAll(child.output),
          .error = readAll(child.error),
          .status = WIFEXITED(wait_status) ? WEXITSTATUS(wait_status) : 128 + WTERMSIG(wait_status),
          .core_dumped = WCOREDUMP(wait_status) != 0};
}

TEST(AskpassProcess, InstalledTargetIgnoresTestAutomationChannel) {
  auto child = startAskpass("holonight-sudo-askpass", nullptr, false, TEST_PRODUCTION_ASKPASS_PATH);
  sendFrame(child, 'T', QByteArrayLiteral("must-not-be-submitted"));
  pollfd output{.fd = child.output, .events = POLLIN, .revents = 0};
  EXPECT_EQ(poll(&output, 1, 1000), 0);
  kill(child.pid, SIGTERM);
  const auto result = finish(child);
  EXPECT_EQ(result.status, 1);
  EXPECT_TRUE(result.output.isEmpty());
  EXPECT_TRUE(result.error.isEmpty()) << result.error.constData();
}

TEST(AskpassProcess, SecretSuccessCorrectionCancellationAndBoundaries) {
  for (const QByteArray& secret : {QByteArray::fromStdString("p\xC3\xA4ss"), QByteArray(1022, 'x')}) {
    auto child = startAskpass("holonight-sudo-askpass");
    sendFrame(child, 'T', secret);
    const auto result = finish(child);
    EXPECT_EQ(result.status, 0);
    EXPECT_EQ(result.output, secret + '\n');
    EXPECT_TRUE(result.error.isEmpty()) << result.error.constData();
  }

  auto correction = startAskpass("holonight-sudo-askpass");
  sendFrame(correction, 'T', QByteArrayLiteral("bad\nsecret"));
  sendFrame(correction, 'Y');
  sendFrame(correction, 'T', QByteArrayLiteral("correct"));
  const auto corrected = finish(correction);
  EXPECT_EQ(corrected.status, 0);
  EXPECT_EQ(corrected.output, QByteArrayLiteral("correct\n"));
  EXPECT_TRUE(corrected.error.isEmpty()) << corrected.error.constData();

  for (const QByteArray& rejected :
       {QByteArray{}, QByteArrayLiteral("bad\rvalue"), QByteArray("bad\0value", 9), QByteArray(1023, 'x')}) {
    auto child = startAskpass("holonight-sudo-askpass");
    sendFrame(child, 'T', rejected);
    sendFrame(child, 'C');
    const auto result = finish(child);
    EXPECT_EQ(result.status, 1);
    EXPECT_TRUE(result.output.isEmpty());
    EXPECT_TRUE(result.error.isEmpty()) << result.error.constData();
  }
}

TEST(AskpassProcess, BasenamesConfirmationCompatibilityAndNotification) {
  auto confirmation = startAskpass("holonight-ssh-askpass", "confirm");
  sendFrame(confirmation, 'A');
  auto result = finish(confirmation);
  EXPECT_EQ(result.status, 0);
  EXPECT_TRUE(result.output.isEmpty());
  EXPECT_TRUE(result.error.isEmpty()) << result.error.constData();

  auto rejection = startAskpass("holonight-ssh-askpass", "confirm");
  sendFrame(rejection, 'R');
  result = finish(rejection);
  EXPECT_EQ(result.status, 1);
  EXPECT_TRUE(result.output.isEmpty());

  auto compatibility = startAskpass("holonight-askpass", "confirm");
  sendFrame(compatibility, 'A');
  result = finish(compatibility);
  EXPECT_EQ(result.status, 0);
  EXPECT_TRUE(result.output.isEmpty());

  auto notification = startAskpass("holonight-ssh-askpass", "none");
  std::this_thread::sleep_for(100ms);
  EXPECT_EQ(kill(notification.pid, 0), 0);
  kill(notification.pid, SIGTERM);
  result = finish(notification);
  EXPECT_EQ(result.status, 0);
  EXPECT_TRUE(result.output.isEmpty());
  EXPECT_TRUE(result.error.isEmpty()) << result.error.constData();
}

TEST(AskpassProcess, AppliesZeroCoreLimitAndKeepsProtocolStreamsClean) {
  auto child = startAskpass("holonight-sudo-askpass");
  std::this_thread::sleep_for(100ms);
  std::ifstream limits("/proc/" + std::to_string(child.pid) + "/limits");
  std::string line;
  bool found_zero_core_limit = false;
  while (std::getline(limits, line)) {
    if (line.starts_with("Max core file size") && line.contains(" 0 ")) {
      found_zero_core_limit = true;
      break;
    }
  }
  EXPECT_TRUE(found_zero_core_limit);
  sendFrame(child, 'T', QByteArrayLiteral("stream-marker"));
  const auto result = finish(child);
  EXPECT_EQ(result.status, 0);
  EXPECT_EQ(result.output, QByteArrayLiteral("stream-marker\n"));
  EXPECT_TRUE(result.error.isEmpty()) << result.error.constData();
}

TEST(AskpassProcess, IndependentProcessesNeverCrossRouteOrCompleteTwice) {
  auto first = startAskpass("holonight-sudo-askpass", "none");
  auto second = startAskpass("holonight-ssh-askpass", "unknown");
  sendFrame(first, 'T', QByteArrayLiteral("first-marker"));
  sendFrame(first, 'T', QByteArrayLiteral("ignored-marker"));
  sendFrame(second, 'T', QByteArrayLiteral("second-marker"));
  const auto first_result = finish(first);
  const auto second_result = finish(second);
  EXPECT_EQ(first_result.status, 0);
  EXPECT_EQ(second_result.status, 0);
  EXPECT_EQ(first_result.output, QByteArrayLiteral("first-marker\n"));
  EXPECT_EQ(second_result.output, QByteArrayLiteral("second-marker\n"));
  EXPECT_TRUE(first_result.error.isEmpty());
  EXPECT_TRUE(second_result.error.isEmpty());
}

TEST(AskpassProcess, RejectsStartupAndInternalFailuresWithoutProtocolOutput) {
  auto startup = startAskpass("holonight-sudo-askpass", nullptr, true);
  auto result = finish(startup);
  EXPECT_EQ(result.status, 1);
  EXPECT_TRUE(result.output.isEmpty());
  EXPECT_TRUE(result.error.contains("classification=extra-arguments"));
  auto internal = startAskpass("holonight-sudo-askpass");
  sendFrame(internal, '?');
  result = finish(internal);
  EXPECT_EQ(result.status, 1);
  EXPECT_TRUE(result.output.isEmpty());
  EXPECT_TRUE(result.error.isEmpty());
}

TEST(AskpassProcess, IntentionalCrashDoesNotProduceCoreDump) {
  auto child = startAskpass("holonight-sudo-askpass");
  sendFrame(child, 'X');
  const auto result = finish(child);
  EXPECT_EQ(result.status, 128 + SIGABRT);
  EXPECT_FALSE(result.core_dumped);
  EXPECT_TRUE(result.output.isEmpty());
  EXPECT_TRUE(result.error.isEmpty());
}

TEST(AskpassProcess, SupplementaryUnicodeHonorsEncodedByteLimit) {
  const QByteArray face = QByteArray::fromHex("f09f9880");
  auto valid = startAskpass("holonight-askpass");
  const QByteArray secret = face.repeated(255) + "xx";
  sendFrame(valid, 'T', secret);
  const auto accepted = finish(valid);
  EXPECT_EQ(accepted.status, 0);
  EXPECT_EQ(accepted.output, secret + '\n');
  auto invalid = startAskpass("holonight-askpass");
  sendFrame(invalid, 'T', secret + "x");
  sendFrame(invalid, 'C');
  const auto rejected = finish(invalid);
  EXPECT_EQ(rejected.status, 1);
  EXPECT_TRUE(rejected.output.isEmpty());
}

}  // namespace

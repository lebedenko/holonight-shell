#include "HyprlandIpcClient.h"
#include "KeyboardLayoutService.h"

#include <QSignalSpy>

#include <gtest/gtest.h>

// Fake transport: records connectEventStream/runCommand calls and lets
// tests emit signals directly without a live Hyprland socket.
namespace {
class FakeHyprlandIpcTransport final : public HyprlandIpcTransport {
 public:
  explicit FakeHyprlandIpcTransport(QObject* parent = nullptr) : HyprlandIpcTransport(parent) {}

  void connectEventStream() override { connect_count++; }

  bool runCommand(const QByteArray& command, CommandCompletePredicate /*predicate*/ = {}) override {
    last_command = command;
    run_command_count++;
    return true;
  }

  [[nodiscard]] bool hasRunningCommand() const override { return simulate_running; }

  void fireEventLine(const QByteArray& line) { emit eventLineReceived(line); }
  void fireConnected() { emit eventStreamConnected(); }
  void fireCommandFinished(const QByteArray& response, bool success) { emit commandFinished(response, success); }

  int connect_count{0};
  int run_command_count{0};
  QByteArray last_command;
  bool simulate_running{false};
};
}  // namespace

TEST(KeyboardLayoutService, ProcessEventLineUpdatesLayoutCode) {
  auto transport = std::make_unique<FakeHyprlandIpcTransport>();
  FakeHyprlandIpcTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  service.start();

  fake->fireEventLine("activelayout>>at-translated-set-2-keyboard,English (US)");

  EXPECT_EQ(service.layoutCode(), QStringLiteral("EN"));
}

TEST(KeyboardLayoutService, ProcessEventLineIgnoresUnrelatedEvent) {
  auto transport = std::make_unique<FakeHyprlandIpcTransport>();
  FakeHyprlandIpcTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  service.start();

  fake->fireEventLine("activewindow>>kitty,build output");

  EXPECT_TRUE(service.layoutCode().isEmpty());
}

TEST(KeyboardLayoutService, OnCommandFinishedUpdatesLayoutCode) {
  auto transport = std::make_unique<FakeHyprlandIpcTransport>();
  FakeHyprlandIpcTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  service.start();

  const QByteArray json = R"json({"keyboards":[{"main":true,"active_keymap":"Ukrainian"}]})json";
  fake->fireCommandFinished(json, true);

  EXPECT_EQ(service.layoutCode(), QStringLiteral("UK"));
}

TEST(KeyboardLayoutService, OnCommandFinishedIgnoresFailedResponse) {
  auto transport = std::make_unique<FakeHyprlandIpcTransport>();
  FakeHyprlandIpcTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  service.start();

  const QByteArray json = R"json({"keyboards":[{"main":true,"active_keymap":"Ukrainian"}]})json";
  fake->fireCommandFinished(json, false);

  EXPECT_TRUE(service.layoutCode().isEmpty());
}

TEST(KeyboardLayoutService, SetLayoutCodeDeduplicatesIdenticalValues) {
  auto transport = std::make_unique<FakeHyprlandIpcTransport>();
  FakeHyprlandIpcTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  service.start();

  QSignalSpy spy(&service, &KeyboardLayoutService::layoutCodeChanged);

  fake->fireEventLine("activelayout>>kbd,English (US)");
  fake->fireEventLine("activelayout>>kbd,English (US)");

  EXPECT_EQ(spy.count(), 1);
  EXPECT_EQ(service.layoutCode(), QStringLiteral("EN"));
}

// REQ-C-014: the full layout name is retained alongside the derived code.
TEST(KeyboardLayoutService, EventLineRetainsBothTheNameAndTheCode) {
  auto transport = std::make_unique<FakeHyprlandIpcTransport>();
  FakeHyprlandIpcTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  service.start();

  fake->fireEventLine("activelayout>>at-translated-set-2-keyboard,English (US)");

  EXPECT_EQ(service.layoutCode(), QStringLiteral("EN"));
  EXPECT_EQ(service.layoutName(), QStringLiteral("English (US)"));
}

TEST(KeyboardLayoutService, DevicesQueryResponseRetainsBothTheNameAndTheCode) {
  auto transport = std::make_unique<FakeHyprlandIpcTransport>();
  FakeHyprlandIpcTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  service.start();

  const QByteArray json = R"json({"keyboards":[{"main":true,"active_keymap":"Ukrainian"}]})json";
  fake->fireCommandFinished(json, true);

  EXPECT_EQ(service.layoutCode(), QStringLiteral("UK"));
  EXPECT_EQ(service.layoutName(), QStringLiteral("Ukrainian"));
}

TEST(KeyboardLayoutService, SetLayoutNameDeduplicatesIdenticalValues) {
  auto transport = std::make_unique<FakeHyprlandIpcTransport>();
  FakeHyprlandIpcTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  service.start();

  QSignalSpy spy(&service, &KeyboardLayoutService::layoutNameChanged);

  fake->fireEventLine("activelayout>>kbd,English (US)");
  fake->fireEventLine("activelayout>>kbd,English (US)");

  EXPECT_EQ(spy.count(), 1);
}

// Two distinct names can share a code. The name must still update, and the code must stay put --
// this is the case that makes layoutName worth storing separately rather than deriving on demand.
TEST(KeyboardLayoutService, NameChangeWithAnUnchangedCodeUpdatesOnlyTheName) {
  auto transport = std::make_unique<FakeHyprlandIpcTransport>();
  FakeHyprlandIpcTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  service.start();

  fake->fireEventLine("activelayout>>kbd,English (US)");

  QSignalSpy name_spy(&service, &KeyboardLayoutService::layoutNameChanged);
  QSignalSpy code_spy(&service, &KeyboardLayoutService::layoutCodeChanged);

  fake->fireEventLine("activelayout>>kbd,English (UK)");

  EXPECT_EQ(name_spy.count(), 1);
  EXPECT_EQ(code_spy.count(), 0);
  EXPECT_EQ(service.layoutName(), QStringLiteral("English (UK)"));
  EXPECT_EQ(service.layoutCode(), QStringLiteral("EN"));
}

// Pins the commit order relied on by consumers that read both properties from either signal: when
// the code signal arrives, the name is already the matching one, never the previous layout's.
TEST(KeyboardLayoutService, NameIsAlreadyCommittedWhenTheCodeSignalFires) {
  auto transport = std::make_unique<FakeHyprlandIpcTransport>();
  FakeHyprlandIpcTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  service.start();

  fake->fireEventLine("activelayout>>kbd,English (US)");

  QString name_seen_with_code_change;
  QObject::connect(&service, &KeyboardLayoutService::layoutCodeChanged, &service,
                   [&service, &name_seen_with_code_change]() { name_seen_with_code_change = service.layoutName(); });

  fake->fireEventLine("activelayout>>kbd,Ukrainian");

  EXPECT_EQ(name_seen_with_code_change, QStringLiteral("Ukrainian"));
}

TEST(KeyboardLayoutService, StartIsIdempotent) {
  auto transport = std::make_unique<FakeHyprlandIpcTransport>();
  FakeHyprlandIpcTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));

  service.start();
  service.start();

  EXPECT_EQ(fake->connect_count, 1);
}

TEST(KeyboardLayoutService, EventSocketConnectedTriggersDevicesQuery) {
  auto transport = std::make_unique<FakeHyprlandIpcTransport>();
  FakeHyprlandIpcTransport* fake = transport.get();
  KeyboardLayoutService service(std::move(transport));
  service.start();

  fake->fireConnected();

  EXPECT_EQ(fake->run_command_count, 1);
  EXPECT_EQ(fake->last_command, QByteArrayLiteral("j/devices"));
}

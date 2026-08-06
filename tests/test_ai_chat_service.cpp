#include "AiChatService.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCall>

#include <gtest/gtest.h>

namespace {

class TestAiChatService final : public AiChatService {
 public:
  using AiChatService::AiChatService;

  QString requested_monitor;

 protected:
  QDBusPendingCall requestTogglePanel(const QString& monitor_name) override {
    requested_monitor = monitor_name;
    return QDBusPendingCall::fromError(QDBusError(QDBusError::Failed, QStringLiteral("test completion")));
  }
};

}  // namespace

TEST(AiChatService, ForwardsRequestedMonitor) {
  TestAiChatService service;

  service.togglePanel(QStringLiteral("DP-5"));

  EXPECT_EQ(service.requested_monitor, QStringLiteral("DP-5"));
}

TEST(AiChatService, ForwardsEmptyMonitor) {
  TestAiChatService service;

  service.togglePanel(QString{});

  EXPECT_TRUE(service.requested_monitor.isEmpty());
}

TEST(AiChatService, BuildsHolonightChatTogglePanelCall) {
  const QDBusMessage message = AiChatService::togglePanelMessage(QStringLiteral("DP-5"));

  EXPECT_EQ(message.service(), QStringLiteral("org.holonight.Chat"));
  EXPECT_EQ(message.path(), QStringLiteral("/org/holonight/Chat"));
  EXPECT_EQ(message.interface(), QStringLiteral("org.holonight.Chat1"));
  EXPECT_EQ(message.member(), QStringLiteral("TogglePanel"));
  ASSERT_EQ(message.arguments().size(), 1);
  EXPECT_EQ(message.arguments().constFirst().toString(), QStringLiteral("DP-5"));
}

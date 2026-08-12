#include "SettingsNavigationService.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCall>

#include <gtest/gtest.h>

namespace {

class TestSettingsNavigationService final : public SettingsNavigationService {
 public:
  using SettingsNavigationService::SettingsNavigationService;

  QString requested_page;

 protected:
  QDBusPendingCall requestOpenPage(const QString& page_key) override {
    requested_page = page_key;
    return QDBusPendingCall::fromError(QDBusError(QDBusError::Failed, QStringLiteral("test completion")));
  }
};

}  // namespace

TEST(SettingsNavigationService, ForwardsRequestedPageAsynchronously) {
  TestSettingsNavigationService service;

  service.openPage(QStringLiteral("audio"));

  EXPECT_EQ(service.requested_page, QStringLiteral("audio"));
}

TEST(SettingsNavigationService, BuildsFreedesktopActivateActionCall) {
  const QDBusMessage message = SettingsNavigationService::openPageMessage(QStringLiteral("audio"));

  EXPECT_EQ(message.service(), QStringLiteral("org.holonight.Settings"));
  EXPECT_EQ(message.path(), QStringLiteral("/org/holonight/Settings"));
  EXPECT_EQ(message.interface(), QStringLiteral("org.freedesktop.Application"));
  EXPECT_EQ(message.member(), QStringLiteral("ActivateAction"));
  ASSERT_EQ(message.arguments().size(), 3);
  EXPECT_EQ(message.arguments().at(0).toString(), QStringLiteral("audio"));
  EXPECT_TRUE(message.arguments().at(1).toList().isEmpty());
  EXPECT_TRUE(message.arguments().at(2).toMap().isEmpty());
}

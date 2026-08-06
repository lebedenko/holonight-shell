#include "NotificationService.h"
#include "SessionService.h"
#include "ShellApplication.h"

#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

// Regression test for the Q_ASSERT(registered_ && services_started_) previously in startShell(),
// which aborted the process (even in default GTest Debug builds) when construction order was
// violated. Reaching the end of this test proves the qCritical-guarded early return replaced it.
TEST(ShellApplicationTest, StartShellBeforeReadyDoesNotAbort) {
  ShellApplication app;

  QTest::ignoreMessage(QtCriticalMsg,
                       "ShellApplication: startShell() called before registerQmlTypes()/startServices(); ignoring");
  app.startShell();

  EXPECT_FALSE(app.shellSurfaceManagersConstructedForTest());
}

// The guarded early return must not regress the normal, correctly-ordered startup path.
TEST(ShellApplicationTest, StartShellSucceedsInCorrectOrder) {
  ShellApplication app;

  app.markReadyForShellStartForTest();
  app.startShell();

  EXPECT_TRUE(app.shellSurfaceManagersConstructedForTest());
}

TEST(ShellApplicationTest, SessionCommandFailureCreatesNotification) {
  ShellApplication app;
  app.connectSessionFailureNotificationsForTest();
  NotificationService* notification_service = app.notificationServiceForTest();
  QSignalSpy notification_added(notification_service, &NotificationService::notificationAdded);

  app.sessionServiceForTest()->commandFailed(QStringLiteral("lock"),
                                             QStringLiteral("failed to launch loginctl lock-session"));

  ASSERT_EQ(notification_added.size(), 1);
  ASSERT_EQ(notification_service->rowCount(), 1);
  const QModelIndex idx = notification_service->index(0);
  EXPECT_EQ(idx.data(NotificationService::SummaryRole).toString(), QStringLiteral("Session command failed: lock"));
  EXPECT_EQ(idx.data(NotificationService::BodyRole).toString(),
            QStringLiteral("failed to launch loginctl lock-session"));
}

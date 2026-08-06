#include "process/GuardedProcessRunner.h"

#include <QTest>

#include <gtest/gtest.h>
#include <memory>
#include <utility>

TEST(GuardedProcessRunner, FastCompletionFiresCallbackExactlyOnce) {
  auto call_count = std::make_shared<int>(0);
  auto last_result = std::make_shared<GuardedProcessResult>();

  runGuardedProcess(QStringLiteral("true"), {}, 5000, [call_count, last_result](GuardedProcessResult result) {
    ++(*call_count);
    *last_result = std::move(result);
  });

  QTest::qWait(200);

  EXPECT_EQ(*call_count, 1);
  EXPECT_FALSE(last_result->timed_out);
  EXPECT_FALSE(last_result->had_error);
  EXPECT_EQ(last_result->exit_code, 0);
}

TEST(GuardedProcessRunner, StartErrorFiresCallbackExactlyOnce) {
  auto call_count = std::make_shared<int>(0);
  auto last_result = std::make_shared<GuardedProcessResult>();

  runGuardedProcess(QStringLiteral("/nonexistent/holonight-guarded-process-test-binary"), {}, 5000,
                    [call_count, last_result](GuardedProcessResult result) {
                      ++(*call_count);
                      *last_result = std::move(result);
                    });

  QTest::qWait(200);

  EXPECT_EQ(*call_count, 1);
  EXPECT_FALSE(last_result->timed_out);
  EXPECT_TRUE(last_result->had_error);
}

TEST(GuardedProcessRunner, TimeoutKillsProcessAndFiresCallbackExactlyOnce) {
  auto call_count = std::make_shared<int>(0);
  auto last_result = std::make_shared<GuardedProcessResult>();

  runGuardedProcess(QStringLiteral("sleep"), {QStringLiteral("5")}, 100,
                    [call_count, last_result](GuardedProcessResult result) {
                      ++(*call_count);
                      *last_result = std::move(result);
                    });

  QTest::qWait(600);

  EXPECT_EQ(*call_count, 1);
  EXPECT_TRUE(last_result->timed_out);
}

TEST(GuardedProcessRunner, NonPositiveTimeoutReportsErrorWithoutStartingProcess) {
  int call_count = 0;
  GuardedProcessResult last_result;

  runGuardedProcess(QStringLiteral("sleep"), {QStringLiteral("5")}, 0,
                    [&call_count, &last_result](GuardedProcessResult result) {
                      ++call_count;
                      last_result = std::move(result);
                    });

  EXPECT_EQ(call_count, 1);
  EXPECT_TRUE(last_result.had_error);
  EXPECT_EQ(last_result.stderr_text, QStringLiteral("process timeout must be positive"));
}

TEST(GuardedProcessRunner, DestroyedCallbackContextSuppressesCallback) {
  int call_count = 0;
  {
    QObject context;
    runGuardedProcess(
        QStringLiteral("sleep"), {QStringLiteral("1")}, 100,
        [&call_count](const GuardedProcessResult& /*result*/) { ++call_count; }, &context);
  }

  QTest::qWait(300);

  EXPECT_EQ(call_count, 0);
}

#include "NotificationFilter.h"
#include "NotificationTypes.h"

#include <gtest/gtest.h>

namespace {

NotificationData makeNotif(NotifUrgency urgency, const QString& app_name = QStringLiteral("TestApp")) {
  NotificationData data;
  data.urgency = urgency;
  data.app_name = app_name;
  return data;
}

AppNotificationRule makeRule(const QString& app_name, bool enabled, UrgencyFilter filter = UrgencyFilter::None) {
  return AppNotificationRule{.app_name = app_name, .enabled = enabled, .urgency_filter = filter};
}

}  // namespace

// Critical urgency always returns Allow regardless of DND or per-app rules.
TEST(NotificationFilter, CriticalAlwaysAllowsWhenDndOff) {
  const NotificationData notif = makeNotif(NotifUrgency::Critical);
  EXPECT_EQ(evaluateFilter(notif, false, {}), FilterDecision::Allow);
}

TEST(NotificationFilter, CriticalAlwaysAllowsWhenDndOn) {
  const NotificationData notif = makeNotif(NotifUrgency::Critical);
  EXPECT_EQ(evaluateFilter(notif, true, {}), FilterDecision::Allow);
}

TEST(NotificationFilter, CriticalBypassesPerAppDisabledRule) {
  const NotificationData notif = makeNotif(NotifUrgency::Critical);
  const QList<AppNotificationRule> rules{makeRule(QStringLiteral("TestApp"), false)};
  EXPECT_EQ(evaluateFilter(notif, false, rules), FilterDecision::Allow);
}

TEST(NotificationFilter, CriticalBypassesPerAppUrgencyFilter) {
  const NotificationData notif = makeNotif(NotifUrgency::Critical);
  const QList<AppNotificationRule> rules{makeRule(QStringLiteral("TestApp"), true, UrgencyFilter::LowAndNormal)};
  EXPECT_EQ(evaluateFilter(notif, false, rules), FilterDecision::Allow);
}

// DND=true suppresses non-critical notifications.
TEST(NotificationFilter, DndSuppressesNormalUrgency) {
  const NotificationData notif = makeNotif(NotifUrgency::Normal);
  EXPECT_EQ(evaluateFilter(notif, true, {}), FilterDecision::Suppress);
}

TEST(NotificationFilter, DndSuppressesLowUrgency) {
  const NotificationData notif = makeNotif(NotifUrgency::Low);
  EXPECT_EQ(evaluateFilter(notif, true, {}), FilterDecision::Suppress);
}

TEST(NotificationFilter, DndDoesNotSuppressWithDndOff) {
  const NotificationData notif = makeNotif(NotifUrgency::Normal);
  EXPECT_EQ(evaluateFilter(notif, false, {}), FilterDecision::Allow);
}

// Per-app rule with enabled=false suppresses all non-critical notifications.
TEST(NotificationFilter, DisabledAppRuleSuppressesNormal) {
  const NotificationData notif = makeNotif(NotifUrgency::Normal);
  const QList<AppNotificationRule> rules{makeRule(QStringLiteral("TestApp"), false)};
  EXPECT_EQ(evaluateFilter(notif, false, rules), FilterDecision::Suppress);
}

TEST(NotificationFilter, DisabledAppRuleSuppressesLow) {
  const NotificationData notif = makeNotif(NotifUrgency::Low);
  const QList<AppNotificationRule> rules{makeRule(QStringLiteral("TestApp"), false)};
  EXPECT_EQ(evaluateFilter(notif, false, rules), FilterDecision::Suppress);
}

TEST(NotificationFilter, DisabledRuleDoesNotAffectOtherApps) {
  const NotificationData notif = makeNotif(NotifUrgency::Normal, QStringLiteral("OtherApp"));
  const QList<AppNotificationRule> rules{makeRule(QStringLiteral("TestApp"), false)};
  EXPECT_EQ(evaluateFilter(notif, false, rules), FilterDecision::Allow);
}

// Per-app urgency filter suppresses matching urgency levels.
TEST(NotificationFilter, UrgencyFilterLowSuppressesLow) {
  const NotificationData notif = makeNotif(NotifUrgency::Low);
  const QList<AppNotificationRule> rules{makeRule(QStringLiteral("TestApp"), true, UrgencyFilter::Low)};
  EXPECT_EQ(evaluateFilter(notif, false, rules), FilterDecision::Suppress);
}

TEST(NotificationFilter, UrgencyFilterLowAllowsNormal) {
  const NotificationData notif = makeNotif(NotifUrgency::Normal);
  const QList<AppNotificationRule> rules{makeRule(QStringLiteral("TestApp"), true, UrgencyFilter::Low)};
  EXPECT_EQ(evaluateFilter(notif, false, rules), FilterDecision::Allow);
}

TEST(NotificationFilter, UrgencyFilterNormalSuppressesNormal) {
  const NotificationData notif = makeNotif(NotifUrgency::Normal);
  const QList<AppNotificationRule> rules{makeRule(QStringLiteral("TestApp"), true, UrgencyFilter::Normal)};
  EXPECT_EQ(evaluateFilter(notif, false, rules), FilterDecision::Suppress);
}

TEST(NotificationFilter, UrgencyFilterNormalAllowsLow) {
  const NotificationData notif = makeNotif(NotifUrgency::Low);
  const QList<AppNotificationRule> rules{makeRule(QStringLiteral("TestApp"), true, UrgencyFilter::Normal)};
  EXPECT_EQ(evaluateFilter(notif, false, rules), FilterDecision::Allow);
}

TEST(NotificationFilter, UrgencyFilterLowAndNormalSuppressesBoth) {
  const QList<AppNotificationRule> rules{makeRule(QStringLiteral("TestApp"), true, UrgencyFilter::LowAndNormal)};
  EXPECT_EQ(evaluateFilter(makeNotif(NotifUrgency::Low), false, rules), FilterDecision::Suppress);
  EXPECT_EQ(evaluateFilter(makeNotif(NotifUrgency::Normal), false, rules), FilterDecision::Suppress);
}

TEST(NotificationFilter, UrgencyFilterNoneAllowsAll) {
  const QList<AppNotificationRule> rules{makeRule(QStringLiteral("TestApp"), true, UrgencyFilter::None)};
  EXPECT_EQ(evaluateFilter(makeNotif(NotifUrgency::Low), false, rules), FilterDecision::Allow);
  EXPECT_EQ(evaluateFilter(makeNotif(NotifUrgency::Normal), false, rules), FilterDecision::Allow);
}

// No matching rule allows all non-critical notifications.
TEST(NotificationFilter, NoRuleAllowsAll) {
  EXPECT_EQ(evaluateFilter(makeNotif(NotifUrgency::Low), false, {}), FilterDecision::Allow);
  EXPECT_EQ(evaluateFilter(makeNotif(NotifUrgency::Normal), false, {}), FilterDecision::Allow);
}

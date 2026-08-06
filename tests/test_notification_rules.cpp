#include "NotificationRuleModel.h"
#include "NotificationRuleStore.h"
#include "NotificationService.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVariantMap>

#include <gtest/gtest.h>

namespace {

bool writeFile(const QString& path, const QByteArray& content) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  file.write(content);
  return true;
}

QString rulesPath(const QTemporaryDir& dir) { return dir.path() + QStringLiteral("/notification-rules.json"); }

QString appsDir(const QTemporaryDir& dir) { return dir.path() + QStringLiteral("/applications"); }

void writeDesktopEntry(const QString& dir, const QString& file_name, const QByteArray& extra = {}) {
  QDir().mkpath(dir);
  ASSERT_TRUE(writeFile(dir + QLatin1Char('/') + file_name, QByteArrayLiteral("[Desktop Entry]\n"
                                                                              "Type=Application\n"
                                                                              "Name=Test App\n"
                                                                              "Exec=test-app\n") +
                                                                extra));
}

NotificationData makeNotification(const QString& app_name, const QString& desktop_entry = {},
                                  const QString& app_icon = {}) {
  NotificationData data;
  data.app_name = app_name;
  data.app_icon = app_icon;
  data.monitor_name = QStringLiteral("DP-1");
  data.expire_timeout_ms = 0;
  if (!desktop_entry.isEmpty()) {
    data.hints.insert(QStringLiteral("desktop-entry"), desktop_entry);
  }
  return data;
}

QJsonObject ruleJson(const QString& app_name, const QString& desktop_entry, const QString& app_icon, bool enabled,
                     int urgency_filter, qint64 last_seen_ms) {
  QJsonObject obj;
  obj[QStringLiteral("appName")] = app_name;
  obj[QStringLiteral("desktopEntry")] = desktop_entry;
  obj[QStringLiteral("appIcon")] = app_icon;
  obj[QStringLiteral("enabled")] = enabled;
  obj[QStringLiteral("urgencyFilter")] = urgency_filter;
  obj[QStringLiteral("lastSeenMs")] = last_seen_ms;
  return obj;
}

void writeRuleFile(const QString& path, const QJsonArray& rules, int version = 1) {
  QJsonObject root;
  root[QStringLiteral("version")] = version;
  root[QStringLiteral("rules")] = rules;
  ASSERT_TRUE(writeFile(path, QJsonDocument(root).toJson(QJsonDocument::Compact)));
}

// Blocks until no further writeCompleted signal arrives within `quiet_ms` — used after a burst of
// persist() calls that may coalesce into fewer than one write per call (dirty-flag serialization).
void waitForWritesToSettle(QSignalSpy& write_spy, int quiet_ms = 500) {
  while (write_spy.wait(quiet_ms)) {
  }
}

}  // namespace

TEST(NotificationRuleStore, LoadsValidRules) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  writeRuleFile(rulesPath(dir), {ruleJson(QStringLiteral("Slack"), QStringLiteral("slack"),
                                          QStringLiteral("slack-icon"), false, 3, 1730000000000)});

  const NotificationRuleStore store(rulesPath(dir));
  const QList<AppNotificationRule> rules = store.load();

  ASSERT_EQ(rules.size(), 1);
  EXPECT_EQ(rules.first().app_name, QStringLiteral("Slack"));
  EXPECT_EQ(rules.first().desktop_entry, QStringLiteral("slack"));
  EXPECT_EQ(rules.first().app_icon, QStringLiteral("slack-icon"));
  EXPECT_FALSE(rules.first().enabled);
  EXPECT_EQ(rules.first().urgency_filter, UrgencyFilter::LowAndNormal);
  EXPECT_EQ(rules.first().last_seen_ms, 1730000000000);
}

TEST(NotificationRuleStore, SkipsMalformedRules) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QJsonArray rules;
  rules.append(ruleJson(QStringLiteral("Good"), QString(), QString(), true, 0, 10));
  rules.append(QJsonObject{{QStringLiteral("appName"), QStringLiteral("Missing fields")}});
  rules.append(ruleJson(QStringLiteral("Bad Filter"), QString(), QString(), true, 9, 10));
  writeRuleFile(rulesPath(dir), rules);

  const NotificationRuleStore store(rulesPath(dir));
  const QList<AppNotificationRule> loaded = store.load();

  ASSERT_EQ(loaded.size(), 1);
  EXPECT_EQ(loaded.first().app_name, QStringLiteral("Good"));
}

TEST(NotificationRuleStore, IgnoresWrongVersion) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  writeRuleFile(rulesPath(dir), {ruleJson(QStringLiteral("Slack"), QString(), QString(), true, 0, 1)}, 99);

  const NotificationRuleStore store(rulesPath(dir));
  EXPECT_TRUE(store.load().isEmpty());
}

TEST(NotificationRuleStore, WritesExpectedJson) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  NotificationRuleStore store(rulesPath(dir));
  QSignalSpy write_spy(&store, &NotificationRuleStore::writeCompleted);
  store.persist({AppNotificationRule{.app_name = QStringLiteral("Slack"),
                                     .desktop_entry = QStringLiteral("slack"),
                                     .app_icon = QStringLiteral("slack-icon"),
                                     .enabled = false,
                                     .urgency_filter = UrgencyFilter::Normal,
                                     .last_seen_ms = 42}},
                QStringLiteral("updateRule"));

  ASSERT_TRUE(write_spy.wait(2000));

  QFile file(rulesPath(dir));
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));
  const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
  EXPECT_EQ(root.value(QStringLiteral("version")).toInt(), 1);
  const QJsonArray rules = root.value(QStringLiteral("rules")).toArray();
  ASSERT_EQ(rules.size(), 1);
  const QJsonObject rule = rules.first().toObject();
  EXPECT_EQ(rule.value(QStringLiteral("appName")).toString(), QStringLiteral("Slack"));
  EXPECT_EQ(rule.value(QStringLiteral("desktopEntry")).toString(), QStringLiteral("slack"));
  EXPECT_EQ(rule.value(QStringLiteral("appIcon")).toString(), QStringLiteral("slack-icon"));
  EXPECT_FALSE(rule.value(QStringLiteral("enabled")).toBool());
  EXPECT_EQ(rule.value(QStringLiteral("urgencyFilter")).toInt(), 2);
  EXPECT_EQ(rule.value(QStringLiteral("lastSeenMs")).toInteger(), 42);
}

TEST(NotificationRuleStore, RapidPersistCallsCoalesceToLatestState) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  NotificationRuleStore store(rulesPath(dir));
  QSignalSpy write_spy(&store, &NotificationRuleStore::writeCompleted);

  // Five synchronous persist() calls with no event-loop yield in between: the first launches a
  // write immediately, the remaining four are buffered by the dirty-flag and each overwrites the
  // pending snapshot — so exactly one coalesced follow-up write (with the last state) is expected,
  // never five.
  for (int i = 0; i < 5; ++i) {
    store.persist({AppNotificationRule{.app_name = QStringLiteral("App-%1").arg(i)}},
                  QStringLiteral("burst-%1").arg(i));
  }

  waitForWritesToSettle(write_spy);
  EXPECT_EQ(write_spy.count(), 2);

  const QList<AppNotificationRule> loaded = store.load();
  ASSERT_EQ(loaded.size(), 1);
  EXPECT_EQ(loaded.first().app_name, QStringLiteral("App-4"));
}

TEST(NotificationRuleStore,
     PersistFailureEmitsPersistFailedWithReason) {  // NOLINT(readability-function-cognitive-complexity)
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString rules_dir = dir.path() + QStringLiteral("/rules-subdir");
  ASSERT_TRUE(QDir().mkpath(rules_dir));
  const QString path = rules_dir + QStringLiteral("/notification-rules.json");

  ASSERT_TRUE(QFile::setPermissions(rules_dir, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
  QFile probe(rules_dir + QStringLiteral("/.write-probe"));
  const bool permissions_enforced = !probe.open(QIODevice::WriteOnly);
  if (!permissions_enforced) {
    // Running as a user (e.g. root in some CI containers) whose write is not blocked by mode
    // bits — the induced failure below would not reproduce. See Phase 4 DESIGN.md Item 3 Risks.
    QFile::setPermissions(rules_dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    GTEST_SKIP() << "write permissions not enforced for the current user; skipping";
  }

  NotificationRuleStore store(path);
  QSignalSpy fail_spy(&store, &NotificationRuleStore::persistFailed);
  QSignalSpy write_spy(&store, &NotificationRuleStore::writeCompleted);

  store.persist({AppNotificationRule{.app_name = QStringLiteral("App")}}, QStringLiteral("setEnabled"));

  ASSERT_TRUE(write_spy.wait(2000));
  ASSERT_EQ(fail_spy.count(), 1);
  EXPECT_EQ(fail_spy.first().at(0).toString(), QStringLiteral("setEnabled"));
  EXPECT_FALSE(fail_spy.first().at(1).toString().isEmpty());

  QFile::setPermissions(rules_dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}

TEST(NotificationRuleModel, RoleNamesExposeQmlContract) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  NotificationRuleStore store(rulesPath(dir));
  NotificationRuleModel model(&store, DesktopEntryScanner({appsDir(dir)}));
  const QHash<int, QByteArray> roles = model.roleNames();

  EXPECT_EQ(roles.value(NotificationRuleModel::AppNameRole), "appName");
  EXPECT_EQ(roles.value(NotificationRuleModel::DisplayNameRole), "displayName");
  EXPECT_EQ(roles.value(NotificationRuleModel::DisplayIconRole), "displayIcon");
  EXPECT_EQ(roles.value(NotificationRuleModel::EnabledRole), "ruleEnabled");
  EXPECT_EQ(roles.value(NotificationRuleModel::UrgencyFilterRole), "urgencyFilter");
  EXPECT_EQ(roles.value(NotificationRuleModel::DesktopEntryRole), "desktopEntry");
  EXPECT_EQ(roles.value(NotificationRuleModel::AppIconRole), "appIcon");
  EXPECT_EQ(roles.value(NotificationRuleModel::LastSeenMsRole), "lastSeenMs");
  EXPECT_EQ(roles.size(), 8);
}

TEST(NotificationRuleModel, PersistedRulesSurviveNewInstance) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  writeDesktopEntry(appsDir(dir), QStringLiteral("slack.desktop"));

  NotificationRuleStore store(rulesPath(dir));
  QSignalSpy write_spy(&store, &NotificationRuleStore::writeCompleted);
  const DesktopEntryScanner scanner({appsDir(dir)});

  {
    NotificationRuleModel model(&store, scanner);
    model.ensureApp(
        makeNotification(QStringLiteral("Slack"), QStringLiteral("slack.desktop"), QStringLiteral("slack-icon")));
    model.setEnabled(0, false);
    model.setUrgencyFilter(0, 3);
  }
  waitForWritesToSettle(write_spy);

  NotificationRuleModel restored(&store, scanner);
  ASSERT_EQ(restored.rowCount(), 1);
  EXPECT_EQ(restored.data(restored.index(0), NotificationRuleModel::AppNameRole).toString(), QStringLiteral("Slack"));
  EXPECT_FALSE(restored.data(restored.index(0), NotificationRuleModel::EnabledRole).toBool());
  EXPECT_EQ(restored.data(restored.index(0), NotificationRuleModel::UrgencyFilterRole).toInt(), 3);
  EXPECT_EQ(restored.data(restored.index(0), NotificationRuleModel::DesktopEntryRole).toString(),
            QStringLiteral("slack"));
}

TEST(NotificationRuleModel, TogglingEnabledAndFilterPersists) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  NotificationRuleStore store(rulesPath(dir));
  QSignalSpy write_spy(&store, &NotificationRuleStore::writeCompleted);
  NotificationRuleModel model(&store, DesktopEntryScanner({appsDir(dir)}));
  model.ensureApp(makeNotification(QStringLiteral("App")));

  model.setEnabled(0, false);
  model.setUrgencyFilter(0, 1);
  waitForWritesToSettle(write_spy);

  const QList<AppNotificationRule> loaded = store.load();
  ASSERT_EQ(loaded.size(), 1);
  EXPECT_FALSE(loaded.first().enabled);
  EXPECT_EQ(loaded.first().urgency_filter, UrgencyFilter::Low);
}

TEST(NotificationRuleModel, RepeatedNotificationsUpdateMetadataWithoutDuplicatingRows) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  writeDesktopEntry(appsDir(dir), QStringLiteral("app.desktop"));
  NotificationRuleStore store(rulesPath(dir));
  NotificationRuleModel model(&store, DesktopEntryScanner({appsDir(dir)}));

  model.ensureApp(makeNotification(QStringLiteral("App"), QStringLiteral("old.desktop"), QStringLiteral("old")));
  const qint64 first_seen = model.data(model.index(0), NotificationRuleModel::LastSeenMsRole).toLongLong();
  model.ensureApp(makeNotification(QStringLiteral("App"), QStringLiteral("app.desktop"), QStringLiteral("new")));

  ASSERT_EQ(model.rowCount(), 1);
  EXPECT_EQ(model.data(model.index(0), NotificationRuleModel::DesktopEntryRole).toString(), QStringLiteral("app"));
  EXPECT_EQ(model.data(model.index(0), NotificationRuleModel::AppIconRole).toString(), QStringLiteral("new"));
  EXPECT_GE(model.data(model.index(0), NotificationRuleModel::LastSeenMsRole).toLongLong(), first_seen);
}

TEST(NotificationRuleModel, StopsDiscoveringAppsAtRuleLimit) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  NotificationRuleStore store(rulesPath(dir));
  NotificationRuleModel model(&store, DesktopEntryScanner({appsDir(dir)}));

  for (qsizetype index = 0; index < NotificationRuleModel::kMaxRuleCount; ++index) {
    model.ensureApp(QStringLiteral("App-%1").arg(index));
  }
  model.ensureApp(QStringLiteral("Overflow"));
  model.ensureApp(makeNotification(QStringLiteral("App-255"), {}, QStringLiteral("updated-icon")));

  EXPECT_EQ(model.rowCount(), NotificationRuleModel::kMaxRuleCount);
  EXPECT_EQ(model.data(model.index(model.rowCount() - 1), NotificationRuleModel::AppNameRole).toString(),
            QStringLiteral("App-255"));
  EXPECT_EQ(model.data(model.index(model.rowCount() - 1), NotificationRuleModel::AppIconRole).toString(),
            QStringLiteral("updated-icon"));
}

TEST(NotificationRuleModel, LoadsMostRecentlySeenRulesWithinLimit) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QJsonArray persisted;
  for (qsizetype index = 0; index < NotificationRuleModel::kMaxRuleCount + 2; ++index) {
    persisted.append(ruleJson(QStringLiteral("App-%1").arg(index), {}, {}, true, 0, index));
  }
  writeRuleFile(rulesPath(dir), persisted);

  NotificationRuleStore store(rulesPath(dir));
  NotificationRuleModel model(&store, DesktopEntryScanner({appsDir(dir)}));

  ASSERT_EQ(model.rowCount(), NotificationRuleModel::kMaxRuleCount);
  EXPECT_EQ(model.data(model.index(0), NotificationRuleModel::AppNameRole).toString(), QStringLiteral("App-2"));
  EXPECT_EQ(model.data(model.index(model.rowCount() - 1), NotificationRuleModel::AppNameRole).toString(),
            QStringLiteral("App-257"));
}

TEST(NotificationService, SuppressedNotificationsStillUpdateRules) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  writeDesktopEntry(appsDir(dir), QStringLiteral("chat.desktop"));
  NotificationRuleStore store(rulesPath(dir));
  writeRuleFile(rulesPath(dir), {ruleJson(QStringLiteral("Chat"), QStringLiteral("chat"), QString(), false, 0, 1)});

  NotificationRuleModel rules(&store, DesktopEntryScanner({appsDir(dir)}));
  NotificationService service(nullptr, nullptr, &rules);

  const uint32_t notif_id = service.addOrReplace(
      makeNotification(QStringLiteral("Chat"), QStringLiteral("chat.desktop"), QStringLiteral("chat-icon")));

  EXPECT_EQ(notif_id, 0U);
  EXPECT_EQ(service.rowCount(), 0);
  ASSERT_EQ(rules.rowCount(), 1);
  EXPECT_EQ(rules.data(rules.index(0), NotificationRuleModel::AppIconRole).toString(), QStringLiteral("chat-icon"));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(NotificationRuleModel, PrunesOnlyRulesForMissingInstalledDesktopEntries) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  writeDesktopEntry(appsDir(dir), QStringLiteral("installed.desktop"));
  writeDesktopEntry(appsDir(dir), QStringLiteral("nodisplay.desktop"), QByteArrayLiteral("NoDisplay=true\n"));
  writeDesktopEntry(appsDir(dir), QStringLiteral("hidden.desktop"), QByteArrayLiteral("Hidden=true\n"));
  writeRuleFile(rulesPath(dir),
                {
                    ruleJson(QStringLiteral("Installed"), QStringLiteral("installed"), QString(), true, 0, 1),
                    ruleJson(QStringLiteral("Missing"), QStringLiteral("missing"), QString(), true, 0, 1),
                    ruleJson(QStringLiteral("NoDisplay"), QStringLiteral("nodisplay"), QString(), true, 0, 1),
                    ruleJson(QStringLiteral("Hidden"), QStringLiteral("hidden"), QString(), true, 0, 1),
                    ruleJson(QStringLiteral("No Desktop Entry"), QString(), QString(), true, 0, 1),
                });

  NotificationRuleStore store(rulesPath(dir));
  NotificationRuleModel model(&store, DesktopEntryScanner({appsDir(dir)}));

  ASSERT_EQ(model.rowCount(), 3);
  QStringList app_names;
  for (int row = 0; row < model.rowCount(); ++row) {
    app_names.append(model.data(model.index(row), NotificationRuleModel::AppNameRole).toString());
  }
  EXPECT_TRUE(app_names.contains(QStringLiteral("Installed")));
  EXPECT_TRUE(app_names.contains(QStringLiteral("NoDisplay")));
  EXPECT_TRUE(app_names.contains(QStringLiteral("No Desktop Entry")));
  EXPECT_FALSE(app_names.contains(QStringLiteral("Missing")));
  EXPECT_FALSE(app_names.contains(QStringLiteral("Hidden")));
}

TEST(NotificationRuleModel, KeepsDisabledNoDisplaySattyRuleEnableable) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  writeDesktopEntry(appsDir(dir), QStringLiteral("satty.desktop"),
                    QByteArrayLiteral("NoDisplay=true\nStartupWMClass=com.gabm.satty\n"));
  writeRuleFile(rulesPath(dir),
                {ruleJson(QStringLiteral("Satty"), QStringLiteral("com.gabm.satty"), QString(), false, 0, 1)});

  NotificationRuleStore store(rulesPath(dir));
  NotificationRuleModel model(&store, DesktopEntryScanner({appsDir(dir)}));

  ASSERT_EQ(model.rowCount(), 1);
  EXPECT_FALSE(model.data(model.index(0), NotificationRuleModel::EnabledRole).toBool());
  model.setEnabled(0, true);
  EXPECT_TRUE(model.data(model.index(0), NotificationRuleModel::EnabledRole).toBool());
}

TEST(NotificationRuleModel, ResolvesDisplayNameAndIconFromDesktopEntry) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  writeDesktopEntry(appsDir(dir), QStringLiteral("satty.desktop"),
                    QByteArrayLiteral("NoDisplay=true\nStartupWMClass=com.gabm.satty\nName=Satty\nIcon=satty\n"));
  writeRuleFile(rulesPath(dir), {ruleJson(QStringLiteral("satty"), QStringLiteral("com.gabm.satty"),
                                          QStringLiteral("raw-icon"), true, 0, 1)});

  NotificationRuleStore store(rulesPath(dir));
  NotificationRuleModel model(&store, DesktopEntryScanner({appsDir(dir)}));

  ASSERT_EQ(model.rowCount(), 1);
  EXPECT_EQ(model.data(model.index(0), NotificationRuleModel::AppNameRole).toString(), QStringLiteral("satty"));
  EXPECT_EQ(model.data(model.index(0), NotificationRuleModel::DisplayNameRole).toString(), QStringLiteral("Satty"));
  EXPECT_EQ(model.data(model.index(0), NotificationRuleModel::DisplayIconRole).toString(), QStringLiteral("satty"));
}

TEST(NotificationRuleModel, ResolvesDisplayMetadataFromAUniqueAppNameMatch) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  writeDesktopEntry(appsDir(dir), QStringLiteral("blueman-manager.desktop"),
                    QByteArrayLiteral("Name=Bluetooth Manager\nIcon=blueman\nExec=blueman-manager\n"));
  writeRuleFile(rulesPath(dir),
                {ruleJson(QStringLiteral("blueman"), QString(), QStringLiteral("battery"), true, 0, 1)});

  NotificationRuleStore store(rulesPath(dir));
  NotificationRuleModel model(&store, DesktopEntryScanner({appsDir(dir)}));

  ASSERT_EQ(model.rowCount(), 1);
  EXPECT_EQ(model.data(model.index(0), NotificationRuleModel::DisplayNameRole).toString(),
            QStringLiteral("Bluetooth Manager"));
  EXPECT_EQ(model.data(model.index(0), NotificationRuleModel::DisplayIconRole).toString(), QStringLiteral("blueman"));
}

TEST(NotificationRuleModel, UsesTheShellLogoForShellNotifications) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  writeRuleFile(rulesPath(dir),
                {ruleJson(QStringLiteral("HoloNight Shell"), QString(), QStringLiteral("battery-low"), true, 0, 1)});

  NotificationRuleStore store(rulesPath(dir));
  NotificationRuleModel model(&store, DesktopEntryScanner({appsDir(dir)}));

  ASSERT_EQ(model.rowCount(), 1);
  EXPECT_EQ(model.data(model.index(0), NotificationRuleModel::DisplayNameRole).toString(),
            QStringLiteral("HoloNight Shell"));
  EXPECT_EQ(model.data(model.index(0), NotificationRuleModel::DisplayIconRole).toString(),
            QStringLiteral("qrc:/HolonightShell/logo.png"));
}

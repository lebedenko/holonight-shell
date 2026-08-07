#include "ConfigService.h"
#include "NotificationService.h"
#include "NotificationStore.h"
#include "NotificationTypes.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <gtest/gtest.h>

using namespace HoloNight::ShellConfig;

namespace {

void writeFile(const QString& path, const QByteArray& content) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
  ASSERT_EQ(file.write(content), content.size());
}

NotificationData makeHistoryNotif(const QString& summary) {
  NotificationData data;
  data.monitor_name = QStringLiteral("DP-1");
  data.app_name = QStringLiteral("app");
  data.summary = summary;
  data.app_icon = QStringLiteral("dialog-information");
  data.expire_timeout_ms = 0;
  return data;
}

NotificationHistoryItem makeHistoryItem(const QString& summary, const QString& body) {
  NotificationHistoryItem item;
  item.id = 1;
  item.app_name = QStringLiteral("app");
  item.summary = summary;
  item.body = body;
  item.app_icon = QStringLiteral("dialog-information");
  item.urgency = 1;
  item.timestamp_ms = 1718000000000;
  item.closed_reason = 1;
  item.read = true;
  return item;
}

}  // namespace

TEST(NotificationStore, loadRejectsWrongVersion) {
  QTemporaryDir state_dir;
  ASSERT_TRUE(state_dir.isValid());
  qputenv("XDG_STATE_HOME", state_dir.path().toUtf8());

  const QString path = state_dir.path() + QStringLiteral("/holonight/notifications/history.json");
  writeFile(path, R"({"version":2,"entries":[]})");

  NotificationStore store(NotificationHistoryConfig{});
  EXPECT_TRUE(store.load().isEmpty());

  qunsetenv("XDG_STATE_HOME");
}

TEST(NotificationStore, loadSkipsMalformedRequiredFields) {
  QTemporaryDir state_dir;
  ASSERT_TRUE(state_dir.isValid());
  qputenv("XDG_STATE_HOME", state_dir.path().toUtf8());

  const QString path = state_dir.path() + QStringLiteral("/holonight/notifications/history.json");
  writeFile(path,
            R"({
              "version": 1,
              "entries": [
                {
                  "id": 1,
                  "appName": "app",
                  "appClass": "",
                  "summary": "valid",
                  "appIcon": "dialog-information",
                  "urgency": 1,
                  "timestampMs": 1718000000000,
                  "closedReason": 1,
                  "read": true,
                  "actions": []
                },
                {
                  "id": 2,
                  "appName": "app",
                  "appClass": "",
                  "summary": 42,
                  "appIcon": "dialog-information",
                  "urgency": 1,
                  "timestampMs": 1718000000000,
                  "closedReason": 1,
                  "read": true,
                  "actions": []
                }
              ]
            })");

  NotificationStore store(NotificationHistoryConfig{});
  const QList<NotificationHistoryItem> items = store.load();
  ASSERT_EQ(items.size(), 1);
  EXPECT_EQ(items.front().summary, QStringLiteral("valid"));

  qunsetenv("XDG_STATE_HOME");
}

TEST(NotificationStore, updateConfigAppliesToNextPersist) {
  QTemporaryDir state_dir;
  ASSERT_TRUE(state_dir.isValid());
  qputenv("XDG_STATE_HOME", state_dir.path().toUtf8());

  NotificationHistoryConfig config;
  config.persist_body = true;
  NotificationStore store(config);
  QSignalSpy write_spy(&store, &NotificationStore::writeCompleted);

  config.persist_body = false;
  store.updateConfig(config);
  store.persist({makeHistoryItem(QStringLiteral("summary"), QStringLiteral("private body"))});

  ASSERT_TRUE(write_spy.wait(2000));

  QFile file(store.historyFilePath());
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  ASSERT_TRUE(doc.isObject());
  const QJsonArray entries = doc.object().value(QStringLiteral("entries")).toArray();
  ASSERT_EQ(entries.size(), 1);
  EXPECT_FALSE(entries.at(0).toObject().contains(QStringLiteral("body")));

  qunsetenv("XDG_STATE_HOME");
}

TEST(NotificationServiceHistory, closePersistsBoundedHistoryInMemory) {
  QTemporaryDir config_dir;
  QTemporaryDir state_dir;
  ASSERT_TRUE(config_dir.isValid());
  ASSERT_TRUE(state_dir.isValid());
  qputenv("XDG_CONFIG_HOME", config_dir.path().toUtf8());
  qputenv("XDG_STATE_HOME", state_dir.path().toUtf8());

  const QString config_path = config_dir.path() + QStringLiteral("/holonight/config.toml");
  writeFile(config_path,
            "[notifications.history]\n"
            "enabled = true\n"
            "max_items = 1\n"
            "max_age_days = 14\n"
            "persist_body = true\n");

  ConfigService config;
  NotificationService service(&config, nullptr);

  const uint32_t first = service.addOrReplace(makeHistoryNotif(QStringLiteral("first")));
  service.requestClose(first);
  const uint32_t second = service.addOrReplace(makeHistoryNotif(QStringLiteral("second")));
  service.requestClose(second);

  const QVariantList groups = service.recentHistoryGrouped(10);
  ASSERT_EQ(groups.size(), 1);
  const QVariantMap group = groups.front().toMap();
  EXPECT_EQ(group.value(QStringLiteral("totalCount")).toInt(), 1);
  EXPECT_EQ(group.value(QStringLiteral("latestSummary")).toString(), QStringLiteral("second"));
  EXPECT_EQ(service.unreadCount(), 1);

  qunsetenv("XDG_CONFIG_HOME");
  qunsetenv("XDG_STATE_HOME");
}

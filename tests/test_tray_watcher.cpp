#include "TrayModel.h"
#include "TrayWatcher.h"

#include <QDBusError>

#include <gtest/gtest.h>

class FakeTrayRegistrationClient final : public TrayRegistrationClient {
 public:
  explicit FakeTrayRegistrationClient(QStringList services) : services_(std::move(services)) {}

  bool registerHost(const QString& /*service*/) override { return true; }
  TrayWatcherRegistrationResult registerWatcher(QObject* /*watcher*/) override {
    return TrayWatcherRegistrationResult::Registered;
  }
  QStringList registeredServices() override { return services_; }

 private:
  QStringList services_;
};

TEST(TrayWatcher, NormalisesServiceOnlyRegistrationToDefaultItemPath) {
  EXPECT_EQ(normaliseTrayItemKey(QStringLiteral("org.example.Tray")),
            QStringLiteral("org.example.Tray:/StatusNotifierItem"));
}

TEST(TrayWatcher, KeepsExplicitServicePathAndPathOnlyRegistrationKeys) {
  EXPECT_EQ(normaliseTrayItemKey(QStringLiteral("org.example.Tray:/CustomItem")),
            QStringLiteral("org.example.Tray:/CustomItem"));
  EXPECT_EQ(normaliseTrayItemKey(QStringLiteral("/StatusNotifierItem")), QStringLiteral("/StatusNotifierItem"));
}

TEST(TrayWatcher, SplitsServicePathKeysWithoutBreakingUniqueBusNames) {
  const TrayItemAddress named = splitTrayItemKey(QStringLiteral("org.example.Tray:/CustomItem"));
  EXPECT_EQ(named.service, QStringLiteral("org.example.Tray"));
  EXPECT_EQ(named.path, QStringLiteral("/CustomItem"));

  const TrayItemAddress unique = splitTrayItemKey(QStringLiteral(":1.42:/StatusNotifierItem"));
  EXPECT_EQ(unique.service, QStringLiteral(":1.42"));
  EXPECT_EQ(unique.path, QStringLiteral("/StatusNotifierItem"));
}

TEST(TrayWatcher, SplitsServiceOnlyKeysToDefaultItemPath) {
  const TrayItemAddress address = splitTrayItemKey(QStringLiteral("org.example.Tray"));

  EXPECT_EQ(address.service, QStringLiteral("org.example.Tray"));
  EXPECT_EQ(address.path, QStringLiteral("/StatusNotifierItem"));
}

TEST(TrayWatcher, RegisterTrayItemKeyIgnoresDuplicatesAndTracksServiceCounts) {
  QStringList registered_items;
  QSet<QString> registered_item_keys;
  QHash<QString, int> service_item_counts;

  EXPECT_TRUE(registerTrayItemKey(registered_items, registered_item_keys, service_item_counts,
                                  QStringLiteral("org.example.Tray:/One")));
  EXPECT_FALSE(registerTrayItemKey(registered_items, registered_item_keys, service_item_counts,
                                   QStringLiteral("org.example.Tray:/One")));
  EXPECT_TRUE(registerTrayItemKey(registered_items, registered_item_keys, service_item_counts,
                                  QStringLiteral("org.example.Tray:/Two")));

  EXPECT_EQ(registered_items,
            QStringList({QStringLiteral("org.example.Tray:/One"), QStringLiteral("org.example.Tray:/Two")}));
  EXPECT_EQ(registered_item_keys,
            QSet<QString>({QStringLiteral("org.example.Tray:/One"), QStringLiteral("org.example.Tray:/Two")}));
  EXPECT_EQ(service_item_counts.value(QStringLiteral("org.example.Tray")), 2);
}

TEST(TrayWatcher, UnregisterTrayItemKeyRemovesKnownItemsAndIgnoresUnknownItems) {
  QStringList registered_items;
  QSet<QString> registered_item_keys;
  QHash<QString, int> service_item_counts;
  ASSERT_TRUE(registerTrayItemKey(registered_items, registered_item_keys, service_item_counts,
                                  QStringLiteral("org.example.Tray:/One")));
  ASSERT_TRUE(registerTrayItemKey(registered_items, registered_item_keys, service_item_counts,
                                  QStringLiteral("org.example.Tray:/Two")));

  EXPECT_TRUE(unregisterTrayItemKey(registered_items, registered_item_keys, service_item_counts,
                                    QStringLiteral("org.example.Tray:/One")));
  EXPECT_FALSE(unregisterTrayItemKey(registered_items, registered_item_keys, service_item_counts,
                                     QStringLiteral("org.example.Tray:/Missing")));

  EXPECT_EQ(registered_items, QStringList({QStringLiteral("org.example.Tray:/Two")}));
  EXPECT_EQ(registered_item_keys, QSet<QString>({QStringLiteral("org.example.Tray:/Two")}));
  EXPECT_EQ(service_item_counts.value(QStringLiteral("org.example.Tray")), 1);

  EXPECT_TRUE(unregisterTrayItemKey(registered_items, registered_item_keys, service_item_counts,
                                    QStringLiteral("org.example.Tray:/Two")));
  EXPECT_FALSE(service_item_counts.contains(QStringLiteral("org.example.Tray")));
  EXPECT_TRUE(registered_item_keys.isEmpty());
}

TEST(TrayWatcher, UnregisterTrayServiceItemsRemovesAllItemsForService) {
  QStringList registered_items;
  QSet<QString> registered_item_keys;
  QHash<QString, int> service_item_counts;
  ASSERT_TRUE(registerTrayItemKey(registered_items, registered_item_keys, service_item_counts,
                                  QStringLiteral("org.example.One:/A")));
  ASSERT_TRUE(registerTrayItemKey(registered_items, registered_item_keys, service_item_counts,
                                  QStringLiteral("org.example.One:/B")));
  ASSERT_TRUE(registerTrayItemKey(registered_items, registered_item_keys, service_item_counts,
                                  QStringLiteral("org.example.Two:/A")));

  const QStringList removed = unregisterTrayServiceItems(registered_items, registered_item_keys, service_item_counts,
                                                         QStringLiteral("org.example.One"));

  EXPECT_EQ(removed, QStringList({QStringLiteral("org.example.One:/A"), QStringLiteral("org.example.One:/B")}));
  EXPECT_EQ(registered_items, QStringList({QStringLiteral("org.example.Two:/A")}));
  EXPECT_EQ(registered_item_keys, QSet<QString>({QStringLiteral("org.example.Two:/A")}));
  EXPECT_FALSE(service_item_counts.contains(QStringLiteral("org.example.One")));
  EXPECT_EQ(service_item_counts.value(QStringLiteral("org.example.Two")), 1);
}

TEST(TrayWatcher, OptionalMissingPixmapPropertyDoesNotNeedWarning) {
  EXPECT_TRUE(isOptionalTrayPropertyMissing(QStringLiteral("IconPixmap"),
                                            QDBusError::errorString(QDBusError::InvalidArgs),
                                            QStringLiteral("No such property \"IconPixmap\"")));
}

TEST(TrayWatcher, OptionalMissingIconNamePropertyDoesNotNeedWarning) {
  EXPECT_TRUE(isOptionalTrayPropertyMissing(QStringLiteral("IconName"),
                                            QStringLiteral("org.freedesktop.DBus.Error.Failed"),
                                            QStringLiteral("error occurred in Get")));
  EXPECT_TRUE(isOptionalTrayPropertyMissing(QStringLiteral("AttentionIconName"),
                                            QDBusError::errorString(QDBusError::InvalidArgs),
                                            QStringLiteral("No such property \"AttentionIconName\"")));
}

TEST(TrayWatcher, RequiredMissingPropertyStillNeedsWarning) {
  EXPECT_FALSE(isOptionalTrayPropertyMissing(QStringLiteral("Status"), QDBusError::errorString(QDBusError::InvalidArgs),
                                             QStringLiteral("No such property \"Status\"")));
}

TEST(TrayWatcher, OptionalPropertyWithUnrelatedFailureStillNeedsWarning) {
  EXPECT_FALSE(isOptionalTrayPropertyMissing(QStringLiteral("IconPixmap"), QStringLiteral("org.example.Error"),
                                             QStringLiteral("service timed out")));
}

TEST(TrayWatcher, DiscoversExistingStatusNotifierItemsWhenBecomingWatcher) {
  TrayModel model;
  auto registration = std::make_unique<FakeTrayRegistrationClient>(QStringList(
      {QStringLiteral("org.kde.StatusNotifierItem-9999-1"), QStringLiteral("org.unrelated.Service"),
       QStringLiteral("org.kde.StatusNotifierWatcher"), QStringLiteral("org.kde.StatusNotifierHost-test")}));
  TrayWatcher watcher(&model, std::move(registration));

  watcher.start();

  EXPECT_EQ(watcher.registeredItems(),
            QStringList({QStringLiteral("org.kde.StatusNotifierItem-9999-1:/StatusNotifierItem")}));
}

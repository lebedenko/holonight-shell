#pragma once

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusServiceWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <memory>

class ItemPropWatcher;
class ItemSignalWatcher;
class TrayModel;

struct TrayItemAddress {
  QString service;
  QString path;
};

[[nodiscard]] QString normaliseTrayItemKey(const QString& raw);
[[nodiscard]] TrayItemAddress splitTrayItemKey(const QString& key);
[[nodiscard]] bool registerTrayItemKey(QStringList& registered_items, QSet<QString>& registered_item_keys,
                                       QHash<QString, int>& service_item_counts, const QString& raw);
[[nodiscard]] bool unregisterTrayItemKey(QStringList& registered_items, QSet<QString>& registered_item_keys,
                                         QHash<QString, int>& service_item_counts, const QString& raw);
[[nodiscard]] QStringList unregisterTrayServiceItems(QStringList& registered_items, QSet<QString>& registered_item_keys,
                                                     QHash<QString, int>& service_item_counts, const QString& service);
[[nodiscard]] bool isOptionalTrayPropertyMissing(const QString& prop_name, const QString& error_name,
                                                 const QString& error_message);

enum class TrayWatcherRegistrationResult : uint8_t { Failed, Registered, AlreadyQueued };

class TrayRegistrationClient {
 public:
  virtual ~TrayRegistrationClient() = default;

  TrayRegistrationClient(const TrayRegistrationClient&) = delete;
  TrayRegistrationClient& operator=(const TrayRegistrationClient&) = delete;
  TrayRegistrationClient(TrayRegistrationClient&&) = delete;
  TrayRegistrationClient& operator=(TrayRegistrationClient&&) = delete;

  virtual bool registerHost(const QString& service) = 0;
  virtual TrayWatcherRegistrationResult registerWatcher(QObject* watcher) = 0;
  [[nodiscard]] virtual QStringList registeredServices() = 0;

 protected:
  TrayRegistrationClient() = default;
};

using TrayRegistrationClientPtr = std::unique_ptr<TrayRegistrationClient>;

class TrayWatcher : public QObject, protected QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierWatcher")
  Q_PROPERTY(QStringList RegisteredStatusNotifierItems READ registeredItems)  // NOLINT(readability-identifier-naming)
  Q_PROPERTY(bool IsStatusNotifierHostRegistered READ isHostRegistered)       // NOLINT(readability-identifier-naming)
  Q_PROPERTY(int ProtocolVersion READ protocolVersion)                        // NOLINT(readability-identifier-naming)

 public:
  explicit TrayWatcher(TrayModel* model, QObject* parent = nullptr);
  TrayWatcher(TrayModel* model, TrayRegistrationClientPtr registration, QObject* parent = nullptr);

  [[nodiscard]] QStringList registeredItems() const { return registered_items_; }
  [[nodiscard]] bool isHostRegistered() const { return host_registered_; }
  [[nodiscard]] static int protocolVersion() { return 0; }
  ~TrayWatcher() override = default;

  void start();

  TrayWatcher(const TrayWatcher&) = delete;
  TrayWatcher& operator=(const TrayWatcher&) = delete;
  TrayWatcher(TrayWatcher&&) = delete;
  TrayWatcher& operator=(TrayWatcher&&) = delete;

 public Q_SLOTS:
  // Names are PascalCase per the SNI D-Bus protocol — cannot use camelCase.
  void RegisterStatusNotifierItem(const QString& service_or_path);  // NOLINT(readability-identifier-naming)
  void RegisterStatusNotifierHost(const QString& service);          // NOLINT(readability-identifier-naming)

 Q_SIGNALS:
  // NOLINT(readability-identifier-naming) — protocol-mandated PascalCase signal names
  void StatusNotifierItemRegistered(const QString& service);    // NOLINT(readability-identifier-naming)
  void StatusNotifierItemUnregistered(const QString& service);  // NOLINT(readability-identifier-naming)
  void StatusNotifierHostRegistered();                          // NOLINT(readability-identifier-naming)
  void StatusNotifierHostUnregistered();                        // NOLINT(readability-identifier-naming)

 private Q_SLOTS:
  void onServiceUnregistered(const QString& service);
  void onItemRegisteredSignal(const QString& service);
  void onItemUnregisteredSignal(const QString& service);

 public:
  // Called by ItemPropWatcher to route PropertiesChanged back with the item key.
  void onItemPropertiesChanged(const QString& key, const QVariantMap& changed);
  // Called by ItemSignalWatcher slots to route inline-value signals (e.g. NewStatus).
  void onItemDirectSignal(const QString& key, const QString& prop_name, const QVariant& value);
  // Called by ItemSignalWatcher slots that require a targeted Get round-trip.
  void onItemNeedsPropertyFetch(const QString& key, const QString& prop_name);
  void onItemNeedsToolTipFetch(const QString& key);

 private:
  void registerHost();
  void tryRegisterWatcher();
  void discoverExistingItems();
  void fallbackReadItems();
  void addItem(const QString& service_and_path);
  void removeItem(const QString& service_and_path);
  void fetchItemProperties(const QString& key, const QString& service, const QString& path);
  void updateItemProperties(const QString& key, const QVariantMap& changed);
  void subscribeItemSignals(const QString& key, const QString& service, const QString& path);
  void fetchSingleProperty(const QString& key, const QString& service, const QString& path, const QString& prop_name);
  void fetchToolTip(const QString& key, const QString& service, const QString& path);

  QDBusConnection bus_;
  TrayModel* model_;
  TrayRegistrationClientPtr registration_;
  QDBusServiceWatcher* service_watcher_{nullptr};
  bool is_watcher_{false};
  bool host_registered_{false};
  QStringList registered_items_;
  QSet<QString> registered_item_keys_;
  QHash<QString, ItemPropWatcher*> property_watchers_;
  QHash<QString, ItemSignalWatcher*> signal_watchers_;
  QHash<QString, int> service_item_counts_;
  bool started_{false};
};

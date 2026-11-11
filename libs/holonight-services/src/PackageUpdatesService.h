#pragma once
#include <QDBusConnection>
#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>
class ApplicationLaunchService;
class PackageUpdatesService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool available READ available NOTIFY changed)
  Q_PROPERTY(bool loading READ loading NOTIFY changed)
  Q_PROPERTY(bool checking READ checking NOTIFY changed)
  Q_PROPERTY(quint32 count READ count NOTIFY changed)
  Q_PROPERTY(quint32 ignoredCount READ ignoredCount NOTIFY changed)
  Q_PROPERTY(quint64 downloadSizeBytes READ downloadSizeBytes NOTIFY changed)
  Q_PROPERTY(qint64 dataAsOf READ dataAsOf NOTIFY changed)
  Q_PROPERTY(qint64 lastCheckTime READ lastCheckTime NOTIFY changed)
  Q_PROPERTY(qint64 snapshotFetchedAt READ snapshotFetchedAt NOTIFY changed)
  Q_PROPERTY(bool checkMetadataAvailable READ checkMetadataAvailable NOTIFY changed)
  Q_PROPERTY(bool lastCheckSucceeded READ lastCheckSucceeded NOTIFY changed)
  Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
  Q_PROPERTY(QString lastCheckError READ lastCheckError NOTIFY changed)
  Q_PROPERTY(QString launchError READ launchError NOTIFY changed)
 public:
  explicit PackageUpdatesService(QObject* parent = nullptr);
  PackageUpdatesService(const QDBusConnection& bus, QObject* parent);
  void start();
  void setApplicationLaunchService(ApplicationLaunchService* service) { launcher_ = service; }
  Q_INVOKABLE void openPackages();
  [[nodiscard]] bool available() const { return available_; }
  [[nodiscard]] bool loading() const { return loading_; }
  [[nodiscard]] bool checking() const { return checking_; }
  [[nodiscard]] quint32 count() const { return count_; }
  [[nodiscard]] quint32 ignoredCount() const { return ignored_count_; }
  [[nodiscard]] quint64 downloadSizeBytes() const { return download_size_bytes_; }
  [[nodiscard]] qint64 dataAsOf() const { return data_as_of_; }
  [[nodiscard]] qint64 lastCheckTime() const { return last_check_time_; }
  [[nodiscard]] qint64 snapshotFetchedAt() const { return snapshot_fetched_at_; }
  [[nodiscard]] bool checkMetadataAvailable() const { return check_metadata_available_; }
  [[nodiscard]] bool lastCheckSucceeded() const { return last_check_succeeded_; }
  [[nodiscard]] QString errorMessage() const { return error_message_; }
  [[nodiscard]] QString lastCheckError() const { return last_check_error_; }
  [[nodiscard]] QString launchError() const { return launch_error_; }
 signals:
  void changed();
 private slots:
  void ownerChanged(const QString&, const QString&, const QString& owner);
  void propertiesChanged(const QString& interface, const QVariantMap&, const QStringList&);

 private:
  void setOwner(const QString& owner);
  void fetch(bool check);
  QDBusConnection bus_;
  QString owner_;
  quint64 generation_ = 0;
  quint64 requests_[2] = {};
  bool started_ = false;
  bool has_results_ = false;
  bool launch_pending_ = false;
  ApplicationLaunchService* launcher_ = nullptr;
  bool available_ = false;
  bool loading_ = false;
  bool checking_ = false;
  quint32 count_ = 0;
  quint32 ignored_count_ = 0;
  quint64 download_size_bytes_ = 0;
  qint64 data_as_of_ = 0;
  qint64 last_check_time_ = 0;
  qint64 snapshot_fetched_at_ = 0;
  bool check_metadata_available_ = false;
  bool last_check_succeeded_ = false;
  QString error_message_ = {};
  QString last_check_error_ = {};
  QString launch_error_ = {};
};

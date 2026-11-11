#include "PackageUpdatesService.h"

#include "ApplicationLaunchService.h"

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
namespace {
const QString service = QStringLiteral("org.holonight.Packages1");
const QString path = QStringLiteral("/org/holonight/Packages1");
const QString updates = service + QStringLiteral(".Updates");
const QString checks = service + QStringLiteral(".UpdateCheck");
}  // namespace
PackageUpdatesService::PackageUpdatesService(QObject* parent)
    : PackageUpdatesService(QDBusConnection::sessionBus(), parent) {}
PackageUpdatesService::PackageUpdatesService(const QDBusConnection& bus, QObject* parent)
    : QObject(parent), bus_(bus) {}
void PackageUpdatesService::start() {
  if (started_) return;
  started_ = true;
  auto* watcher = new QDBusServiceWatcher(service, bus_, QDBusServiceWatcher::WatchForOwnerChange, this);
  connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &PackageUpdatesService::ownerChanged);
  auto activation =
      QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                                     QStringLiteral("org.freedesktop.DBus"), QStringLiteral("StartServiceByName"));
  activation << service << quint32(0);
  bus_.asyncCall(activation);
  auto query =
      QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                                     QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetNameOwner"));
  query << service;
  const auto generation = generation_;
  auto* pending = new QDBusPendingCallWatcher(bus_.asyncCall(query), this);
  connect(pending, &QDBusPendingCallWatcher::finished, this, [this, generation, pending] {
    QDBusPendingReply<QString> reply = *pending;
    pending->deleteLater();
    if (generation == generation_ && !reply.isError()) setOwner(reply.value());
  });
}
void PackageUpdatesService::ownerChanged(const QString&, const QString&, const QString& owner) { setOwner(owner); }
void PackageUpdatesService::setOwner(const QString& owner) {
  if (!owner_.isEmpty())
    bus_.disconnect(owner_, path, QStringLiteral("org.freedesktop.DBus.Properties"),
                    QStringLiteral("PropertiesChanged"), this,
                    SLOT(propertiesChanged(QString, QVariantMap, QStringList)));
  ++generation_;
  owner_ = owner;
  available_ = !owner.isEmpty();
  loading_ = available_;
  has_results_ = false;
  checking_ = false;
  count_ = ignored_count_ = 0;
  download_size_bytes_ = 0;
  data_as_of_ = last_check_time_ = snapshot_fetched_at_ = 0;
  check_metadata_available_ = last_check_succeeded_ = false;
  error_message_.clear();
  last_check_error_.clear();
  emit changed();
  if (!available_) return;
  bus_.connect(owner_, path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"),
               this, SLOT(propertiesChanged(QString, QVariantMap, QStringList)));
  fetch(false);
  fetch(true);
}
void PackageUpdatesService::propertiesChanged(const QString& interface, const QVariantMap&, const QStringList&) {
  if (interface == updates || interface == checks) fetch(interface == checks);
}
void PackageUpdatesService::fetch(bool check) {
  const auto generation = generation_;
  const auto request = ++requests_[check];
  auto message = QDBusMessage::createMethodCall(owner_, path, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                QStringLiteral("GetAll"));
  message << (check ? checks : updates);
  auto* pending = new QDBusPendingCallWatcher(bus_.asyncCall(message), this);
  connect(pending, &QDBusPendingCallWatcher::finished, this, [this, pending, generation, request, check] {
    QDBusPendingReply<QVariantMap> reply = *pending;
    pending->deleteLater();
    if (generation != generation_ || request != requests_[check]) return;
    const auto values = reply.value();
    if (check) {
      check_metadata_available_ = !reply.isError() && values.value("Checking").metaType().id() == QMetaType::Bool &&
                                  values.value("LastCheckSucceeded").metaType().id() == QMetaType::Bool;
      if (check_metadata_available_) {
        checking_ = values.value("Checking").toBool();
        last_check_succeeded_ = values.value("LastCheckSucceeded").toBool();
        last_check_time_ = values.value("LastCheckTime").toLongLong();
        snapshot_fetched_at_ = values.value("SnapshotFetchedAt").toLongLong();
        last_check_error_ = values.value("LastCheckError").toString();
      } else {
        checking_ = false;
        last_check_succeeded_ = false;
        last_check_time_ = snapshot_fetched_at_ = 0;
        last_check_error_.clear();
      }
    } else {
      loading_ = false;
      const auto state = values.value("State").toString();
      if (reply.isError() || (state != "ready" && state != "loading" && state != "error" && state != "no-databases") ||
          values.value("Count").metaType().id() != QMetaType::UInt ||
          values.value("IgnoredCount").metaType().id() != QMetaType::UInt ||
          values.value("DownloadSizeBytes").metaType().id() != QMetaType::ULongLong ||
          values.value("DataAsOf").metaType().id() != QMetaType::LongLong) {
        error_message_ = reply.isError() ? reply.error().message() : tr("Invalid package update response");
      } else {
        loading_ = state == "loading";
        if (state == "ready" || (!has_results_ && state != "loading")) {
          has_results_ = true;
          count_ = values.value("Count").toUInt();
          ignored_count_ = values.value("IgnoredCount").toUInt();
          download_size_bytes_ = values.value("DownloadSizeBytes").toULongLong();
          data_as_of_ = values.value("DataAsOf").toLongLong();
        }
        error_message_ =
            state == "no-databases" ? tr("Package databases are missing") : values.value("LastError").toString();
        if (state == "error" && error_message_.isEmpty()) error_message_ = tr("Package evaluation failed");
      }
    }
    emit changed();
  });
}
void PackageUpdatesService::openPackages() {
  if (launch_pending_) return;
  launch_pending_ = true;
  ApplicationLaunchService fallback;
  auto* launcher = launcher_ ? launcher_ : &fallback;
  launcher->launch({.program = QStringLiteral("holonight-packages")}, this,
                   [this](const QString&, const QString& error) {
                     launch_pending_ = false;
                     launch_error_ = error;
                     emit changed();
                   });
}

#pragma once

#include <QDBusPendingCall>
#include <QDBusServiceWatcher>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <utility>

class QObject;

// Test seam: abstracts session-bus D-Bus calls so PortalService unit tests never touch the real bus.
class IPortalDBus {
 public:
  virtual ~IPortalDBus() = default;
  IPortalDBus() = default;
  IPortalDBus(const IPortalDBus&) = delete;
  IPortalDBus& operator=(const IPortalDBus&) = delete;
  IPortalDBus(IPortalDBus&&) = delete;
  IPortalDBus& operator=(IPortalDBus&&) = delete;

  virtual QDBusPendingCall nameHasOwner(const QString& service_name) = 0;
  virtual QDBusPendingCall introspectPortal() = 0;
  virtual QDBusPendingCall listNames() = 0;
  virtual QDBusPendingCall readSetting(const QString& portal_namespace, const QString& key) = 0;
  virtual bool connectSettingChanged(QObject* receiver, const char* slot) = 0;
  virtual bool connectNameOwnerChanged(QObject* receiver, const char* slot) = 0;
  virtual QDBusPendingCall openFile(const QString& window_handle, const QString& title, const QStringList& filters) = 0;
  virtual QDBusPendingCall openUri(const QString& uri) = 0;
  virtual QDBusServiceWatcher* createServiceWatcher(const QString& service, QDBusServiceWatcher::WatchModeFlag mode,
                                                    QObject* parent) = 0;
};

// Production implementation: delegates every call to the real session bus.
class SystemPortalDBus final : public IPortalDBus {
 public:
  static constexpr int kDefaultProbeTimeoutMs = 5000;

  explicit SystemPortalDBus(int probe_timeout_ms = kDefaultProbeTimeoutMs,
                            QString portal_service = QStringLiteral("org.freedesktop.portal.Desktop"))
      : probe_timeout_ms_(probe_timeout_ms), portal_service_(std::move(portal_service)) {}

  QDBusPendingCall nameHasOwner(const QString& service_name) override;
  QDBusPendingCall introspectPortal() override;
  QDBusPendingCall listNames() override;
  QDBusPendingCall readSetting(const QString& portal_namespace, const QString& key) override;
  bool connectSettingChanged(QObject* receiver, const char* slot) override;
  bool connectNameOwnerChanged(QObject* receiver, const char* slot) override;
  QDBusPendingCall openFile(const QString& window_handle, const QString& title, const QStringList& filters) override;
  QDBusPendingCall openUri(const QString& uri) override;
  QDBusServiceWatcher* createServiceWatcher(const QString& service, QDBusServiceWatcher::WatchModeFlag mode,
                                            QObject* parent) override;

 private:
  int probe_timeout_ms_;
  QString portal_service_;
};

// Test stub: returns immediately-completed pending calls without hitting the bus.
class NullPortalDBus final : public IPortalDBus {
 public:
  void setNameHasOwner(bool val) { name_has_owner_ = val; }
  void setInterfaces(const QStringList& ifaces);
  void setNameList(const QStringList& names) { names_ = names; }
  void setSettingResult(const QString& key, const QVariant& value) { settings_[key] = value; }
  void setSettingError(const QString& key) { setting_errors_.insert(key); }

  [[nodiscard]] int openFileCalls() const { return open_file_calls_; }
  [[nodiscard]] int openUriCalls() const { return open_uri_calls_; }
  [[nodiscard]] QStringList lastOpenFileFilters() const { return last_open_file_filters_; }

  QDBusPendingCall nameHasOwner(const QString& /*service_name*/) override;
  QDBusPendingCall introspectPortal() override;
  QDBusPendingCall listNames() override;
  QDBusPendingCall readSetting(const QString& /*portal_namespace*/, const QString& key) override;
  bool connectSettingChanged(QObject* /*receiver*/, const char* /*slot*/) override { return true; }
  bool connectNameOwnerChanged(QObject* /*receiver*/, const char* /*slot*/) override { return true; }
  QDBusPendingCall openFile(const QString& /*window_handle*/, const QString& /*title*/,
                            const QStringList& filters) override;
  QDBusPendingCall openUri(const QString& /*uri*/) override;
  QDBusServiceWatcher* createServiceWatcher(const QString& /*service*/, QDBusServiceWatcher::WatchModeFlag mode,
                                            QObject* parent) override;

 private:
  static QDBusPendingCall makeReply(const QVariantList& args);
  static QDBusPendingCall makeErrorReply(const QString& error_name, const QString& msg);
  static QString buildIntrospectXml(const QStringList& ifaces);

  bool name_has_owner_{false};
  QString introspect_xml_;
  QStringList names_;
  QHash<QString, QVariant> settings_;
  QSet<QString> setting_errors_;
  int open_file_calls_{0};
  int open_uri_calls_{0};
  QStringList last_open_file_filters_;
};

#pragma once

#include <QObject>
#include <QString>

class QDBusMessage;
class QDBusPendingCall;

class SettingsNavigationService : public QObject {
  Q_OBJECT

 public:
  explicit SettingsNavigationService(QObject* parent = nullptr);
  ~SettingsNavigationService() override = default;

  SettingsNavigationService(const SettingsNavigationService&) = delete;
  SettingsNavigationService& operator=(const SettingsNavigationService&) = delete;
  SettingsNavigationService(SettingsNavigationService&&) = delete;
  SettingsNavigationService& operator=(SettingsNavigationService&&) = delete;

  Q_INVOKABLE void openPage(const QString& page_key);
  [[nodiscard]] static QDBusMessage openPageMessage(const QString& page_key);

 protected:
  virtual QDBusPendingCall requestOpenPage(const QString& page_key);
};

#pragma once

#include <QDBusConnection>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QString>

class ConfigService;

class SystemInfoService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QString name READ name CONSTANT)
  Q_PROPERTY(QString displayName READ displayName CONSTANT)
  Q_PROPERTY(QString logoIconName READ logoIconName CONSTANT)
  Q_PROPERTY(QString logoSource READ logoSource CONSTANT)
  Q_PROPERTY(bool logoTinted READ logoTinted CONSTANT)
  Q_PROPERTY(QString avatarPath READ avatarPath CONSTANT)
  Q_PROPERTY(QString userName READ userName CONSTANT)
  Q_PROPERTY(QString realName READ realName CONSTANT)

 public:
  explicit SystemInfoService(ConfigService* config = nullptr, QObject* parent = nullptr);
  ~SystemInfoService() override = default;

  SystemInfoService(const SystemInfoService&) = delete;
  SystemInfoService& operator=(const SystemInfoService&) = delete;
  SystemInfoService(SystemInfoService&&) = delete;
  SystemInfoService& operator=(SystemInfoService&&) = delete;

  [[nodiscard]] QString name() const { return name_; }
  [[nodiscard]] QString displayName() const { return display_name_; }
  [[nodiscard]] QString logoIconName() const { return logo_icon_name_; }
  [[nodiscard]] QString logoSource() const { return logo_source_; }
  [[nodiscard]] bool logoTinted() const { return logo_tinted_; }
  [[nodiscard]] QString avatarPath() const { return avatar_path_; }
  [[nodiscard]] QString userName() const { return user_name_; }
  [[nodiscard]] QString realName() const { return real_name_; }

  // Test seam: overrides the D-Bus connection readAccountsService() queries (defaults to the
  // system bus), mirroring QtDbusPropertyClient::setDbusConnection/resetDbusConnection.
  static void setDbusConnection(const QDBusConnection& connection);
  static void resetDbusConnection();

 private:
  void readOsRelease();
  void readAccountsService();
  [[nodiscard]] static QString resolveThemeLogoIconName(const QString& icon_name);
  // Applies the [logo] config precedence (steps 1-3: file override, generic flag, distro alias
  // table). Returns true if it resolved logo_source_/logo_tinted_ itself, in which case
  // readOsRelease() skips the pixmaps/icon-theme fallback chain (steps 4-5).
  [[nodiscard]] bool applyLogoConfigOverride(const QHash<QString, QString>& os_release);

  ConfigService* config_{nullptr};
  QString name_;
  QString display_name_;
  QString logo_icon_name_;
  QString logo_source_;
  bool logo_tinted_{false};
  QString avatar_path_;
  QString user_name_;
  QString real_name_;
};

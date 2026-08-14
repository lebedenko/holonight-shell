#pragma once

#include "AudioService.h"
#include "BatteryService.h"
#include "BatteryState.h"
#include "CompositorService.h"
#include "ConfigService.h"
#include "DbusPropertyClient.h"
#include "GeneratedQmlFiles.h"
#include "MprisDbus.h"
#include "MprisService.h"
#include "PowerProfilesService.h"
#include "SuspendInhibitorService.h"
#include "TrayModel.h"
#include "WeatherIconBridge.h"
#include "WifiNetworkModel.h"

#include <QColor>
#include <QCoreApplication>
#include <QDBusObjectPath>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QStringListModel>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QtQml/qqml.h>

#include <memory>

class NullDbusPropertyClient final : public DbusPropertyClient {
 public:
  [[nodiscard]] bool systemBusConnected() const override { return false; }
  [[nodiscard]] bool serviceRegistered(const QString& /*service*/) const override { return false; }

  [[nodiscard]] std::optional<QVariant> property(const QString& /*service*/, const QString& /*path*/,
                                                 const QString& /*interface*/, const QString& /*name*/) const override {
    return std::nullopt;
  }

  [[nodiscard]] std::optional<QVariantMap> allProperties(const QString& /*service*/, const QString& /*path*/,
                                                         const QString& /*interface*/) const override {
    return std::nullopt;
  }

  [[nodiscard]] std::optional<QList<QDBusObjectPath>> upowerDevices() const override { return std::nullopt; }

  bool connectSignal(const QString& /*service*/, const QString& /*path*/, const QString& /*interface*/,
                     const QString& /*signal*/, QObject* /*receiver*/, const char* /*slot*/) const override {
    return false;
  }

  bool disconnectSignal(const QString& /*service*/, const QString& /*path*/, const QString& /*interface*/,
                        const QString& /*signal*/, QObject* /*receiver*/, const char* /*slot*/) const override {
    return true;
  }
};

// NOLINTBEGIN(readability-convert-member-functions-to-static, modernize-return-braced-init-list)
class FakeKeyboardLayoutService : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString layoutCode READ layoutCode NOTIFY layoutCodeChanged)

 public:
  [[nodiscard]] QString layoutCode() const { return layout_code_; }

  void setLayoutCode(const QString& value) {
    if (layout_code_ == value) {
      return;
    }
    layout_code_ = value;
    emit layoutCodeChanged();
  }

 Q_SIGNALS:
  void layoutCodeChanged();

 private:
  QString layout_code_{QStringLiteral("EN")};
};

class FakeNetworkService : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool available READ available NOTIFY availableChanged)
  Q_PROPERTY(bool online READ online CONSTANT)
  Q_PROPERTY(int type READ type CONSTANT)
  Q_PROPERTY(bool vpnActive READ vpnActive CONSTANT)
  Q_PROPERTY(int strength READ strength CONSTANT)
  Q_PROPERTY(QString ssid READ ssid CONSTANT)
  Q_PROPERTY(bool wifiEnabled READ wifiEnabled CONSTANT)
  Q_PROPERTY(bool wifiHardwareEnabled READ wifiHardwareEnabled CONSTANT)
  Q_PROPERTY(bool scanning READ scanning CONSTANT)
  Q_PROPERTY(QString activeConnectionName READ activeConnectionName CONSTANT)
  Q_PROPERTY(QString activeIp4Address READ activeIp4Address CONSTANT)
  Q_PROPERTY(uint activeFrequencyMhz READ activeFrequencyMhz CONSTANT)
  Q_PROPERTY(uint activeLinkSpeedMbps READ activeLinkSpeedMbps CONSTANT)
  Q_PROPERTY(QString downloadSpeedText READ downloadSpeedText CONSTANT)
  Q_PROPERTY(QString uploadSpeedText READ uploadSpeedText CONSTANT)
  Q_PROPERTY(QString connectionStatus READ connectionStatus CONSTANT)
  Q_PROPERTY(QString lastError READ lastError CONSTANT)
  Q_PROPERTY(QAbstractItemModel* wifiNetworks READ wifiNetworks CONSTANT)

 public:
  FakeNetworkService() {
    networks_.setNetworks({
        WifiNetwork{
            .ssid = QStringLiteral("Test Wi-Fi"),
            .strength = 82,
            .secured = true,
            .known = true,
            .connected = true,
            .active = true,
            .access_point_path = QStringLiteral("/ap/1"),
            .device_path = QStringLiteral("/dev/wlan0"),
            .connection_path = QStringLiteral("/settings/1"),
            .frequency = 5200,
            .status_text = QStringLiteral("Connected"),
        },
    });
  }

  [[nodiscard]] bool available() const { return available_; }
  [[nodiscard]] bool online() const { return true; }
  [[nodiscard]] int type() const { return 1; }
  [[nodiscard]] bool vpnActive() const { return true; }
  [[nodiscard]] int strength() const { return 82; }
  [[nodiscard]] QString ssid() const { return QStringLiteral("Test Wi-Fi"); }
  [[nodiscard]] bool wifiEnabled() const { return true; }
  [[nodiscard]] bool wifiHardwareEnabled() const { return true; }
  [[nodiscard]] bool scanning() const { return false; }
  [[nodiscard]] QString activeConnectionName() const { return QStringLiteral("Test Wi-Fi"); }
  [[nodiscard]] QString activeIp4Address() const { return QStringLiteral("192.0.2.10"); }
  [[nodiscard]] uint activeFrequencyMhz() const { return 5200; }
  [[nodiscard]] uint activeLinkSpeedMbps() const { return 866; }
  [[nodiscard]] QString downloadSpeedText() const { return QStringLiteral("12 Mbps"); }
  [[nodiscard]] QString uploadSpeedText() const { return QStringLiteral("2 Mbps"); }
  [[nodiscard]] QString connectionStatus() const { return QStringLiteral("Connected"); }
  [[nodiscard]] QString lastError() const { return {}; }
  [[nodiscard]] QAbstractItemModel* wifiNetworks() { return &networks_; }

  Q_INVOKABLE void rescanWifi() {}
  Q_INVOKABLE void setWifiEnabled(bool /*enabled*/) {}
  Q_INVOKABLE void connectNetwork(int /*row*/) {}
  Q_INVOKABLE void connectNetworkWithPassword(int /*row*/, const QString& /*password*/) {}
  Q_INVOKABLE void disconnectActive() {}
  Q_INVOKABLE void openNetworkSettings() {}
  Q_INVOKABLE void clearLastError() {}

  void setAvailable(bool value) {
    if (available_ == value) {
      return;
    }
    available_ = value;
    emit availableChanged();
  }

 Q_SIGNALS:
  void availableChanged();

 private:
  bool available_{true};
  WifiNetworkModel networks_;
};

class FakeWeatherService : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool hasData READ hasData NOTIFY hasDataChanged)
  Q_PROPERTY(bool stale READ stale NOTIFY staleChanged)
  Q_PROPERTY(QString locationLabel READ locationLabel NOTIFY locationChanged)
  Q_PROPERTY(QVariant current READ current NOTIFY currentChanged)

 public:
  [[nodiscard]] bool hasData() const { return has_data_; }
  [[nodiscard]] bool stale() const { return false; }
  [[nodiscard]] QString locationLabel() const { return location_label_; }
  [[nodiscard]] QVariant current() const { return current_; }

  Q_INVOKABLE QString iconPath(int /*condition_id*/, bool /*is_day*/) const {
    return QStringLiteral("qrc:/HolonightShell/weather/wsymbol_0001_sunny.svg");
  }

  void setHasData(bool value) {
    if (has_data_ == value) {
      return;
    }
    has_data_ = value;
    emit hasDataChanged();
  }

  Q_INVOKABLE void setLocationLabel(const QString& value) {
    if (location_label_ == value) {
      return;
    }
    location_label_ = value;
    emit locationChanged();
  }

  Q_INVOKABLE void setCurrentTemperature(double value) {
    QVariantMap map = current_.toMap();
    map[QStringLiteral("temperature")] = value;
    current_ = map;
    emit currentChanged();
  }

 Q_SIGNALS:
  void hasDataChanged();
  void staleChanged();
  void locationChanged();
  void currentChanged();

 private:
  bool has_data_{true};
  QString location_label_{QStringLiteral("Lviv, Ukraine")};
  QVariant current_{QVariantMap{{QStringLiteral("temperature"), 18.0},
                                {QStringLiteral("feelsLike"), 17.0},
                                {QStringLiteral("condition"), QStringLiteral("clear sky")},
                                {QStringLiteral("conditionId"), 800},
                                {QStringLiteral("sunrise"), 0},
                                {QStringLiteral("sunset"), QVariant::fromValue<qint64>(4102444800)}}};
};

class FakeBrightnessService : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool hasBacklight READ hasBacklight CONSTANT)
  Q_PROPERTY(int brightnessPercent READ brightnessPercent NOTIFY brightnessPercentChanged)
  Q_PROPERTY(int writeCount READ writeCount NOTIFY writesChanged)
  Q_PROPERTY(int lastWritten READ lastWritten NOTIFY writesChanged)

 public:
  [[nodiscard]] bool hasBacklight() const { return true; }
  [[nodiscard]] int brightnessPercent() const { return brightness_percent_; }
  [[nodiscard]] int writeCount() const { return write_count_; }
  [[nodiscard]] int lastWritten() const { return last_written_; }

  Q_INVOKABLE void setBrightnessPercent(int value) {
    brightness_percent_ = value;
    last_written_ = value;
    ++write_count_;
    emit brightnessPercentChanged(value);
    emit writesChanged();
  }

  Q_INVOKABLE void resetWrites() {
    write_count_ = 0;
    last_written_ = 0;
    emit writesChanged();
  }

 Q_SIGNALS:
  void brightnessPercentChanged(int percent);
  void writesChanged();

 private:
  int brightness_percent_{50};
  int write_count_{0};
  int last_written_{0};
};

class FakeSessionService : public QObject {
  Q_OBJECT

 public:
  Q_INVOKABLE void lockScreen() {}
  Q_INVOKABLE void logout() {}
  Q_INVOKABLE void sleep() {}
  Q_INVOKABLE void reboot() {}
  Q_INVOKABLE void shutdown() {}
};

class FakeSystemInfoService : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString name READ name CONSTANT)
  Q_PROPERTY(QString displayName READ displayName CONSTANT)
  Q_PROPERTY(QString logoIconName READ logoIconName CONSTANT)
  Q_PROPERTY(QString logoSource READ logoSource NOTIFY logoChanged)
  Q_PROPERTY(bool logoTinted READ logoTinted NOTIFY logoChanged)

 public:
  [[nodiscard]] QString name() const { return QStringLiteral("Holonight Test"); }
  [[nodiscard]] QString displayName() const { return QStringLiteral("Test"); }
  [[nodiscard]] QString logoIconName() const { return QStringLiteral("computer-symbolic"); }
  [[nodiscard]] QString logoSource() const { return logo_source_; }
  [[nodiscard]] bool logoTinted() const { return logo_tinted_; }

  Q_INVOKABLE void setLogo(const QString& source, bool tinted) {
    logo_source_ = source;
    logo_tinted_ = tinted;
    emit logoChanged();
  }

 Q_SIGNALS:
  void logoChanged();

 private:
  QString logo_source_{QStringLiteral("qrc:/HolonightShell/linux-logo/archlinux.svg")};
  bool logo_tinted_{true};
};

class FakeAppearanceService : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString scheme READ scheme CONSTANT)
  Q_PROPERTY(QString accent READ accent CONSTANT)
  Q_PROPERTY(QString colorMode READ colorMode CONSTANT)
  Q_PROPERTY(QString uiFont READ uiFont CONSTANT)
  Q_PROPERTY(QString monospaceFont READ monospaceFont CONSTANT)
  Q_PROPERTY(QString titleFont READ titleFont CONSTANT)
  Q_PROPERTY(QString displayFont READ displayFont CONSTANT)
  Q_PROPERTY(int uiFontSize READ uiFontSize CONSTANT)
  Q_PROPERTY(int monospaceFontSize READ monospaceFontSize CONSTANT)
  Q_PROPERTY(int titleFontSize READ titleFontSize CONSTANT)
  Q_PROPERTY(int displayFontSize READ displayFontSize CONSTANT)
  Q_PROPERTY(QString iconTheme READ iconTheme CONSTANT)
  Q_PROPERTY(QString fallbackIconTheme READ fallbackIconTheme CONSTANT)
  Q_PROPERTY(QString cursorTheme READ cursorTheme CONSTANT)
  Q_PROPERTY(qreal layoutScale READ layoutScale CONSTANT)
  Q_PROPERTY(QString shapeStyle READ shapeStyle CONSTANT)
  Q_PROPERTY(qreal shapeScale READ shapeScale CONSTANT)
  Q_PROPERTY(bool hasBaseRadius READ hasBaseRadius CONSTANT)
  Q_PROPERTY(qreal baseRadius READ baseRadius CONSTANT)
  Q_PROPERTY(bool hasBaseChamfer READ hasBaseChamfer CONSTANT)
  Q_PROPERTY(qreal baseChamfer READ baseChamfer CONSTANT)
  Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
  Q_PROPERTY(bool debugOverlays READ debugOverlays CONSTANT)

 public:
  [[nodiscard]] QString scheme() const { return QStringLiteral("holonight-dark"); }
  [[nodiscard]] QString accent() const { return QStringLiteral("blue"); }
  [[nodiscard]] QString colorMode() const { return QStringLiteral("dark"); }
  [[nodiscard]] QString uiFont() const { return QStringLiteral("Sans"); }
  [[nodiscard]] QString monospaceFont() const { return QStringLiteral("Monospace"); }
  [[nodiscard]] QString titleFont() const { return QStringLiteral("Sans"); }
  [[nodiscard]] QString displayFont() const { return QStringLiteral("Sans"); }
  [[nodiscard]] int uiFontSize() const { return 12; }
  [[nodiscard]] int monospaceFontSize() const { return 12; }
  [[nodiscard]] int titleFontSize() const { return 8; }
  [[nodiscard]] int displayFontSize() const { return 20; }
  [[nodiscard]] QString iconTheme() const { return QStringLiteral("HoloNight"); }
  [[nodiscard]] QString fallbackIconTheme() const { return QStringLiteral("hicolor"); }
  [[nodiscard]] QString cursorTheme() const { return QStringLiteral("default"); }
  [[nodiscard]] qreal layoutScale() const { return 1.0; }
  [[nodiscard]] QString shapeStyle() const { return QStringLiteral("inherit"); }
  [[nodiscard]] qreal shapeScale() const { return 1.0; }
  [[nodiscard]] bool hasBaseRadius() const { return false; }
  [[nodiscard]] qreal baseRadius() const { return 0.0; }
  [[nodiscard]] bool hasBaseChamfer() const { return false; }
  [[nodiscard]] qreal baseChamfer() const { return 0.0; }
  [[nodiscard]] int revision() const { return 0; }
  [[nodiscard]] bool debugOverlays() const { return false; }

 Q_SIGNALS:
  void revisionChanged();
};

class FakePopupSurface : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool popupVisible READ popupVisible NOTIFY popupVisibleChanged)

 public:
  [[nodiscard]] bool popupVisible() const { return false; }
  Q_INVOKABLE void show(const QString& /*monitor_name*/) {}
  Q_INVOKABLE void hide() {}

 Q_SIGNALS:
  void popupVisibleChanged();
};

class FakeStatusPopupSurface : public QObject {
  Q_OBJECT
  Q_PROPERTY(int pointerX READ pointerX CONSTANT)
  Q_PROPERTY(int hideCount READ hideCount NOTIFY hideCountChanged)

 public:
  [[nodiscard]] int pointerX() const { return 120; }
  [[nodiscard]] int hideCount() const { return hide_count_; }
  Q_INVOKABLE void hide() {
    ++hide_count_;
    Q_EMIT hideCountChanged();
  }

 Q_SIGNALS:
  void geometryChanged();
  void hideCountChanged();

 private:
  int hide_count_{0};
};

class FakeSettingsNavigationService : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString lastOpenedPage READ lastOpenedPage NOTIFY lastOpenedPageChanged)

 public:
  [[nodiscard]] QString lastOpenedPage() const { return last_opened_page_; }
  Q_INVOKABLE void openPage(const QString& page_key) {
    last_opened_page_ = page_key;
    Q_EMIT lastOpenedPageChanged();
  }

 Q_SIGNALS:
  void lastOpenedPageChanged();

 private:
  QString last_opened_page_;
};

class FakeTooltipSurface : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool tooltipVisible READ tooltipVisible NOTIFY tooltipVisibleChanged)
  Q_PROPERTY(QString iconName READ iconName NOTIFY tooltipChanged)
  Q_PROPERTY(QString title READ title NOTIFY tooltipChanged)
  Q_PROPERTY(QString description READ description NOTIFY tooltipChanged)
  Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY tooltipChanged)
  Q_PROPERTY(bool charging READ charging NOTIFY tooltipChanged)
  Q_PROPERTY(int signalStrength READ signalStrength NOTIFY tooltipChanged)

 public:
  [[nodiscard]] bool tooltipVisible() const { return tooltip_visible_; }
  [[nodiscard]] QString iconName() const { return icon_name_; }
  [[nodiscard]] QString title() const { return title_; }
  [[nodiscard]] QString description() const { return description_; }
  [[nodiscard]] int batteryPercent() const { return battery_percent_; }
  [[nodiscard]] bool charging() const { return charging_; }
  [[nodiscard]] int signalStrength() const { return signal_strength_; }
  Q_INVOKABLE void show(const QString& /*monitor_name*/, int /*x*/, int /*width*/, const QString& title,
                        const QString& description, const QString& icon_name, int battery_percent, bool charging,
                        int signal_strength) {
    if (title_ != title || description_ != description || icon_name_ != icon_name ||
        battery_percent_ != battery_percent || charging_ != charging || signal_strength_ != signal_strength) {
      title_ = title;
      description_ = description;
      icon_name_ = icon_name;
      battery_percent_ = battery_percent;
      charging_ = charging;
      signal_strength_ = signal_strength;
      Q_EMIT tooltipChanged();
    }
    if (!tooltip_visible_) {
      tooltip_visible_ = true;
      Q_EMIT tooltipVisibleChanged();
    }
  }
  Q_INVOKABLE void hide() {
    if (!tooltip_visible_) {
      return;
    }
    tooltip_visible_ = false;
    Q_EMIT tooltipVisibleChanged();
  }

 Q_SIGNALS:
  void tooltipVisibleChanged();
  void tooltipChanged();

 private:
  bool tooltip_visible_ = false;
  QString icon_name_;
  QString title_;
  QString description_;
  int battery_percent_ = 100;
  bool charging_ = false;
  int signal_strength_ = 100;
};

class FakeNotificationModel : public QAbstractListModel {
  Q_OBJECT
 public:
  explicit FakeNotificationModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

  enum Roles {
    NotifIdRole = Qt::UserRole + 1,
    SummaryRole,
    BodyRole,
    AppIconRole,
    ActionsRole,
    HasDefaultActionRole,
    AccentKindRole,
    UrgencyRole,
    IsResidentRole,
    MonitorRole,
    LifecycleRole,
    CreatedAtMsRole,
  };

  int rowCount(const QModelIndex& parent = {}) const override {
    Q_UNUSED(parent);
    return list_.size();
  }

  QVariant data(const QModelIndex& index, int role) const override {
    if (!index.isValid() || index.row() < 0 || index.row() >= list_.size()) {
      return {};
    }
    const auto& item = list_.at(index.row());
    switch (role) {
      case NotifIdRole:
        return item.value(QStringLiteral("notifId"));
      case SummaryRole:
        return item.value(QStringLiteral("summary"));
      case BodyRole:
        return item.value(QStringLiteral("body"));
      case AppIconRole:
        return item.value(QStringLiteral("appIcon"));
      case ActionsRole:
        return item.value(QStringLiteral("actions"));
      case HasDefaultActionRole:
        return item.value(QStringLiteral("hasDefaultAction"));
      case AccentKindRole:
        return item.value(QStringLiteral("accentKind"));
      case UrgencyRole:
        return item.value(QStringLiteral("urgency"));
      case IsResidentRole:
        return item.value(QStringLiteral("isResident"));
      case MonitorRole:
        return item.value(QStringLiteral("monitor"));
      case LifecycleRole:
        return item.value(QStringLiteral("lifecycle"));
      case CreatedAtMsRole:
        return item.value(QStringLiteral("createdAtMs"));
      default:
        return {};
    }
  }

  QHash<int, QByteArray> roleNames() const override {
    return {{NotifIdRole, "notifId"},       {SummaryRole, "summary"},     {BodyRole, "body"},
            {AppIconRole, "appIcon"},       {ActionsRole, "actions"},     {HasDefaultActionRole, "hasDefaultAction"},
            {AccentKindRole, "accentKind"}, {UrgencyRole, "urgency"},     {IsResidentRole, "isResident"},
            {MonitorRole, "monitor"},       {LifecycleRole, "lifecycle"}, {CreatedAtMsRole, "createdAtMs"}};
  }

  void setNotifications(QVariantList list) {
    beginResetModel();
    list_.clear();
    for (const auto& var : list) {
      list_.append(var.toMap());
    }
    endResetModel();
  }

 private:
  QList<QVariantMap> list_;
};

class FakeNotificationService : public QObject {
  Q_OBJECT
  Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
  Q_PROPERTY(QString unreadAppNames READ unreadAppNames NOTIFY unreadCountChanged)

 public:
  FakeNotificationService() : visible_model_(this) {
    visible_model_.setNotifications({QVariantMap{
        {QStringLiteral("notifId"), 42},
        {QStringLiteral("summary"), QStringLiteral("Test Notification")},
        {QStringLiteral("body"), QStringLiteral("This is a smoke test notification")},
        {QStringLiteral("appIcon"), QStringLiteral("dialog-information")},
        {QStringLiteral("actions"), QVariantList{}},
        {QStringLiteral("hasDefaultAction"), true},
        {QStringLiteral("accentKind"), QStringLiteral("cyan")},
        {QStringLiteral("createdAtMs"), 1718000000000LL},
    }});
  }

  [[nodiscard]] int unreadCount() const { return unread_count_; }
  [[nodiscard]] QString unreadAppNames() const { return unread_app_names_; }
  [[nodiscard]] uint32_t lastActionId() const { return last_action_id_; }
  [[nodiscard]] QString lastActionKey() const { return last_action_key_; }
  [[nodiscard]] uint32_t lastDismissId() const { return last_dismiss_id_; }

  void setUnreadState(int count, const QString& app_names) {
    unread_count_ = count;
    unread_app_names_ = app_names;
    emit unreadCountChanged();
  }

  void setHistoryGroups(QVariantList groups) { history_groups_ = std::move(groups); }

  [[nodiscard]] Q_INVOKABLE QVariantList recentHistoryGrouped(int max_groups) const {
    if (max_groups >= history_groups_.size()) {
      return history_groups_;
    }
    return history_groups_.mid(0, max_groups);
  }

  [[nodiscard]] Q_INVOKABLE QAbstractItemModel* visibleModelForMonitor(const QString& /*monitor_name*/) {
    return &visible_model_;
  }

  Q_INVOKABLE void invokeAction(uint32_t notif_id, const QString& action_key) {
    last_action_id_ = notif_id;
    last_action_key_ = action_key;
  }

  Q_INVOKABLE void dismiss(uint32_t notif_id) { last_dismiss_id_ = notif_id; }
  Q_INVOKABLE void hoverEntered(uint32_t /*notif_id*/) {}
  Q_INVOKABLE void hoverLeft(uint32_t /*notif_id*/) {}

 Q_SIGNALS:
  void unreadCountChanged();

 private:
  int unread_count_{0};
  QString unread_app_names_;
  QVariantList history_groups_;
  uint32_t last_action_id_{0};
  QString last_action_key_;
  uint32_t last_dismiss_id_{0};
  FakeNotificationModel visible_model_;
};

class FakeCalendarService : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString weekStartDay READ weekStartDay CONSTANT)

 public:
  [[nodiscard]] QString weekStartDay() const { return QStringLiteral("Mon"); }
};

class FakeNotificationRuleModel : public QAbstractListModel {
  Q_OBJECT

 public:
  enum Role {
    AppNameRole = Qt::UserRole + 1,
    DisplayNameRole,
    DisplayIconRole,
    RuleEnabledRole,
    UrgencyFilterRole,
  };
  Q_ENUM(Role)

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : 10; }

  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
      return {};
    }
    switch (role) {
      case AppNameRole:
        return QStringLiteral("Test App %1").arg(index.row() + 1);
      case DisplayNameRole:
        return QStringLiteral("Test Application %1").arg(index.row() + 1);
      case DisplayIconRole:
        return QStringLiteral("application-x-executable");
      case RuleEnabledRole:
        return true;
      case UrgencyFilterRole:
        return 0;
      default:
        return {};
    }
  }

  Q_INVOKABLE void setEnabled(int /*row*/, bool /*enabled*/) {}
  Q_INVOKABLE void setUrgencyFilter(int /*row*/, int /*filter*/) {}

 protected:
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override {
    return {{AppNameRole, "appName"},
            {DisplayNameRole, "displayName"},
            {DisplayIconRole, "displayIcon"},
            {RuleEnabledRole, "ruleEnabled"},
            {UrgencyFilterRole, "urgencyFilter"}};
  }
};

class FakeSidebarManager : public QObject {
  Q_OBJECT

 public:
  [[nodiscard]] QString lastToggledMonitor() const { return last_toggled_monitor_; }

  Q_INVOKABLE void toggle(const QString& monitor_name) { last_toggled_monitor_ = monitor_name; }
  Q_INVOKABLE void close(const QString& /*monitor_name*/ = {}) {}
  Q_INVOKABLE void onContentHeightChanged(const QString& /*monitor_name*/, int /*height*/) {}
  Q_INVOKABLE void onClosingAnimationFinished(const QString& /*monitor_name*/) {}

 private:
  QString last_toggled_monitor_;
};

class FakeLauncherService : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
  Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
  Q_PROPERTY(int resultCount READ resultCount NOTIFY resultCountChanged)
  Q_PROPERTY(QAbstractItemModel* results READ results CONSTANT)
  Q_PROPERTY(QString activeCategory READ activeCategory NOTIFY activeCategoryChanged)
  Q_PROPERTY(int appResultCount READ appResultCount CONSTANT)
  Q_PROPERTY(int actionResultCount READ actionResultCount CONSTANT)
  Q_PROPERTY(QString selectedEntryName READ selectedEntryName CONSTANT)
  Q_PROPERTY(QString selectedEntryIcon READ selectedEntryIcon CONSTANT)
  Q_PROPERTY(QString selectedEntryDesktopFile READ selectedEntryDesktopFile CONSTANT)
  Q_PROPERTY(QVariantList selectedEntryActions READ selectedEntryActions CONSTANT)

 public:
  FakeLauncherService() { results_.setStringList({QStringLiteral("App 1"), QStringLiteral("App 2")}); }

  [[nodiscard]] QString query() const { return query_; }
  [[nodiscard]] int selectedIndex() const { return selected_index_; }
  [[nodiscard]] int resultCount() const { return results_.stringList().size(); }
  [[nodiscard]] QAbstractItemModel* results() { return &results_; }
  [[nodiscard]] QString activeCategory() const { return active_category_; }
  [[nodiscard]] int appResultCount() const { return 2; }
  [[nodiscard]] int actionResultCount() const { return 1; }
  [[nodiscard]] QString selectedEntryName() const { return QStringLiteral("Firefox Web Browser"); }
  [[nodiscard]] QString selectedEntryIcon() const { return QStringLiteral("firefox"); }
  [[nodiscard]] QString selectedEntryDesktopFile() const { return QStringLiteral("firefox.desktop"); }
  [[nodiscard]] QVariantList selectedEntryActions() const {
    return QVariantList{QVariantMap{{QStringLiteral("name"), QStringLiteral("New Private Window")}}};
  }

  Q_INVOKABLE void setQuery(const QString& query) {
    if (query_ == query) {
      return;
    }
    query_ = query;
    emit queryChanged();
  }
  Q_INVOKABLE void setSelectedIndex(int index) {
    if (selected_index_ == index) {
      return;
    }
    selected_index_ = index;
    emit selectedIndexChanged();
  }
  Q_INVOKABLE void moveSelection(int /*delta*/) {}
  Q_INVOKABLE bool launchSelected() { return false; }
  Q_INVOKABLE bool launch(int /*index*/) { return false; }
  Q_INVOKABLE void reload() {}
  Q_INVOKABLE void setActiveCategory(const QString& category) {
    if (active_category_ == category) {
      return;
    }
    active_category_ = category;
    emit activeCategoryChanged();
  }
  Q_INVOKABLE QVariant entryInfoForDesktopFile(const QString& desktopFile) const {
    return QVariantMap{{QStringLiteral("name"), QStringLiteral("Firefox")},
                       {QStringLiteral("iconName"), QStringLiteral("firefox")},
                       {QStringLiteral("desktopFile"), desktopFile}};
  }
  Q_INVOKABLE bool launchDesktopFile(const QString& /*desktopFile*/) { return true; }
  Q_INVOKABLE QStringList availableCategories() const {
    return QStringList{QStringLiteral("Internet"), QStringLiteral("Development")};
  }
  Q_INVOKABLE int countForCategory(const QString& /*category*/) const { return 5; }
  Q_INVOKABLE void launchAction(int /*index*/, int /*actionIndex*/) {}
  Q_INVOKABLE QVariantList defaultAppEntriesForMimeTypes(const QStringList& /*mime_types*/) const {
    return defaultAppEntries();
  }
  Q_INVOKABLE QVariantList defaultAppEntriesForMimeTypesAndCategories(const QStringList& /*mime_types*/,
                                                                      const QStringList& /*categories*/) const {
    return defaultAppEntries();
  }
  Q_INVOKABLE QVariantList defaultAppEntriesForCategory(const QString& /*category*/) const {
    return defaultAppEntries();
  }

 Q_SIGNALS:
  void queryChanged();
  void selectedIndexChanged();
  void resultCountChanged();
  void activeCategoryChanged();
  void launched();

 private:
  [[nodiscard]] static QVariantList defaultAppEntries() {
    return {
        QVariantMap{{QStringLiteral("name"), QStringLiteral("First Candidate")},
                    {QStringLiteral("icon"), QStringLiteral("first-candidate-icon")},
                    {QStringLiteral("desktopFile"), QStringLiteral("first.desktop")},
                    {QStringLiteral("mimeTypes"), QStringList{QStringLiteral("text/plain")}}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Selected Candidate")},
                    {QStringLiteral("icon"), QStringLiteral("selected-candidate-icon")},
                    {QStringLiteral("desktopFile"), QStringLiteral("selected.desktop")},
                    {QStringLiteral("mimeTypes"), QStringList{QStringLiteral("text/plain")}}},
    };
  }

  QString query_;
  int selected_index_{-1};
  QString active_category_;
  QStringListModel results_;
};

class FakeRecentAppsTracker : public QObject {
  Q_OBJECT
 public:
  Q_INVOKABLE QVariantList recentEntries(int /*count*/) const {
    if (empty_) {
      return {};
    }
    return QVariantList{QVariantMap{{QStringLiteral("desktopFile"), QStringLiteral("firefox.desktop")}}};
  }
  Q_INVOKABLE QDateTime lastUsedFor(const QString& /*desktopFile*/) const { return QDateTime::currentDateTime(); }
  Q_INVOKABLE void setEmpty(bool empty) {
    if (empty_ == empty) {
      return;
    }
    empty_ = empty;
    emit recentChanged();
  }

 Q_SIGNALS:
  void recentChanged();

 private:
  bool empty_{false};
};

class FakeLauncherSurface : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)

 public:
  [[nodiscard]] bool visible() const { return false; }
  Q_INVOKABLE void toggle(const QString& /*monitor_name*/ = {}) {}
  Q_INVOKABLE void show(const QString& /*monitor_name*/ = {}) {}
  Q_INVOKABLE void hide() {}
  Q_INVOKABLE void notifyHideReady() {}

 Q_SIGNALS:
  void visibleChanged();
};

// Test-only seam for QML harness tests: MprisService's D-Bus-facing slots stay private in
// production. This wrapper drives the injected FakeMprisDBus directly so tst_*.qml files can put
// MprisService into arbitrary discovered/playing/paused states without loosening its API.
class MprisTestSeed : public QObject {
  Q_OBJECT
 public:
  explicit MprisTestSeed(FakeMprisDBus& dbus) : dbus_(dbus) {}

  Q_INVOKABLE void seedPlayer(const QString& service, const QVariantMap& properties) {
    dbus_.seedPlayer(service, properties);
    dbus_.emitNameOwnerChanged(service, QString(), QStringLiteral("owner"));
  }

  Q_INVOKABLE void setPlaybackStatus(const QString& service, const QString& status) {
    dbus_.emitPlayerPropertiesChanged(service, {{QStringLiteral("PlaybackStatus"), status}});
  }

  Q_INVOKABLE void setCanGoNext(const QString& service, bool can_go_next) {
    dbus_.emitPlayerPropertiesChanged(service, {{QStringLiteral("CanGoNext"), can_go_next}});
  }

  // Drives the same disappearance path a real player exiting takes, so tst_*.qml files can
  // reset MprisService to an empty registry between tests (QtQuickTest runs test functions in
  // alphabetical, not declaration, order, and MprisService is one process-wide singleton).
  Q_INVOKABLE void removePlayer(const QString& service) {
    dbus_.emitNameOwnerChanged(service, QStringLiteral("owner"), QString());
    dbus_.removePlayer(service);
  }

 private:
  FakeMprisDBus& dbus_;
};

// NOLINTEND(readability-convert-member-functions-to-static, modernize-return-braced-init-list)

inline bool writeFile(const QString& path, const QByteArray& content) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  file.write(content);
  return true;
}

inline QByteArray shellQmldir(const QString& source_root) {
  const QStringList entries = holonightQmlModuleEntries(source_root);
  return QStringLiteral("module HolonightShell\n%1\n").arg(entries.join(QLatin1Char('\n'))).toUtf8();
}

inline QStringList discoveredQmlFiles(const QString& source_root) {
  QStringList files;
  QDirIterator qml_it(source_root + QStringLiteral("/apps/shell/qml"), {QStringLiteral("*.qml")}, QDir::Files,
                      QDirIterator::Subdirectories);
  while (qml_it.hasNext()) {
    files.append(QDir(source_root).relativeFilePath(qml_it.next()));
  }
  files.sort();
  return files;
}

inline QStringList qmlFilesFromModuleEntries(const QStringList& entries) {
  QStringList files;
  for (const QString& entry : entries) {
    const qsizetype file_url_start = entry.indexOf(QStringLiteral("file://"));
    if (file_url_start < 0) {
      continue;
    }
    files.append(QDir(QStringLiteral(TEST_SOURCE_DIR)).relativeFilePath(QUrl(entry.mid(file_url_start)).toLocalFile()));
  }
  files.sort();
  return files;
}

struct XdgTempIsolation {
  QTemporaryDir dir;
  XdgTempIsolation() { qputenv("XDG_CONFIG_HOME", dir.path().toUtf8()); }
  ~XdgTempIsolation() { qunsetenv("XDG_CONFIG_HOME"); }

  XdgTempIsolation(const XdgTempIsolation&) = delete;
  XdgTempIsolation& operator=(const XdgTempIsolation&) = delete;
  XdgTempIsolation(XdgTempIsolation&&) = delete;
  XdgTempIsolation& operator=(XdgTempIsolation&&) = delete;
};

class FakeTopbarTestSeed : public QObject {
  Q_OBJECT

 public:
  FakeTopbarTestSeed(FakeWeatherService& weather, AudioService& audio, BatteryService& battery,
                     FakeKeyboardLayoutService& keyboard_layout, FakeNetworkService& network)
      : weather_(weather), audio_(audio), battery_(battery), keyboard_layout_(keyboard_layout), network_(network) {}

  Q_INVOKABLE void reset() {
    weather_.setHasData(true);
    audio_.setAvailable(true);
    setBatteryPresent(true);
    keyboard_layout_.setLayoutCode(QStringLiteral("EN"));
    network_.setAvailable(true);
  }

  Q_INVOKABLE void setWeatherHasData(bool value) { weather_.setHasData(value); }
  Q_INVOKABLE void setWeatherTemperature(double value) { weather_.setCurrentTemperature(value); }
  Q_INVOKABLE void setAudioAvailable(bool value) { audio_.setAvailable(value); }
  Q_INVOKABLE void setBatteryPresent(bool value) {
    BatteryStateUpdate update;
    update.present = value;
    battery_.applyStateUpdate(update);
  }
  Q_INVOKABLE void setKeyboardLayoutCode(const QString& value) { keyboard_layout_.setLayoutCode(value); }
  Q_INVOKABLE void setNetworkAvailable(bool value) { network_.setAvailable(value); }

 private:
  FakeWeatherService& weather_;
  AudioService& audio_;
  BatteryService& battery_;
  FakeKeyboardLayoutService& keyboard_layout_;
  FakeNetworkService& network_;
};

class FakeQmlServices {
 public:
  FakeQmlServices()
      : battery_(std::make_unique<NullDbusPropertyClient>()),
        audio_(AudioService::SkipInit),
        power_profiles_(PowerProfilesService::SkipInit),
        topbar_test_seed_(weather_, audio_, battery_, keyboard_layout_, network_) {
    compositor_.publishSnapshotForTest({.connected = true,
                                        .focused_output = QStringLiteral("DP-1"),
                                        .capabilities = {.workspace_listing = true,
                                                         .workspace_activation = true,
                                                         .numeric_workspace_creation = true,
                                                         .special_workspaces = true,
                                                         .active_window = true,
                                                         .focused_output = true,
                                                         .urgency = true,
                                                         .occupancy = true},
                                        .workspaces = {{.id = QStringLiteral("1"),
                                                        .numeric_slot = 1,
                                                        .display_name = QStringLiteral("1"),
                                                        .outputs = {QStringLiteral("DP-1")},
                                                        .active = true,
                                                        .focused = true,
                                                        .occupied = true}}});

    BatteryStateUpdate battery_update;
    battery_update.percent = 74;
    battery_update.present = true;
    battery_.applyStateUpdate(battery_update);

    audio_.applyVolume(55);
    audio_.setAvailable(true);

    power_profiles_.applyActiveProfile(QStringLiteral("balanced"));
    power_profiles_.applyProfiles(
        {QStringLiteral("power-saver"), QStringLiteral("balanced"), QStringLiteral("performance")});
  }

  [[nodiscard]] bool registerSingletons() {  // NOLINT(readability-function-cognitive-complexity)
    return qmlRegisterSingletonInstance("HolonightShell", 1, 0, "CompositorService", &compositor_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "BatteryService", &battery_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "AudioService", &audio_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "PowerProfilesService", &power_profiles_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "TrayModel", &tray_model_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "KeyboardLayoutService", &keyboard_layout_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "NetworkService", &network_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "WeatherService", &weather_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "BrightnessService", &brightness_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "TopbarTestSeed", &topbar_test_seed_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "SessionService", &session_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "SystemInfoService", &system_info_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "AppearanceService", &appearance_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "NotificationService", &notifications_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "NotificationRuleModel", &notification_rules_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "CalendarService", &calendar_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "SidebarManager", &sidebar_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "LauncherService", &launcher_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "LauncherSurface", &launcher_surface_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "PopupSurface", &popup_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "StatusPopupSurface", &status_popup_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "SettingsNavigationService", &settings_navigation_) >=
               0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "TooltipSurface", &tooltip_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "RecentAppsTracker", &recent_apps_tracker_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "SuspendInhibitorService",
                                        &suspend_inhibitor_service_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "MprisService", &mpris_) >= 0 &&
           qmlRegisterSingletonInstance("HolonightShell", 1, 0, "MprisTestSeed", &mpris_test_seed_) >= 0 &&
           qmlRegisterSingletonType<WeatherIconBridge>(
               "HolonightShell", 1, 0, "WeatherIconBridge",
               [](QQmlEngine* engine, QJSEngine*) -> QObject* { return new WeatherIconBridge(engine); }) >= 0;
  }

  [[nodiscard]] FakeNotificationService& notifications() { return notifications_; }
  [[nodiscard]] FakeSidebarManager& sidebar() { return sidebar_; }

 private:
  // MprisService owns its IMprisDBus via unique_ptr, so the FakeMprisDBus it hands over is
  // captured here (before std::move) to construct MprisTestSeed with a live reference.
  std::unique_ptr<IMprisDBus> captureMprisDbus() {
    auto owned = std::make_unique<FakeMprisDBus>();
    mpris_dbus_ = owned.get();
    return owned;
  }

  XdgTempIsolation xdg_isolation_;  // must be first — sets XDG_CONFIG_HOME before config_service
  ConfigService config_service_;
  CompositorService compositor_{CompositorKind::Hyprland};
  BatteryService battery_;
  SuspendInhibitorService suspend_inhibitor_service_{SuspendInhibitorService::SkipInit};
  AudioService audio_;
  PowerProfilesService power_profiles_;
  TrayModel tray_model_{&config_service_};
  FakeKeyboardLayoutService keyboard_layout_;
  FakeNetworkService network_;
  FakeWeatherService weather_;
  FakeBrightnessService brightness_;
  FakeTopbarTestSeed topbar_test_seed_;
  FakeSessionService session_;
  FakeSystemInfoService system_info_;
  FakeAppearanceService appearance_;
  FakeNotificationService notifications_;
  FakeNotificationRuleModel notification_rules_;
  FakeCalendarService calendar_;
  FakeSidebarManager sidebar_;
  FakeLauncherService launcher_;
  FakeLauncherSurface launcher_surface_;
  FakePopupSurface popup_;
  FakeStatusPopupSurface status_popup_;
  FakeSettingsNavigationService settings_navigation_;
  FakeTooltipSurface tooltip_;
  FakeRecentAppsTracker recent_apps_tracker_;
  FakeMprisDBus* mpris_dbus_{nullptr};
  MprisService mpris_{captureMprisDbus()};
  MprisTestSeed mpris_test_seed_{*mpris_dbus_};
};

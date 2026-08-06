#pragma once

#include <QColor>
#include <QDBusVariant>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QVariant>

#include <memory>

class IPortalDBus;
class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

// QML singleton. Probes the XDG Desktop Portal broker, exposes availability diagnostics and
// system appearance settings (color-scheme, accent-color), and provides Q_INVOKABLE plumbing
// for file-chooser and URI-opening portals. Does not implement portal backends.
class PortalService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  // Broker diagnostics
  Q_PROPERTY(bool available READ available NOTIFY availableChanged FINAL)
  Q_PROPERTY(QStringList interfaces READ interfaces NOTIFY interfacesChanged FINAL)
  Q_PROPERTY(QStringList backends READ backends NOTIFY backendsChanged FINAL)

  // Per-interface availability booleans (derived from interfaces)
  Q_PROPERTY(bool settingsAvailable READ settingsAvailable NOTIFY settingsAvailableChanged FINAL)
  Q_PROPERTY(bool fileChooserAvailable READ fileChooserAvailable NOTIFY fileChooserAvailableChanged FINAL)
  Q_PROPERTY(bool openUriAvailable READ openUriAvailable NOTIFY openUriAvailableChanged FINAL)
  Q_PROPERTY(bool inhibitAvailable READ inhibitAvailable NOTIFY inhibitAvailableChanged FINAL)
  Q_PROPERTY(bool screenCastAvailable READ screenCastAvailable NOTIFY screenCastAvailableChanged FINAL)
  Q_PROPERTY(bool globalShortcutsAvailable READ globalShortcutsAvailable NOTIFY globalShortcutsAvailableChanged FINAL)

  // Settings portal values
  Q_PROPERTY(int colorScheme READ colorScheme NOTIFY colorSchemeChanged FINAL)
  Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged FINAL)

 public:
  // Production: probes the real session bus.
  explicit PortalService(QObject* parent = nullptr);
  // Test seam: inject a fake D-Bus abstraction; no real bus contact.
  explicit PortalService(std::unique_ptr<IPortalDBus> dbus, QObject* parent = nullptr);
  ~PortalService() override;

  PortalService(const PortalService&) = delete;
  PortalService& operator=(const PortalService&) = delete;
  PortalService(PortalService&&) = delete;
  PortalService& operator=(PortalService&&) = delete;

  [[nodiscard]] bool available() const { return available_; }
  [[nodiscard]] QStringList interfaces() const { return interfaces_; }
  [[nodiscard]] QStringList backends() const { return backends_; }
  [[nodiscard]] bool settingsAvailable() const { return settings_available_; }
  [[nodiscard]] bool fileChooserAvailable() const { return file_chooser_available_; }
  [[nodiscard]] bool openUriAvailable() const { return open_uri_available_; }
  [[nodiscard]] bool inhibitAvailable() const { return inhibit_available_; }
  [[nodiscard]] bool screenCastAvailable() const { return screen_cast_available_; }
  [[nodiscard]] bool globalShortcutsAvailable() const { return global_shortcuts_available_; }
  [[nodiscard]] int colorScheme() const { return color_scheme_; }
  [[nodiscard]] QColor accentColor() const { return accent_color_; }

  Q_INVOKABLE void openFile(const QString& window_handle, const QString& title, const QStringList& filters);
  Q_INVOKABLE void openUri(const QString& uri);

 Q_SIGNALS:
  void availableChanged();
  void interfacesChanged();
  void backendsChanged();
  void settingsAvailableChanged();
  void fileChooserAvailableChanged();
  void openUriAvailableChanged();
  void inhibitAvailableChanged();
  void screenCastAvailableChanged();
  void globalShortcutsAvailableChanged();
  void colorSchemeChanged();
  void accentColorChanged();

 private Q_SLOTS:
  void onSettingChanged(const QString& portal_namespace, const QString& key, const QDBusVariant& value);
  void onBrokerAppeared();
  void onBrokerDisappeared();
  void onNameOwnerChanged(const QString& name, const QString& old_owner, const QString& new_owner);
  void refreshBackends();

 private:
  void init();
  void startProbe();
  void onNameHasOwnerReply(QDBusPendingCallWatcher* watcher);
  void onIntrospectReply(QDBusPendingCallWatcher* watcher);
  void onListNamesReply(QDBusPendingCallWatcher* watcher);
  void onSettingsReadReply(const QString& key, QDBusPendingCallWatcher* watcher);
  void readSettingsAsync();
  void applyInterfaceList(const QStringList& ifaces);
  void applyBackendList(const QStringList& names);
  [[nodiscard]] bool setAvailable(bool val);
  [[nodiscard]] bool setColorScheme(int val);
  [[nodiscard]] bool setAccentColor(const QColor& color);
  [[nodiscard]] static QVariant unwrapSettingValue(const QVariant& raw);
  [[nodiscard]] static QColor decodeAccentColor(const QVariant& inner);
  void finishProbe();

  std::unique_ptr<IPortalDBus> dbus_;
  std::unique_ptr<QDBusServiceWatcher> broker_watcher_;

  bool available_{false};
  QStringList interfaces_;
  QStringList backends_;

  bool settings_available_{false};
  bool file_chooser_available_{false};
  bool open_uri_available_{false};
  bool inhibit_available_{false};
  bool screen_cast_available_{false};
  bool global_shortcuts_available_{false};

  int color_scheme_{0};
  QColor accent_color_;

  // Re-entrancy guard: set while a broker probe is in flight.
  bool probe_in_flight_{false};
  // Counter tracking parallel Introspect + ListNames calls; cleared to 0 when both complete.
  int probe_in_flight_count_{0};
};

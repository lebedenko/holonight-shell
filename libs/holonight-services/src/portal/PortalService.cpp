#include "PortalService.h"

#include "PortalDbus.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QLoggingCategory>
#include <QTimer>
#include <QXmlStreamReader>

#include <algorithm>

Q_LOGGING_CATEGORY(lcPortalService, "holonight.portal")

namespace {
constexpr auto kPortalDesktop = "org.freedesktop.portal.Desktop";
constexpr auto kImplPortalPrefix = "org.freedesktop.impl.portal.";
constexpr auto kAppearanceNs = "org.freedesktop.appearance";
constexpr auto kKeyColorScheme = "color-scheme";
constexpr auto kKeyAccentColor = "accent-color";

// Portal interface names used to derive the six availability booleans.
constexpr auto kIfaceSettings = "org.freedesktop.portal.Settings";
constexpr auto kIfaceFileChooser = "org.freedesktop.portal.FileChooser";
constexpr auto kIfaceOpenUri = "org.freedesktop.portal.OpenURI";
constexpr auto kIfaceInhibit = "org.freedesktop.portal.Inhibit";
constexpr auto kIfaceScreenCast = "org.freedesktop.portal.ScreenCast";
constexpr auto kIfaceGlobalShortcuts = "org.freedesktop.portal.GlobalShortcuts";
}  // namespace

// ─── Construction ────────────────────────────────────────────────────────────

PortalService::PortalService(QObject* parent) : QObject(parent), dbus_(std::make_unique<SystemPortalDBus>()) { init(); }

PortalService::PortalService(std::unique_ptr<IPortalDBus> dbus, QObject* parent)
    : QObject(parent), dbus_(std::move(dbus)) {
  init();
}

PortalService::~PortalService() = default;

void PortalService::init() {
  broker_watcher_.reset(
      dbus_->createServiceWatcher(QLatin1String(kPortalDesktop), QDBusServiceWatcher::WatchForOwnerChange, this));
  connect(broker_watcher_.get(), &QDBusServiceWatcher::serviceRegistered, this, &PortalService::onBrokerAppeared);
  connect(broker_watcher_.get(), &QDBusServiceWatcher::serviceUnregistered, this, &PortalService::onBrokerDisappeared);

  dbus_->connectSettingChanged(this, SLOT(onSettingChanged(QString, QString, QDBusVariant)));
  dbus_->connectNameOwnerChanged(this, SLOT(onNameOwnerChanged(QString, QString, QString)));

  QTimer::singleShot(0, this, [this] { startProbe(); });
}

// ─── Probe sequence ──────────────────────────────────────────────────────────

void PortalService::startProbe() {
  if (probe_in_flight_) {
    return;
  }
  probe_in_flight_ = true;
  probe_in_flight_count_ = 0;

  auto* watcher = new QDBusPendingCallWatcher(dbus_->nameHasOwner(QLatin1String(kPortalDesktop)), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this, &PortalService::onNameHasOwnerReply);

  qCInfo(lcPortalService) << "Probing portal broker...";
}

void PortalService::onNameHasOwnerReply(QDBusPendingCallWatcher* watcher) {
  // Do NOT use QDBusPendingReply<bool> here — its assign() calls setMetaTypes() which
  // corrupts fromCompletedCall() replies that lack a D-Bus connection (test seam).
  // Use QDBusPendingCall::isError() + reply().arguments() instead.
  const bool is_error = watcher->isError();
  const bool has_owner = !is_error && watcher->reply().arguments().value(0).toBool();
  watcher->deleteLater();

  if (!has_owner) {
    if (setAvailable(false)) {
      emit availableChanged();
    }
    probe_in_flight_ = false;
    qCInfo(lcPortalService) << "Broker available: false";
    return;
  }

  if (setAvailable(true)) {
    emit availableChanged();
  }
  qCInfo(lcPortalService) << "Broker available: true — running Introspect + ListNames";

  // Fire Introspect and ListNames in parallel; finishProbe() fires when both complete.
  probe_in_flight_count_ = 2;

  auto* introspect_watcher = new QDBusPendingCallWatcher(dbus_->introspectPortal(), this);
  connect(introspect_watcher, &QDBusPendingCallWatcher::finished, this, &PortalService::onIntrospectReply);

  auto* list_watcher = new QDBusPendingCallWatcher(dbus_->listNames(), this);
  connect(list_watcher, &QDBusPendingCallWatcher::finished, this, &PortalService::onListNamesReply);
}

void PortalService::onIntrospectReply(QDBusPendingCallWatcher* watcher) {
  const bool is_error = watcher->isError();
  const QString xml = is_error ? QString{} : watcher->reply().arguments().value(0).toString();
  watcher->deleteLater();

  if (!available_) {
    return;  // Broker disappeared while probe was in flight.
  }

  if (is_error) {
    qCWarning(lcPortalService) << "Introspect error";
  } else {
    QStringList ifaces;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
      if (reader.readNext() == QXmlStreamReader::StartElement && reader.name() == QLatin1String("interface")) {
        const QString name = reader.attributes().value(QLatin1String("name")).toString();
        if (name.startsWith(QLatin1String("org.freedesktop.portal."))) {
          ifaces.append(name);
        }
      }
    }
    applyInterfaceList(ifaces);
  }

  if (--probe_in_flight_count_ <= 0) {
    finishProbe();
  }
}

void PortalService::onListNamesReply(QDBusPendingCallWatcher* watcher) {
  const bool is_error = watcher->isError();
  const QStringList all_names = is_error ? QStringList{} : watcher->reply().arguments().value(0).toStringList();
  watcher->deleteLater();

  if (!available_) {
    return;
  }

  if (is_error) {
    qCWarning(lcPortalService) << "ListNames error";
  } else {
    QStringList impl_names;
    for (const QString& name : all_names) {
      if (name.startsWith(QLatin1String(kImplPortalPrefix))) {
        impl_names.append(name);
      }
    }
    applyBackendList(impl_names);
  }

  if (--probe_in_flight_count_ <= 0) {
    finishProbe();
  }
}

void PortalService::finishProbe() {
  probe_in_flight_ = false;
  qCInfo(lcPortalService) << "Portal probe complete; interfaces:" << interfaces_.size()
                          << "backends:" << backends_.size();
  if (settings_available_) {
    readSettingsAsync();
  }
}

// ─── Watcher lifecycle ───────────────────────────────────────────────────────

void PortalService::onBrokerAppeared() {
  qCInfo(lcPortalService) << "Portal broker appeared; re-probing";
  startProbe();
}

void PortalService::onBrokerDisappeared() {
  // Clear in-flight state before resetting properties so a rapid restart can re-probe cleanly.
  probe_in_flight_ = false;
  probe_in_flight_count_ = 0;

  if (setAvailable(false)) {
    emit availableChanged();
  }
  applyInterfaceList({});
  applyBackendList({});
  qCInfo(lcPortalService) << "Portal broker disappeared; available=false";
}

void PortalService::onNameOwnerChanged(const QString& name, const QString& old_owner, const QString& new_owner) {
  if (!name.startsWith(QLatin1String(kImplPortalPrefix))) {
    return;
  }
  if (old_owner == new_owner) {
    return;
  }
  refreshBackends();
}

void PortalService::refreshBackends() {
  auto* watcher = new QDBusPendingCallWatcher(dbus_->listNames(), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* wch) {
    const bool is_error = wch->isError();
    const QStringList all_names = is_error ? QStringList{} : wch->reply().arguments().value(0).toStringList();
    wch->deleteLater();
    if (is_error) {
      qCWarning(lcPortalService) << "refreshBackends ListNames error";
      return;
    }
    QStringList impl_names;
    for (const QString& name : all_names) {
      if (name.startsWith(QLatin1String(kImplPortalPrefix))) {
        impl_names.append(name);
      }
    }
    applyBackendList(impl_names);
  });
}

// ─── Interface / backend list application ────────────────────────────────────

void PortalService::applyInterfaceList(const QStringList& ifaces) {
  if (ifaces == interfaces_) {
    return;
  }
  interfaces_ = ifaces;
  emit interfacesChanged();

  const bool new_settings = ifaces.contains(QLatin1String(kIfaceSettings));
  const bool new_file_chooser = ifaces.contains(QLatin1String(kIfaceFileChooser));
  const bool new_open_uri = ifaces.contains(QLatin1String(kIfaceOpenUri));
  const bool new_inhibit = ifaces.contains(QLatin1String(kIfaceInhibit));
  const bool new_screen_cast = ifaces.contains(QLatin1String(kIfaceScreenCast));
  const bool new_global_shortcuts = ifaces.contains(QLatin1String(kIfaceGlobalShortcuts));

  if (new_settings != settings_available_) {
    settings_available_ = new_settings;
    emit settingsAvailableChanged();
  }
  if (new_file_chooser != file_chooser_available_) {
    file_chooser_available_ = new_file_chooser;
    emit fileChooserAvailableChanged();
  }
  if (new_open_uri != open_uri_available_) {
    open_uri_available_ = new_open_uri;
    emit openUriAvailableChanged();
  }
  if (new_inhibit != inhibit_available_) {
    inhibit_available_ = new_inhibit;
    emit inhibitAvailableChanged();
  }
  if (new_screen_cast != screen_cast_available_) {
    screen_cast_available_ = new_screen_cast;
    emit screenCastAvailableChanged();
  }
  if (new_global_shortcuts != global_shortcuts_available_) {
    global_shortcuts_available_ = new_global_shortcuts;
    emit globalShortcutsAvailableChanged();
  }
}

void PortalService::applyBackendList(const QStringList& names) {
  if (names == backends_) {
    return;
  }
  backends_ = names;
  emit backendsChanged();
}

// ─── Settings portal ─────────────────────────────────────────────────────────

void PortalService::readSettingsAsync() {
  const QString appearance = QLatin1String(kAppearanceNs);
  const QString key_cs = QLatin1String(kKeyColorScheme);
  const QString key_ac = QLatin1String(kKeyAccentColor);

  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
  auto* watcher_cs = new QDBusPendingCallWatcher(dbus_->readSetting(appearance, key_cs), this);
  connect(watcher_cs, &QDBusPendingCallWatcher::finished, this,
          [this, key_cs](QDBusPendingCallWatcher* wch) { onSettingsReadReply(key_cs, wch); });

  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
  auto* watcher_ac = new QDBusPendingCallWatcher(dbus_->readSetting(appearance, key_ac), this);
  connect(watcher_ac, &QDBusPendingCallWatcher::finished, this,
          [this, key_ac](QDBusPendingCallWatcher* wch) { onSettingsReadReply(key_ac, wch); });
}

void PortalService::onSettingsReadReply(const QString& key, QDBusPendingCallWatcher* watcher) {
  const bool is_error = watcher->isError();
  const QDBusError error = watcher->error();
  const QVariant raw_arg = is_error ? QVariant{} : watcher->reply().arguments().value(0);
  watcher->deleteLater();

  if (is_error) {
    if (key == QLatin1String(kKeyAccentColor) &&
        error.name() == QLatin1String("org.freedesktop.portal.Error.NotFound")) {
      qCInfo(lcPortalService) << "Settings accent-color unavailable:" << error.message();
    } else {
      qCWarning(lcPortalService) << "Settings.Read(" << key << ") error:" << error.name() << error.message();
    }
    return;
  }

  const QVariant result = unwrapSettingValue(raw_arg);

  if (key == QLatin1String(kKeyColorScheme)) {
    const int scheme = static_cast<int>(result.toUInt());
    if (setColorScheme(scheme)) {
      emit colorSchemeChanged();
      qCInfo(lcPortalService) << "Settings color-scheme:" << scheme;
    }
  } else if (key == QLatin1String(kKeyAccentColor)) {
    const QColor color = decodeAccentColor(result);
    if (color.isValid() && setAccentColor(color)) {
      emit accentColorChanged();
      qCInfo(lcPortalService) << "Settings accent-color:" << color;
    }
  }
}

void PortalService::onSettingChanged(const QString& portal_namespace, const QString& key, const QDBusVariant& value) {
  if (portal_namespace != QLatin1String(kAppearanceNs)) {
    return;
  }
  if (key == QLatin1String(kKeyColorScheme)) {
    const int scheme = static_cast<int>(unwrapSettingValue(QVariant::fromValue(value)).toUInt());
    if (setColorScheme(scheme)) {
      emit colorSchemeChanged();
      qCInfo(lcPortalService) << "Settings color-scheme changed:" << scheme;
    }
  } else if (key == QLatin1String(kKeyAccentColor)) {
    const QColor color = decodeAccentColor(unwrapSettingValue(QVariant::fromValue(value)));
    if (color.isValid() && setAccentColor(color)) {
      emit accentColorChanged();
      qCInfo(lcPortalService) << "Settings accent-color changed:" << color;
    }
  }
}

QVariant PortalService::unwrapSettingValue(const QVariant& raw) {
  QVariant value = raw;
  for (int depth = 0; depth < 4 && value.userType() == qMetaTypeId<QDBusVariant>(); ++depth) {
    value = value.value<QDBusVariant>().variant();
  }
  return value;
}

QColor PortalService::decodeAccentColor(const QVariant& inner) {
  // The accent-color value is a (ddd) D-Bus struct. Qt stores it as QDBusArgument
  // inside the QVariant. Using canConvert<T>() would trigger a write-mode conversion
  // and log "QDBusArgument: write from a read-only object". Check userType() first.
  if (inner.userType() != qMetaTypeId<QDBusArgument>()) {
    qCWarning(lcPortalService) << "accent-color: unexpected inner variant type" << inner.typeName();
    return {};
  }

  const auto arg = inner.value<QDBusArgument>();
  if (arg.currentType() != QDBusArgument::StructureType) {
    qCWarning(lcPortalService) << "accent-color: QDBusArgument is not a struct";
    return {};
  }

  double red{};
  double green{};
  double blue{};
  arg.beginStructure();
  arg >> red >> green >> blue;
  arg.endStructure();

  return QColor::fromRgbF(static_cast<float>(std::clamp(red, 0.0, 1.0)),
                          static_cast<float>(std::clamp(green, 0.0, 1.0)),
                          static_cast<float>(std::clamp(blue, 0.0, 1.0)));
}

// ─── Portal invokables ───────────────────────────────────────────────────────

void PortalService::openFile(const QString& window_handle, const QString& title, const QStringList& filters) {
  if (!file_chooser_available_) {
    qCWarning(lcPortalService) << "Portal FileChooser unavailable; openFile() is a no-op";
    return;
  }
  auto* watcher = new QDBusPendingCallWatcher(dbus_->openFile(window_handle, title, filters), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, watcher, &QDBusPendingCallWatcher::deleteLater);
  qCInfo(lcPortalService) << "openFile requested:" << title;
}

void PortalService::openUri(const QString& uri) {
  if (!open_uri_available_) {
    qCWarning(lcPortalService) << "Portal OpenURI unavailable; openUri() is a no-op";
    return;
  }
  auto* watcher = new QDBusPendingCallWatcher(dbus_->openUri(uri), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, watcher, &QDBusPendingCallWatcher::deleteLater);
  qCInfo(lcPortalService) << "openUri requested:" << uri;
}

// ─── State setters (return true if value changed) ────────────────────────────

bool PortalService::setAvailable(bool val) {
  if (val == available_) {
    return false;
  }
  available_ = val;
  return true;
}

bool PortalService::setColorScheme(int val) {
  if (val == color_scheme_) {
    return false;
  }
  color_scheme_ = val;
  return true;
}

bool PortalService::setAccentColor(const QColor& color) {
  if (color == accent_color_) {
    return false;
  }
  accent_color_ = color;
  return true;
}

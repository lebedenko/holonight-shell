#include "SettingsPortalBackend.h"

#include "ThemeConfigPath.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMetaType>
#include <QLoggingCategory>
#include <QSettings>

#include <algorithm>
#include <holonight/theme_catalog.h>

Q_LOGGING_CATEGORY(lcSettingsPortalBackend, "holonight.portal.settingsbackend")

namespace {
constexpr auto kServiceName = "org.freedesktop.impl.portal.desktop.holonight";
constexpr auto kObjectPath = "/org/freedesktop/portal/desktop";
constexpr auto kAppearanceNs = "org.freedesktop.appearance";
constexpr auto kColorSchemeKey = "color-scheme";
constexpr auto kAccentColorKey = "accent-color";

QString normalizedMode(const QString& mode) {
  const QString normalized = mode.trimmed().toLower();
  if (normalized == QLatin1String("light")) {
    return QStringLiteral("light");
  }
  return QStringLiteral("dark");
}

QString schemeForThemeConfig(const QString& scheme, const QString& mode) {
  QString normalized = Holonight::normalizeSchemeId(scheme);
  if (!normalized.isEmpty()) {
    return normalized;
  }
  return normalizedMode(mode) == QLatin1String("light") ? QStringLiteral("holonight-light")
                                                        : Holonight::defaultSchemeId();
}

QColor resolvedAccentColorForThemeConfig(const QString& scheme, const QString& accent) {
  return Holonight::accentColorForScheme(scheme, accent);
}
}  // namespace

QDBusArgument& operator<<(QDBusArgument& argument, const SettingsPortalAccentColor& color) {
  argument.beginStructure();
  argument << color.red << color.green << color.blue;
  argument.endStructure();
  return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, SettingsPortalAccentColor& color) {
  argument.beginStructure();
  argument >> color.red >> color.green >> color.blue;
  argument.endStructure();
  return argument;
}

SettingsPortalBackend::SettingsPortalBackend(QObject* parent) : SettingsPortalBackend(true, parent) {}

SettingsPortalBackend::SettingsPortalBackend(bool register_on_session_bus, QObject* parent) : QObject(parent) {
  qRegisterMetaType<SettingsPortalAccentColor>("SettingsPortalAccentColor");
  qDBusRegisterMetaType<SettingsPortalAccentColor>();
  values_ = readThemeConfig();
  if (register_on_session_bus) {
    registerOnSessionBus();
  }
}

SettingsPortalBackend::~SettingsPortalBackend() {
  if (registered_) {
    QDBusConnection::sessionBus().unregisterObject(QLatin1String(kObjectPath));
    QDBusConnection::sessionBus().unregisterService(QLatin1String(kServiceName));
  }
}

QDBusVariant SettingsPortalBackend::Read(const QString& portal_namespace, const QString& key) const {
  if (portal_namespace != QLatin1String(kAppearanceNs)) {
    return {};
  }
  if (key == QLatin1String(kColorSchemeKey)) {
    return variantForColorScheme(values_.color_scheme);
  }
  if (key == QLatin1String(kAccentColorKey)) {
    return variantForAccentColor(values_.accent_color);
  }
  return {};
}

QMap<QString, QVariantMap> SettingsPortalBackend::ReadAll(const QStringList& namespaces) const {
  QMap<QString, QVariantMap> result;
  if (namespaces.isEmpty() || namespaces.contains(QLatin1String(kAppearanceNs))) {
    result.insert(QLatin1String(kAppearanceNs), appearanceMap(values_));
  }
  return result;
}

void SettingsPortalBackend::reloadFromThemeConfig() { applyValues(readThemeConfig(), true); }

void SettingsPortalBackend::registerOnSessionBus() {
  QDBusConnection bus = QDBusConnection::sessionBus();
  if (!bus.isConnected()) {
    qCWarning(lcSettingsPortalBackend) << "Session bus unavailable; Settings portal backend not registered";
    return;
  }

  registered_ = bus.registerService(QLatin1String(kServiceName)) &&
                bus.registerObject(QLatin1String(kObjectPath), this,
                                   QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals);
  if (!registered_) {
    qCWarning(lcSettingsPortalBackend) << "Failed to register Settings portal backend:" << bus.lastError().message();
    bus.unregisterObject(QLatin1String(kObjectPath));
    bus.unregisterService(QLatin1String(kServiceName));
  } else {
    qCInfo(lcSettingsPortalBackend) << "Registered Settings portal backend at" << kServiceName << kObjectPath;
  }
}

void SettingsPortalBackend::applyValues(const Values& values, bool emit_changes) {
  const Values previous = values_;
  values_ = values;
  if (!emit_changes) {
    return;
  }
  if (values_.color_scheme != previous.color_scheme) {
    emit SettingChanged(QLatin1String(kAppearanceNs), QLatin1String(kColorSchemeKey),
                        variantForColorScheme(values_.color_scheme));
  }
  if (values_.accent_color != previous.accent_color) {
    emit SettingChanged(QLatin1String(kAppearanceNs), QLatin1String(kAccentColorKey),
                        variantForAccentColor(values_.accent_color));
  }
}

int SettingsPortalBackend::colorSchemeForThemeConfig(const QString& scheme, const QString& mode) {
  const QString normalized = Holonight::normalizeSchemeId(scheme);
  if (!normalized.isEmpty()) {
    return Holonight::modeNameForScheme(normalized) == QLatin1String("light") ? kColorSchemeLight : kColorSchemeDark;
  }
  return normalizedMode(mode) == QLatin1String("light") ? kColorSchemeLight : kColorSchemeDark;
}

SettingsPortalBackend::Values SettingsPortalBackend::readThemeConfig() {
  QSettings settings{resolveHolonightThemeConfigPaths().file_path, QSettings::IniFormat};
  const QString scheme = settings.value(QStringLiteral("appearance/scheme")).toString();
  const QString mode = settings.value(QStringLiteral("appearance/mode"), QStringLiteral("dark")).toString();
  const QString resolved_scheme = schemeForThemeConfig(scheme, mode);

  Values values;
  values.color_scheme = colorSchemeForThemeConfig(scheme, mode);
  values.accent_color = resolvedAccentColorForThemeConfig(
      resolved_scheme, settings.value(QStringLiteral("appearance/accent")).toString());
  return values;
}

QDBusVariant SettingsPortalBackend::variantForColorScheme(int color_scheme) {
  return QDBusVariant{QVariant::fromValue(static_cast<uint>(color_scheme))};
}

QDBusVariant SettingsPortalBackend::variantForAccentColor(const QColor& color) {
  const SettingsPortalAccentColor accent{.red = std::clamp(static_cast<double>(color.redF()), 0.0, 1.0),
                                         .green = std::clamp(static_cast<double>(color.greenF()), 0.0, 1.0),
                                         .blue = std::clamp(static_cast<double>(color.blueF()), 0.0, 1.0)};
  return QDBusVariant{QVariant::fromValue(accent)};
}

QVariantMap SettingsPortalBackend::appearanceMap(const Values& values) {
  QVariantMap map;
  map.insert(QLatin1String(kColorSchemeKey), QVariant::fromValue(variantForColorScheme(values.color_scheme)));
  map.insert(QLatin1String(kAccentColorKey), QVariant::fromValue(variantForAccentColor(values.accent_color)));
  return map;
}

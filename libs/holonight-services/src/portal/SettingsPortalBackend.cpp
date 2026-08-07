#include "SettingsPortalBackend.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMetaType>
#include <QLoggingCategory>

#include <algorithm>
#include <holonight/theme_catalog.h>

Q_LOGGING_CATEGORY(lcSettingsPortalBackend, "holonight.portal.settingsbackend")

namespace {
constexpr auto kServiceName = "org.freedesktop.impl.portal.desktop.holonight";
constexpr auto kObjectPath = "/org/freedesktop/portal/desktop";
constexpr auto kAppearanceNs = "org.freedesktop.appearance";
constexpr auto kColorSchemeKey = "color-scheme";
constexpr auto kAccentColorKey = "accent-color";

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

SettingsPortalBackend::SettingsPortalBackend(const Holonight::ResolvedAppearance& appearance, QObject* parent)
    : SettingsPortalBackend(appearance, true, parent) {}

SettingsPortalBackend::SettingsPortalBackend(const Holonight::ResolvedAppearance& appearance,
                                             bool register_on_session_bus, QObject* parent)
    : QObject(parent) {
  qRegisterMetaType<SettingsPortalAccentColor>("SettingsPortalAccentColor");
  qDBusRegisterMetaType<SettingsPortalAccentColor>();
  values_ = valuesForAppearance(appearance);
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

void SettingsPortalBackend::applyAppearance(const Holonight::ResolvedAppearance& appearance) {
  applyValues(valuesForAppearance(appearance), true);
}

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

SettingsPortalBackend::Values SettingsPortalBackend::valuesForAppearance(
    const Holonight::ResolvedAppearance& appearance) {
  Values values;
  values.color_scheme = appearance.color_mode == Holonight::ColorMode::Light ? kColorSchemeLight : kColorSchemeDark;
  values.accent_color = Holonight::accentColorForScheme(appearance.scheme, appearance.accent);
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

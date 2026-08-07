#pragma once

#include <QColor>
#include <QDBusArgument>
#include <QDBusVariant>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <holonight/appearance.h>

struct SettingsPortalAccentColor {
  double red{};
  double green{};
  double blue{};
};

Q_DECLARE_METATYPE(SettingsPortalAccentColor)

QDBusArgument& operator<<(QDBusArgument& argument, const SettingsPortalAccentColor& color);
const QDBusArgument& operator>>(const QDBusArgument& argument, SettingsPortalAccentColor& color);

class SettingsPortalBackend : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Settings")

 public:
  struct Values {
    int color_scheme{kColorSchemeDark};
    QColor accent_color{QColor::fromRgbF(0.490196, 0.811765, 1.0)};

    friend bool operator==(const Values& lhs, const Values& rhs) {
      return lhs.color_scheme == rhs.color_scheme && lhs.accent_color == rhs.accent_color;
    }
    friend bool operator!=(const Values& lhs, const Values& rhs) { return !(lhs == rhs); }
  };

  explicit SettingsPortalBackend(const Holonight::ResolvedAppearance& appearance, QObject* parent = nullptr);
  SettingsPortalBackend(const Holonight::ResolvedAppearance& appearance, bool register_on_session_bus,
                        QObject* parent = nullptr);
  ~SettingsPortalBackend() override;

  SettingsPortalBackend(const SettingsPortalBackend&) = delete;
  SettingsPortalBackend& operator=(const SettingsPortalBackend&) = delete;
  SettingsPortalBackend(SettingsPortalBackend&&) = delete;
  SettingsPortalBackend& operator=(SettingsPortalBackend&&) = delete;

  [[nodiscard]] Values values() const { return values_; }
  [[nodiscard]] bool registered() const { return registered_; }

  // NOLINTNEXTLINE(readability-identifier-naming)
  [[nodiscard]] Q_SCRIPTABLE QDBusVariant Read(const QString& portal_namespace, const QString& key) const;
  // NOLINTNEXTLINE(readability-identifier-naming)
  [[nodiscard]] Q_SCRIPTABLE QMap<QString, QVariantMap> ReadAll(const QStringList& namespaces) const;

  void applyAppearance(const Holonight::ResolvedAppearance& appearance);

 Q_SIGNALS:
  // NOLINTNEXTLINE(readability-identifier-naming)
  Q_SCRIPTABLE void SettingChanged(const QString& portal_namespace, const QString& key, const QDBusVariant& value);

 private:
  static constexpr int kColorSchemeDark = 1;
  static constexpr int kColorSchemeLight = 2;

  void registerOnSessionBus();
  void applyValues(const Values& values, bool emit_changes);

  [[nodiscard]] static Values valuesForAppearance(const Holonight::ResolvedAppearance& appearance);
  [[nodiscard]] static QDBusVariant variantForColorScheme(int color_scheme);
  [[nodiscard]] static QDBusVariant variantForAccentColor(const QColor& color);
  [[nodiscard]] static QVariantMap appearanceMap(const Values& values);

  Values values_;
  bool registered_{false};
};

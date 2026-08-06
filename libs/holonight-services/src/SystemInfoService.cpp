#include "SystemInfoService.h"

#include "ConfigService.h"
#include "SystemInfo.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QLoggingCategory>
#include <QStringConverter>
#include <QTextStream>
#include <QUrl>

#include <array>
#include <holonight_config/config_parsers.h>
#include <sys/types.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcSystemInfo, "holonight.systeminfo")

namespace {
// Bounds each blocking Accounts D-Bus round trip so an unresponsive/absent service cannot stall
// shell startup for the OS-default (~25s) timeout. See docs/sdd/poc-remediation-phase3/DESIGN.md
// Item 4 for why 1000ms and why setTimeout() (not QDBusPendingCallWatcher) is the right knob.
constexpr int kAccountsDbusTimeoutMs = 1000;

struct DbusConnectionOverride {
  bool has_custom{false};
  QDBusConnection custom_connection{QString()};
};

DbusConnectionOverride& dbusConnectionOverride() {
  static DbusConnectionOverride state;
  return state;
}

QDBusConnection accountsDbusConnection() {
  const DbusConnectionOverride& state = dbusConnectionOverride();
  return state.has_custom ? state.custom_connection : QDBusConnection::systemBus();
}

constexpr auto kAccountsService = "org.freedesktop.Accounts";
constexpr auto kAccountsManagerPath = "/org/freedesktop/Accounts";
constexpr auto kAccountsManagerInterface = "org.freedesktop.Accounts";
constexpr auto kAccountsUserInterface = "org.freedesktop.Accounts.User";

// Reads one property via an explicit, timeout-bounded org.freedesktop.DBus.Properties.Get call.
// Bypasses QDBusInterface::property() deliberately: QDBusInterface performs its own dynamic
// meta-object introspection, which is a separate, unbounded blocking round trip not governed by
// QDBusAbstractInterface::setTimeout() (confirmed empirically — see
// docs/sdd/poc-remediation-phase3/DESIGN.md Item 4 risk log). Building the message directly and
// calling QDBusConnection::call() with an explicit timeout has no such hidden step.
QString readUserProperty(const QDBusConnection& bus, const QString& user_path, const QString& property_name) {
  QDBusMessage get_msg =
      QDBusMessage::createMethodCall(QString::fromLatin1(kAccountsService), user_path,
                                     QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
  get_msg << QString::fromLatin1(kAccountsUserInterface) << property_name;
  const QDBusMessage reply = bus.call(get_msg, QDBus::Block, kAccountsDbusTimeoutMs);
  if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
    return {};
  }
  return reply.arguments().constFirst().value<QDBusVariant>().variant().toString();
}
}  // namespace

void SystemInfoService::setDbusConnection(const QDBusConnection& connection) {
  DbusConnectionOverride& state = dbusConnectionOverride();
  state.has_custom = true;
  state.custom_connection = connection;
}

void SystemInfoService::resetDbusConnection() {
  DbusConnectionOverride& state = dbusConnectionOverride();
  state.has_custom = false;
  state.custom_connection = QDBusConnection(QString());
}

SystemInfoService::SystemInfoService(ConfigService* config, QObject* parent) : QObject(parent), config_(config) {
  readOsRelease();
  readAccountsService();
}

void SystemInfoService::readOsRelease() {
  QFile file(QStringLiteral("/etc/os-release"));
  QHash<QString, QString> os_release;
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    os_release = parseOsRelease(stream.readAll());
  } else {
    qCWarning(lcSystemInfo) << "SystemInfoService: failed to read /etc/os-release:" << file.errorString();
  }

  const SystemInfoSnapshot snapshot = systemInfoFromOsRelease(os_release);
  name_ = snapshot.name;
  display_name_ = snapshot.display_name;
  logo_icon_name_ = resolveThemeLogoIconName(snapshot.logo_icon_name);

  // Steps 1-3 of the resolution precedence (SPEC.md Appendix A). Only reachable when a valid
  // file override, generic flag, or distro-table hit exists; falls through otherwise.
  if (applyLogoConfigOverride(os_release)) {
    qCInfo(lcSystemInfo) << "SystemInfoService: detected system" << name_ << "display" << display_name_ << "logo"
                         << logo_source_ << "(config/alias override)";
    return;
  }

  // Steps 4-5, unchanged (REQ-C-001 / REQ-C-002).
  const QString logo_path = findSystemLogoPath(os_release, {QStringLiteral("/usr/share/pixmaps")});
  logo_source_ = logo_path.isEmpty() ? QStringLiteral("image://icon/%1").arg(logo_icon_name_)
                                     : QUrl::fromLocalFile(logo_path).toString();
  logo_tinted_ = false;

  qCInfo(lcSystemInfo) << "SystemInfoService: detected system" << name_ << "display" << display_name_ << "logo"
                       << logo_source_;
}

bool SystemInfoService::applyLogoConfigOverride(const QHash<QString, QString>& os_release) {
  if (config_ != nullptr) {
    const LogoConfig& logo_cfg = config_->logo();

    // Step 1: file override.
    if (!logo_cfg.file.isEmpty()) {
      const QFileInfo file_info(logo_cfg.file);
      if (file_info.exists() && file_info.isReadable()) {
        logo_source_ = QUrl::fromLocalFile(logo_cfg.file).toString();
        logo_tinted_ = false;
        return true;
      }
      qCWarning(lcConfigParsers) << "Logo file override not readable:" << logo_cfg.file
                                 << "— falling back to next resolution step";
      // fall through to step 2/3, NOT the invalid path (REQ-NF-001)
    }

    // Step 2: generic flag.
    if (logo_cfg.generic) {
      logo_source_ = QStringLiteral("qrc:/HolonightShell/linux-logo/linux.svg");
      logo_tinted_ = true;
      return true;
    }
  }

  // Step 3: distro alias table.
  const QString dist_id = os_release.value(QStringLiteral("ID")).trimmed();
  QString mapped = mapDistroIdToLogoName(dist_id);
  if (mapped.isEmpty()) {
    const QStringList id_like = os_release.value(QStringLiteral("ID_LIKE")).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& token : id_like) {
      mapped = mapDistroIdToLogoName(token.trimmed());
      if (!mapped.isEmpty()) {
        break;
      }
    }
  }
  if (!mapped.isEmpty()) {
    logo_source_ = QStringLiteral("qrc:/HolonightShell/linux-logo/%1.svg").arg(mapped);
    logo_tinted_ = true;
    return true;
  }

  return false;
}

QString SystemInfoService::resolveThemeLogoIconName(const QString& icon_name) {
  if (!icon_name.isEmpty() && QIcon::hasThemeIcon(icon_name)) {
    return icon_name;
  }

  static constexpr std::array fallback_icons = {
      "distributor-logo",
      "computer-symbolic",
  };

  for (const char* fallback_icon : fallback_icons) {
    const QString candidate = QString::fromLatin1(fallback_icon);
    if (QIcon::hasThemeIcon(candidate)) {
      return candidate;
    }
  }

  return QStringLiteral("computer-symbolic");
}

void SystemInfoService::readAccountsService() {
  QDBusConnection bus = accountsDbusConnection();
  if (!bus.isConnected()) {
    return;
  }

  // Built and dispatched as a raw QDBusMessage (not QDBusInterface) so the timeout applies to the
  // one and only round trip this call makes — no separate, unbounded introspection step.
  QDBusMessage find_user_msg =
      QDBusMessage::createMethodCall(QString::fromLatin1(kAccountsService), QString::fromLatin1(kAccountsManagerPath),
                                     QString::fromLatin1(kAccountsManagerInterface), QStringLiteral("FindUserById"));
  const uid_t uid = getuid();
  find_user_msg << static_cast<qlonglong>(uid);

  const QDBusMessage find_user_reply = bus.call(find_user_msg, QDBus::Block, kAccountsDbusTimeoutMs);
  if (find_user_reply.type() != QDBusMessage::ReplyMessage || find_user_reply.arguments().isEmpty()) {
    return;
  }

  const QVariant& user_path_argument = find_user_reply.arguments().constFirst();
  if (!user_path_argument.canConvert<QDBusObjectPath>()) {
    return;
  }
  const QString user_path = user_path_argument.value<QDBusObjectPath>().path();
  if (user_path.isEmpty()) {
    return;
  }
  avatar_path_ = readUserProperty(bus, user_path, QStringLiteral("IconFile"));
  user_name_ = readUserProperty(bus, user_path, QStringLiteral("UserName"));
  real_name_ = readUserProperty(bus, user_path, QStringLiteral("RealName"));
}

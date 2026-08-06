// REQ-F-009/REQ-F-010: SystemInfoService::readAccountsService() must bound each blocking
// org.freedesktop.Accounts D-Bus round trip via setTimeout(), rather than blocking for the
// OS-default (~25s) timeout when the Accounts service is unresponsive or slow.
//
// The fake Accounts service below runs on a dedicated QThread with its own private D-Bus
// connection to the session bus, so the client-side call (made from the main thread, using the
// default sessionBus() connection SystemInfoService is redirected to via setDbusConnection) races
// against a genuinely independent event loop — not the same thread the fake server blocks on.
// Running client and "server" on the same thread would let a synchronous server-side sleep starve
// the very event loop needed to detect the client's own timeout, defeating the test.

#include "ConfigService.h"
#include "SystemInfo.h"
#include "SystemInfoService.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVirtualObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QUrl>

#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>

namespace {

constexpr auto kAccountsService = "org.freedesktop.Accounts";
constexpr auto kAccountsPath = "/org/freedesktop/Accounts";
constexpr auto kUserPath = "/org/freedesktop/Accounts/User1";

// Swallows every incoming message without ever sending a reply — simulates a service that is
// registered (name owned) but never responds, exercising the client-side setTimeout() bound.
class SilentVirtualObject : public QDBusVirtualObject {
 public:
  [[nodiscard]] QString introspect(const QString& /*path*/) const override { return {}; }
  bool handleMessage(const QDBusMessage& /*message*/, const QDBusConnection& /*connection*/) override { return true; }
};

class FakeAccountsUser : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Accounts.User")
  Q_PROPERTY(QString IconFile READ iconFile CONSTANT)
  Q_PROPERTY(QString UserName READ userName CONSTANT)
  Q_PROPERTY(QString RealName READ realName CONSTANT)

 public:
  [[nodiscard]] static QString iconFile() { return QStringLiteral("/fake/icon.png"); }
  [[nodiscard]] static QString userName() { return QStringLiteral("fakeuser"); }
  [[nodiscard]] static QString realName() { return QStringLiteral("Fake User"); }
};

class FakeAccountsManager : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Accounts")

 public:
  int delay_ms{0};

 public Q_SLOTS:
  // NOLINTNEXTLINE(readability-identifier-naming) — must match the real Accounts D-Bus method name.
  [[nodiscard]] QDBusObjectPath FindUserById(qlonglong /*uid*/) const {
    if (delay_ms > 0) {
      QThread::msleep(static_cast<unsigned long>(delay_ms));
    }
    return QDBusObjectPath(QString::fromLatin1(kUserPath));
  }
};

class FakeAccountsServerThread : public QThread {
 public:
  enum class Mode : std::uint8_t { Silent, RespondAfterDelay };

  explicit FakeAccountsServerThread(Mode mode, int delay_ms = 0) : mode_(mode), delay_ms_(delay_ms) {}

  void run() override {
    // Pointer identity used only to make each test's private bus connection name unique.
    const auto unique_suffix = reinterpret_cast<quintptr>(this);  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    QDBusConnection conn = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                         QStringLiteral("holonightFakeAccounts%1").arg(unique_suffix));
    if (!conn.registerService(QString::fromLatin1(kAccountsService))) {
      return;
    }

    SilentVirtualObject silent_manager;
    FakeAccountsManager responsive_manager;
    responsive_manager.delay_ms = delay_ms_;
    FakeAccountsUser user;

    if (mode_ == Mode::Silent) {
      conn.registerVirtualObject(QString::fromLatin1(kAccountsPath), &silent_manager, QDBusConnection::SubPath);
    } else {
      conn.registerObject(QString::fromLatin1(kAccountsPath), &responsive_manager, QDBusConnection::ExportAllSlots);
      conn.registerObject(QString::fromLatin1(kUserPath), &user, QDBusConnection::ExportAllProperties);
    }

    exec();

    conn.unregisterService(QString::fromLatin1(kAccountsService));
    const QString conn_name = conn.name();
    conn = QDBusConnection(QString());
    QDBusConnection::disconnectFromBus(conn_name);
  }

 private:
  Mode mode_;
  int delay_ms_;
};

// Points XDG_CONFIG_HOME at a temp dir and returns the resulting config.toml path — mirrors the
// helper in test_config_service.cpp. Caller must qunsetenv("XDG_CONFIG_HOME") once ConfigService
// construction (and any dependent SystemInfoService construction) is done.
QString setTempXdgConfig(const QTemporaryDir& tmp) {
  qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());
  return tmp.path() + QStringLiteral("/holonight/config.toml");
}

void writeTempLogoConfig(const QString& path, const QByteArray& content) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write(content);
}

}  // namespace

TEST(SystemInfoService, NeverRespondingAccountsReturnsPromptlyWithFallbackValues) {
  FakeAccountsServerThread server(FakeAccountsServerThread::Mode::Silent);
  server.start();
  QThread::msleep(100);  // let the service claim its bus name before the client queries it

  SystemInfoService::setDbusConnection(QDBusConnection::sessionBus());
  const auto start = std::chrono::steady_clock::now();
  SystemInfoService service;
  const auto elapsed = std::chrono::steady_clock::now() - start;
  SystemInfoService::resetDbusConnection();

  server.quit();
  server.wait(2000);

  EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 1500);
  EXPECT_TRUE(service.avatarPath().isEmpty());
  EXPECT_TRUE(service.userName().isEmpty());
  EXPECT_TRUE(service.realName().isEmpty());
}

TEST(SystemInfoService, SlowRespondingAccountsBoundedByTimeoutFallsBackWithinThreeSeconds) {
  FakeAccountsServerThread server(FakeAccountsServerThread::Mode::RespondAfterDelay, 2000);
  server.start();
  QThread::msleep(100);

  SystemInfoService::setDbusConnection(QDBusConnection::sessionBus());
  const auto start = std::chrono::steady_clock::now();
  SystemInfoService service;
  const auto elapsed = std::chrono::steady_clock::now() - start;
  SystemInfoService::resetDbusConnection();

  server.quit();
  server.wait(3000);

  // Bounded by the client-side setTimeout() (~1s), not the server's 2s reply delay — even though
  // the server would eventually answer, the client must not wait for it.
  EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 1500);
  EXPECT_TRUE(service.avatarPath().isEmpty());
  EXPECT_TRUE(service.userName().isEmpty());
  EXPECT_TRUE(service.realName().isEmpty());
}

// ---------------------------------------------------------------------------
// [logo] config precedence chain (REQ-F-001/002/003/006/007/010/011, REQ-NF-001/002)
// ---------------------------------------------------------------------------

TEST(SystemInfoService, ValidFileOverrideWinsOverGenericAndDistroTable) {
  QTemporaryDir cfg_dir;
  ASSERT_TRUE(cfg_dir.isValid());
  QTemporaryDir logo_dir;
  ASSERT_TRUE(logo_dir.isValid());

  QFile logo_file(logo_dir.filePath(QStringLiteral("custom-logo.svg")));
  ASSERT_TRUE(logo_file.open(QIODevice::WriteOnly));
  logo_file.write("<svg></svg>");
  logo_file.close();

  const QString config_path = setTempXdgConfig(cfg_dir);
  writeTempLogoConfig(config_path,
                      QStringLiteral("[logo]\nfile = \"%1\"\ngeneric = true\n").arg(logo_file.fileName()).toUtf8());

  ConfigService config;
  SystemInfoService service(&config);
  qunsetenv("XDG_CONFIG_HOME");

  EXPECT_EQ(service.logoSource(), QUrl::fromLocalFile(logo_file.fileName()).toString());
  EXPECT_FALSE(service.logoTinted());
}

TEST(SystemInfoService, GenericFlagWinsWhenNoValidFileOverride) {
  QTemporaryDir cfg_dir;
  ASSERT_TRUE(cfg_dir.isValid());
  const QString config_path = setTempXdgConfig(cfg_dir);
  writeTempLogoConfig(config_path, "[logo]\ngeneric = true\n");

  ConfigService config;
  SystemInfoService service(&config);
  qunsetenv("XDG_CONFIG_HOME");

  EXPECT_EQ(service.logoSource(), QStringLiteral("qrc:/HolonightShell/linux-logo/linux.svg"));
  EXPECT_TRUE(service.logoTinted());
}

TEST(SystemInfoService, InvalidFileOverrideFallsThroughToGenericWithoutCrash) {
  QTemporaryDir cfg_dir;
  ASSERT_TRUE(cfg_dir.isValid());
  const QString config_path = setTempXdgConfig(cfg_dir);
  writeTempLogoConfig(config_path, "[logo]\nfile = \"/nonexistent/path/does-not-exist.svg\"\ngeneric = true\n");

  ConfigService config;
  QTest::ignoreMessage(QtWarningMsg,
                       QRegularExpression(QStringLiteral("Logo file override not readable:.*does-not-exist\\.svg")));
  SystemInfoService service(&config);
  qunsetenv("XDG_CONFIG_HOME");

  EXPECT_EQ(service.logoSource(), QStringLiteral("qrc:/HolonightShell/linux-logo/linux.svg"));
  EXPECT_TRUE(service.logoTinted());
}

TEST(SystemInfoService, DistroTableUsedWhenNeitherFileNorGenericConfigured) {
  QTemporaryDir cfg_dir;
  ASSERT_TRUE(cfg_dir.isValid());
  const QString config_path = setTempXdgConfig(cfg_dir);
  writeTempLogoConfig(config_path, "[appearance]\nui_font = \"Inter\"\n");

  ConfigService config;
  SystemInfoService service(&config);
  qunsetenv("XDG_CONFIG_HOME");

  // Derived from the same /etc/os-release SystemInfoService reads internally, so this assertion
  // stays portable across distros/CI images instead of hardcoding one distro's ID.
  QFile os_release_file(QStringLiteral("/etc/os-release"));
  QString expected_dist_id;
  if (os_release_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    const QHash<QString, QString> os_release = parseOsRelease(QString::fromUtf8(os_release_file.readAll()));
    expected_dist_id = os_release.value(QStringLiteral("ID")).trimmed();
  }
  const QString expected_mapped = mapDistroIdToLogoName(expected_dist_id);

  if (expected_mapped.isEmpty()) {
    // Unmapped distro on this machine: falls through to steps 4-5 (untinted pixmaps/icon-theme).
    EXPECT_FALSE(service.logoTinted());
  } else {
    EXPECT_EQ(service.logoSource(), QStringLiteral("qrc:/HolonightShell/linux-logo/%1.svg").arg(expected_mapped));
    EXPECT_TRUE(service.logoTinted());
  }
}

TEST(SystemInfoService, NullConfigServiceFallsThroughToDistroTableOrPixmapsWithoutCrash) {
  SystemInfoService service;  // default nullptr config_, matches pre-existing call sites
  EXPECT_FALSE(service.logoSource().isEmpty());
}

#include "test_system_info_service.moc"

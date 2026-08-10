#include "PortalDbus.h"
#include "PortalService.h"
#include "SettingsPortalBackend.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusVariant>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

namespace {

constexpr auto kSilentPortalService = "org.holonight.TestSilentPortal";
constexpr auto kPortalPath = "/org/freedesktop/portal/desktop";

class DelayedSettingsPortal : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Settings")

 public:
  int delay_ms{0};

 public Q_SLOTS:
  // NOLINTNEXTLINE(readability-identifier-naming) — must match the portal Settings D-Bus method name.
  [[nodiscard]] QDBusVariant Read(const QString& /*portal_namespace*/, const QString& /*key*/) const {
    QThread::msleep(static_cast<unsigned long>(delay_ms));
    return {};
  }
};

class DelayedPortalServerThread : public QThread {
 public:
  void run() override {
    const auto unique_suffix = reinterpret_cast<quintptr>(this);  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    QDBusConnection conn = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                         QStringLiteral("holonightSilentPortal%1").arg(unique_suffix));
    if (!conn.registerService(QString::fromLatin1(kSilentPortalService))) {
      return;
    }

    DelayedSettingsPortal portal;
    portal.delay_ms = 2000;
    conn.registerObject(QString::fromLatin1(kPortalPath), &portal, QDBusConnection::ExportAllSlots);
    exec();

    conn.unregisterService(QString::fromLatin1(kSilentPortalService));
    const QString conn_name = conn.name();
    conn = QDBusConnection(QString());
    QDBusConnection::disconnectFromBus(conn_name);
  }
};

// Wait for the async probe chain to settle.
// The chain has several queued-connection hops:
//   QTimer::singleShot(0) → startProbe → NameHasOwner watcher → Introspect+ListNames watchers
// QTest::qWait runs a real event loop that reliably drains all pending queued events + timers.
void drainEvents(int msec = 100) { QTest::qWait(msec); }

std::unique_ptr<NullPortalDBus> makeDbus(bool broker_present = true, const QStringList& ifaces = {},
                                         const QStringList& backends = {}) {
  auto dbus = std::make_unique<NullPortalDBus>();
  dbus->setNameHasOwner(broker_present);
  dbus->setInterfaces(ifaces);
  dbus->setNameList(backends);
  return dbus;
}

Holonight::ResolvedAppearance makeAppearance(QString scheme, QString accent, Holonight::ColorMode color_mode) {
  Holonight::ResolvedAppearance appearance;
  appearance.scheme = std::move(scheme);
  appearance.accent = std::move(accent);
  appearance.color_mode = color_mode;
  return appearance;
}

}  // namespace

TEST(SystemPortalDBusTest, SettingsReadTimesOutWhenPortalDoesNotReply) {
  DelayedPortalServerThread server;
  server.start();
  QThread::msleep(100);

  SystemPortalDBus dbus(500, QString::fromLatin1(kSilentPortalService));
  const auto start = std::chrono::steady_clock::now();
  QDBusPendingCallWatcher watcher(
      dbus.readSetting(QStringLiteral("org.freedesktop.appearance"), QStringLiteral("color-scheme")));
  QSignalSpy finished_spy(&watcher, &QDBusPendingCallWatcher::finished);

  EXPECT_TRUE(finished_spy.wait(1500));
  const auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_TRUE(watcher.isError());
  EXPECT_EQ(watcher.error().type(), QDBusError::NoReply);
  EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 1500);

  server.quit();
  EXPECT_TRUE(server.wait(3000));
}

// ─── Broker absent ───────────────────────────────────────────────────────────

TEST(PortalServiceTest, BrokerAbsent_AvailableFalse) {
  PortalService service(makeDbus(false));
  drainEvents();
  EXPECT_FALSE(service.available());
}

TEST(PortalServiceTest, BrokerAbsent_InterfacesEmpty) {
  PortalService service(makeDbus(false));
  drainEvents();
  EXPECT_TRUE(service.interfaces().isEmpty());
}

TEST(PortalServiceTest, BrokerAbsent_AllBooleansFalse) {
  PortalService service(makeDbus(false));
  drainEvents();
  EXPECT_FALSE(service.settingsAvailable());
  EXPECT_FALSE(service.fileChooserAvailable());
  EXPECT_FALSE(service.openUriAvailable());
  EXPECT_FALSE(service.inhibitAvailable());
  EXPECT_FALSE(service.screenCastAvailable());
  EXPECT_FALSE(service.globalShortcutsAvailable());
}

// ─── Broker present, interface list ──────────────────────────────────────────

TEST(PortalServiceTest, BrokerPresent_AvailableTrue) {
  PortalService service(makeDbus(true));
  drainEvents();
  EXPECT_TRUE(service.available());
}

TEST(PortalServiceTest, BrokerPresent_InterfacesParsed) {
  const QStringList ifaces = {QStringLiteral("org.freedesktop.portal.Settings"),
                              QStringLiteral("org.freedesktop.portal.FileChooser")};
  PortalService service(makeDbus(true, ifaces));
  drainEvents();
  EXPECT_EQ(service.interfaces().size(), 2);
  EXPECT_TRUE(service.interfaces().contains(QStringLiteral("org.freedesktop.portal.Settings")));
}

TEST(PortalServiceTest, BrokerPresent_CorrectBooleans) {
  const QStringList ifaces = {
      QStringLiteral("org.freedesktop.portal.Settings"),   QStringLiteral("org.freedesktop.portal.FileChooser"),
      QStringLiteral("org.freedesktop.portal.OpenURI"),    QStringLiteral("org.freedesktop.portal.Inhibit"),
      QStringLiteral("org.freedesktop.portal.ScreenCast"), QStringLiteral("org.freedesktop.portal.GlobalShortcuts")};
  PortalService service(makeDbus(true, ifaces));
  drainEvents();
  EXPECT_TRUE(service.settingsAvailable());
  EXPECT_TRUE(service.fileChooserAvailable());
  EXPECT_TRUE(service.openUriAvailable());
  EXPECT_TRUE(service.inhibitAvailable());
  EXPECT_TRUE(service.screenCastAvailable());
  EXPECT_TRUE(service.globalShortcutsAvailable());
}

TEST(PortalServiceTest, BrokerPresent_PartialInterfaces_CorrectBooleans) {
  const QStringList ifaces = {QStringLiteral("org.freedesktop.portal.Settings")};
  PortalService service(makeDbus(true, ifaces));
  drainEvents();
  EXPECT_TRUE(service.settingsAvailable());
  EXPECT_FALSE(service.fileChooserAvailable());
  EXPECT_FALSE(service.inhibitAvailable());
}

// ─── Backend detection ───────────────────────────────────────────────────────

TEST(PortalServiceTest, BackendsListedFromListNames) {
  const QStringList all_names = {QStringLiteral("org.freedesktop.impl.portal.Hyprland"),
                                 QStringLiteral("org.freedesktop.impl.portal.gtk"),
                                 QStringLiteral("org.freedesktop.DBus")};  // should be filtered out
  PortalService service(makeDbus(true, {}, all_names));
  drainEvents();
  EXPECT_EQ(service.backends().size(), 2);
  EXPECT_TRUE(service.backends().contains(QStringLiteral("org.freedesktop.impl.portal.Hyprland")));
  EXPECT_FALSE(service.backends().contains(QStringLiteral("org.freedesktop.DBus")));
}

TEST(PortalServiceTest, NameOwnerChangedForPortalBackendRefreshesBackends) {
  auto dbus = makeDbus(true, {}, {});
  auto* raw = dbus.get();
  PortalService service(std::move(dbus));
  drainEvents();
  ASSERT_TRUE(service.backends().isEmpty());

  raw->setNameList({QStringLiteral("org.freedesktop.impl.portal.gtk")});
  QMetaObject::invokeMethod(&service, "onNameOwnerChanged",
                            Q_ARG(QString, QStringLiteral("org.freedesktop.impl.portal.gtk")),
                            Q_ARG(QString, QString{}), Q_ARG(QString, QStringLiteral(":1.42")));
  drainEvents();

  EXPECT_EQ(service.backends(), QStringList{QStringLiteral("org.freedesktop.impl.portal.gtk")});
}

TEST(PortalServiceTest, NameOwnerChangedIgnoresNonPortalNames) {
  auto dbus = makeDbus(true, {}, {});
  auto* raw = dbus.get();
  PortalService service(std::move(dbus));
  drainEvents();

  raw->setNameList({QStringLiteral("org.freedesktop.impl.portal.gtk")});
  QMetaObject::invokeMethod(&service, "onNameOwnerChanged", Q_ARG(QString, QStringLiteral("org.example.Service")),
                            Q_ARG(QString, QString{}), Q_ARG(QString, QStringLiteral(":1.42")));
  drainEvents();

  EXPECT_TRUE(service.backends().isEmpty());
}

// ─── probe_in_flight_ guard ───────────────────────────────────────────────────

TEST(PortalServiceTest, AvailableChangedSignalEmittedOnce) {
  PortalService service(makeDbus(true));
  QSignalSpy spy(&service, &PortalService::availableChanged);
  drainEvents();
  // availableChanged fires exactly once (false→true at startup)
  EXPECT_EQ(spy.size(), 1);
}

// ─── Broker disappear ────────────────────────────────────────────────────────

TEST(PortalServiceTest, BrokerDisappear_ClearsAvailableAndInterfaces) {
  const QStringList ifaces = {QStringLiteral("org.freedesktop.portal.Settings")};
  PortalService service(makeDbus(true, ifaces));
  drainEvents();
  ASSERT_TRUE(service.available());
  ASSERT_TRUE(service.settingsAvailable());

  // Simulate broker disappearing by calling the handler directly (private Q_SLOT accessible
  // via QMetaObject::invokeMethod since it's a registered slot).
  QMetaObject::invokeMethod(&service, "onBrokerDisappeared");

  EXPECT_FALSE(service.available());
  EXPECT_FALSE(service.settingsAvailable());
  EXPECT_TRUE(service.interfaces().isEmpty());
}

TEST(PortalServiceTest, BrokerDisappear_ClearsBackends) {
  const QStringList backends = {QStringLiteral("org.freedesktop.impl.portal.Hyprland")};
  PortalService service(makeDbus(true, {}, backends));
  drainEvents();
  ASSERT_EQ(service.backends(), backends);

  QMetaObject::invokeMethod(&service, "onBrokerDisappeared");

  EXPECT_TRUE(service.backends().isEmpty());
}

// ─── Settings: color-scheme ───────────────────────────────────────────────────

TEST(PortalServiceTest, ColorScheme_ReadAtStartup) {
  const QStringList ifaces = {QStringLiteral("org.freedesktop.portal.Settings")};
  auto dbus = makeDbus(true, ifaces);
  // color-scheme 1 = dark
  dbus->setSettingResult(QStringLiteral("color-scheme"), QVariant(static_cast<uint>(1)));
  PortalService service(std::move(dbus));
  drainEvents();
  EXPECT_EQ(service.colorScheme(), 1);
}

TEST(PortalServiceTest, ColorScheme_ReadsNestedPortalVariant) {
  const QStringList ifaces = {QStringLiteral("org.freedesktop.portal.Settings")};
  auto dbus = makeDbus(true, ifaces);
  // Some portal backends return Settings.Read as a nested variant (busctl: "v v u 1").
  QDBusVariant inner;
  inner.setVariant(QVariant(static_cast<uint>(1)));
  dbus->setSettingResult(QStringLiteral("color-scheme"), QVariant::fromValue(inner));
  PortalService service(std::move(dbus));
  drainEvents();
  EXPECT_EQ(service.colorScheme(), 1);
}

TEST(PortalServiceTest, ColorScheme_DefaultZeroWhenSettingsAbsent) {
  PortalService service(makeDbus(true));  // no Settings iface
  drainEvents();
  EXPECT_EQ(service.colorScheme(), 0);
}

TEST(PortalServiceTest, ColorScheme_ErrorReply_RemainsDefault) {
  const QStringList ifaces = {QStringLiteral("org.freedesktop.portal.Settings")};
  auto dbus = makeDbus(true, ifaces);
  dbus->setSettingError(QStringLiteral("color-scheme"));
  PortalService service(std::move(dbus));
  drainEvents();
  EXPECT_EQ(service.colorScheme(), 0);
}

TEST(PortalServiceTest, AccentColor_NotFound_RemainsInvalid) {
  const QStringList ifaces = {QStringLiteral("org.freedesktop.portal.Settings")};
  auto dbus = makeDbus(true, ifaces);
  dbus->setSettingError(QStringLiteral("accent-color"));
  PortalService service(std::move(dbus));
  drainEvents();
  EXPECT_FALSE(service.accentColor().isValid());
}

// ─── Settings portal backend ─────────────────────────────────────────────────

TEST(SettingsPortalBackendTest, ReadPublishesResolvedColorScheme) {
  SettingsPortalBackend backend(
      makeAppearance(QStringLiteral("holonight-day"), QStringLiteral("blue"), Holonight::ColorMode::Light), false);

  EXPECT_EQ(
      backend.Read(QStringLiteral("org.freedesktop.appearance"), QStringLiteral("color-scheme")).variant().toUInt(),
      2U);
}

TEST(SettingsPortalBackendTest, ExportsReadMethodsOnSessionBus) {
  SettingsPortalBackend backend(
      makeAppearance(QStringLiteral("holonight-dark"), QStringLiteral("blue"), Holonight::ColorMode::Dark), true);
  if (!backend.registered()) {
    GTEST_SKIP() << "HoloNight portal service is already owned on the test session bus";
  }

  QDBusInterface introspection(QStringLiteral("org.freedesktop.impl.portal.desktop.holonight"),
                               QString::fromLatin1(kPortalPath), QStringLiteral("org.freedesktop.DBus.Introspectable"),
                               QDBusConnection::sessionBus());
  const QDBusReply<QString> reply = introspection.call(QStringLiteral("Introspect"));

  ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();
  EXPECT_THAT(reply.value().toStdString(), testing::HasSubstr("<method name=\"Read\">"));
  EXPECT_THAT(reply.value().toStdString(), testing::HasSubstr("<method name=\"ReadAll\">"));
}

TEST(SettingsPortalBackendTest, UnknownSettingReturnsNotFoundOnSessionBus) {
  SettingsPortalBackend backend(
      makeAppearance(QStringLiteral("holonight-dark"), QStringLiteral("blue"), Holonight::ColorMode::Dark), true);
  if (!backend.registered()) {
    GTEST_SKIP() << "HoloNight portal service is already owned on the test session bus";
  }

  QDBusInterface settings(QStringLiteral("org.freedesktop.impl.portal.desktop.holonight"),
                          QString::fromLatin1(kPortalPath), QStringLiteral("org.freedesktop.impl.portal.Settings"),
                          QDBusConnection::sessionBus());
  const QDBusReply<QDBusVariant> reply =
      settings.call(QStringLiteral("Read"), QStringLiteral("org.freedesktop.appearance"), QStringLiteral("contrast"));

  ASSERT_FALSE(reply.isValid());
  EXPECT_EQ(reply.error().name(), QStringLiteral("org.freedesktop.portal.Error.NotFound"));
}

TEST(SettingsPortalBackendTest, ReadUsesInjectedResolvedColorMode) {
  SettingsPortalBackend backend(
      makeAppearance(QStringLiteral("holonight-light"), QStringLiteral("blue"), Holonight::ColorMode::Light), false);

  EXPECT_EQ(
      backend.Read(QStringLiteral("org.freedesktop.appearance"), QStringLiteral("color-scheme")).variant().toUInt(),
      2U);
}

TEST(SettingsPortalBackendTest, ReadAllPublishesAppearanceValues) {
  SettingsPortalBackend backend(
      makeAppearance(QStringLiteral("holonight-dark"), QStringLiteral("violet"), Holonight::ColorMode::Dark), false);
  const QMap<QString, QVariantMap> all = backend.ReadAll({QStringLiteral("org.freedesktop.appearance")});
  const QVariantMap appearance = all.value(QStringLiteral("org.freedesktop.appearance"));

  EXPECT_EQ(appearance.value(QStringLiteral("color-scheme")).value<QDBusVariant>().variant().toUInt(), 1U);
  const auto accent = appearance.value(QStringLiteral("accent-color"))
                          .value<QDBusVariant>()
                          .variant()
                          .value<SettingsPortalAccentColor>();
  EXPECT_NEAR(accent.red, 0x9a / 255.0, 0.001);
  EXPECT_NEAR(accent.green, 0x8c / 255.0, 0.001);
  EXPECT_NEAR(accent.blue, 0xff / 255.0, 0.001);
}

TEST(SettingsPortalBackendTest, DefaultAccentUsesSchemeNativeColor) {
  SettingsPortalBackend backend(
      makeAppearance(QStringLiteral("holonight-latte"), QStringLiteral("default"), Holonight::ColorMode::Light), false);
  const QMap<QString, QVariantMap> all = backend.ReadAll({QStringLiteral("org.freedesktop.appearance")});
  const QVariantMap appearance = all.value(QStringLiteral("org.freedesktop.appearance"));
  const auto accent = appearance.value(QStringLiteral("accent-color"))
                          .value<QDBusVariant>()
                          .variant()
                          .value<SettingsPortalAccentColor>();

  EXPECT_NEAR(accent.red, 0x1e / 255.0, 0.001);
  EXPECT_NEAR(accent.green, 0x66 / 255.0, 0.001);
  EXPECT_NEAR(accent.blue, 0xf5 / 255.0, 0.001);
}

TEST(SettingsPortalBackendTest, ApplyAppearanceEmitsOnlyChangedSystemFacingValues) {
  SettingsPortalBackend backend(
      makeAppearance(QStringLiteral("holonight-dark"), QStringLiteral("cyan"), Holonight::ColorMode::Dark), false);
  QSignalSpy changed_spy(&backend, &SettingsPortalBackend::SettingChanged);

  backend.applyAppearance(
      makeAppearance(QStringLiteral("holonight-light"), QStringLiteral("cyan"), Holonight::ColorMode::Light));

  ASSERT_EQ(changed_spy.count(), 2);
  EXPECT_EQ(changed_spy.takeFirst().at(1).toString(), QStringLiteral("color-scheme"));
  EXPECT_EQ(changed_spy.takeFirst().at(1).toString(), QStringLiteral("accent-color"));

  backend.applyAppearance(
      makeAppearance(QStringLiteral("holonight-light"), QStringLiteral("yellow"), Holonight::ColorMode::Light));

  ASSERT_EQ(changed_spy.count(), 1);
  EXPECT_EQ(changed_spy.takeFirst().at(1).toString(), QStringLiteral("accent-color"));
}

// ─── Portal invokables ────────────────────────────────────────────────────────

TEST(PortalServiceTest, OpenFile_NopWhenUnavailable) {
  auto dbus = makeDbus(true);  // no FileChooser iface
  auto* raw = dbus.get();
  PortalService service(std::move(dbus));
  drainEvents();
  service.openFile(QString{}, QStringLiteral("Pick"), {});
  drainEvents();
  EXPECT_EQ(raw->openFileCalls(), 0);
}

TEST(PortalServiceTest, OpenFile_CallsDbusWhenAvailable) {
  const QStringList ifaces = {QStringLiteral("org.freedesktop.portal.FileChooser")};
  auto dbus = makeDbus(true, ifaces);
  auto* raw = dbus.get();
  PortalService service(std::move(dbus));
  drainEvents();
  service.openFile(QString{}, QStringLiteral("Pick"), {});
  drainEvents();
  EXPECT_EQ(raw->openFileCalls(), 1);
}

TEST(PortalServiceTest, OpenFile_ForwardsFiltersWhenAvailable) {
  const QStringList ifaces = {QStringLiteral("org.freedesktop.portal.FileChooser")};
  auto dbus = makeDbus(true, ifaces);
  auto* raw = dbus.get();
  PortalService service(std::move(dbus));
  drainEvents();

  const QStringList filters = {QStringLiteral("*.png"), QStringLiteral("*.jpg")};
  service.openFile(QString{}, QStringLiteral("Pick"), filters);
  drainEvents();

  EXPECT_EQ(raw->lastOpenFileFilters(), filters);
}

TEST(PortalServiceTest, OpenUri_NopWhenUnavailable) {
  auto dbus = makeDbus(true);  // no OpenURI iface
  auto* raw = dbus.get();
  PortalService service(std::move(dbus));
  drainEvents();
  service.openUri(QStringLiteral("https://example.com"));
  drainEvents();
  EXPECT_EQ(raw->openUriCalls(), 0);
}

TEST(PortalServiceTest, OpenUri_CallsDbusWhenAvailable) {
  const QStringList ifaces = {QStringLiteral("org.freedesktop.portal.OpenURI")};
  auto dbus = makeDbus(true, ifaces);
  auto* raw = dbus.get();
  PortalService service(std::move(dbus));
  drainEvents();
  service.openUri(QStringLiteral("https://example.com"));
  drainEvents();
  EXPECT_EQ(raw->openUriCalls(), 1);
}

#include "test_portal_service.moc"

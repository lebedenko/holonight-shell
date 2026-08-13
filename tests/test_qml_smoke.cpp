#include "AudioService.h"
#include "BatteryService.h"
#include "BatteryState.h"
#include "ConfigService.h"
#include "FakeQmlServices.h"
#include "GeneratedQmlFiles.h"
#include "PowerProfilesService.h"
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
#include <QRegularExpression>
#include <QSignalSpy>
#include <QStringListModel>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QtQml/qqml.h>

#include <gtest/gtest.h>
#include <memory>

namespace {

void expectLoads(QQmlEngine* engine, QByteArrayView qml) {
  QQmlComponent component(engine);
  component.setData(qml.toByteArray(), QUrl(QStringLiteral("memory:test.qml")));

  if (component.isLoading()) {
    QSignalSpy spy(&component, &QQmlComponent::statusChanged);
    if (!spy.wait(2000)) {
      ADD_FAILURE() << "QML component timed out during import resolution";
      return;
    }
  }

  if (component.isError()) {
    ADD_FAILURE() << qPrintable(component.errorString());
    return;
  }

  std::unique_ptr<QObject> object(component.create());
  if (!object) {
    ADD_FAILURE() << qml.data() << "\nstatus=" << component.status()
                  << "\nerrors=" << qPrintable(component.errorString());
  }
}

void expectFileLoads(QQmlEngine* engine, const QUrl& url, const QVariantMap& initial_properties = {}) {
  QQmlComponent component(engine, url, QQmlComponent::PreferSynchronous);
  if (component.isError()) {
    ADD_FAILURE() << qPrintable(component.errorString());
    return;
  }

  std::unique_ptr<QObject> object(component.createWithInitialProperties(initial_properties));
  if (!object) {
    ADD_FAILURE() << qPrintable(url.toString()) << "\n" << qPrintable(component.errorString());
  }
}

QByteArray componentsQmldir(const QString& source_root) {
  const QString prefix = QStringLiteral("file://") + source_root + QStringLiteral("/qml/HoloNight/Components/");
  return QStringLiteral(
             "module Holonight.Components\n"
             "ExternalIcon 1.0 %1ExternalIcon.qml\n")
      .arg(prefix)
      .toUtf8();
}

std::unique_ptr<QObject> createQmlObject(QQmlEngine* engine, const QUrl& url, const QVariantMap& initial_properties) {
  QQmlComponent component(engine, url, QQmlComponent::PreferSynchronous);
  if (component.isError()) {
    ADD_FAILURE() << qPrintable(component.errorString());
    return nullptr;
  }

  std::unique_ptr<QObject> object(component.createWithInitialProperties(initial_properties));
  if (!object) {
    ADD_FAILURE() << qPrintable(url.toString()) << "\n" << qPrintable(component.errorString());
  }
  return object;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void expectBatteryIndicatorState(QQmlEngine* engine, const QString& source_root, int percent, bool charging,
                                 bool discharging, bool fully_charged, bool high_discharging, bool low, bool critical,
                                 bool pulse_glow, const QColor& glow_color) {
  const auto object = createQmlObject(
      engine, QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Topbar/BatteryIndicator.qml")),
      {{QStringLiteral("percent"), percent},
       {QStringLiteral("charging"), charging},
       {QStringLiteral("discharging"), discharging},
       {QStringLiteral("fullyCharged"), fully_charged}});
  ASSERT_NE(object, nullptr);

  EXPECT_EQ(object->property("highDischarging").toBool(), high_discharging);
  EXPECT_EQ(object->property("low").toBool(), low);
  EXPECT_EQ(object->property("critical").toBool(), critical);
  EXPECT_EQ(object->property("pulseGlow").toBool(), pulse_glow);
  EXPECT_EQ(object->property("statusGlowColor").value<QColor>(), glow_color);

  auto* icon = object->findChild<QObject*>(QStringLiteral("batteryIcon"));
  ASSERT_NE(icon, nullptr);
  EXPECT_EQ(icon->property("charging").toBool(), charging);
  EXPECT_EQ(icon->property("glowEnabled").toBool(), pulse_glow);
  EXPECT_EQ(icon->property("glowColor").value<QColor>(), glow_color);
}

}  // namespace

TEST(QmlSmoke, CanonicalModulesResolveOwnedTypes) {
  QQmlEngine engine;
  engine.addImportPath(QStringLiteral("/tmp/holonight-qt-prefix/lib/qt6/qml"));

  expectLoads(&engine, R"(
      import QtQuick
      import Holonight.Core
      import Holonight.Controls

      Item {
          readonly property color paletteColor: HoloniightPalette.background
          readonly property var themeFamilies: HolonightTheme.themeFamilies
          readonly property int surfaceRole: HnSurfaceRole.Card
          readonly property int shapeKind: HnShapeKind.Rounded
          readonly property bool semanticIconSupported: HnIconProvider.supportsSemanticColors("audio-volume-high")

          HnIcon { source: "audio-volume-high" }
          HnSurfaceFrame {
              surfaceRole: HnSurfaceRole.Card
          }
      }
  )");
}

TEST(QmlSmoke, CanonicalModuleArtifactsExistInDependencyPrefix) {
  const QString module_root = QStringLiteral("/tmp/holonight-qt-prefix/lib/qt6/qml/Holonight");
  const QStringList artifacts = {
      QStringLiteral("Core/qmldir"),
      QStringLiteral("Core/holonight_core_qml.qmltypes"),
      QStringLiteral("Core/libholonight_core_qml.so"),
      QStringLiteral("Core/HnIcon.qml"),
      QStringLiteral("Controls/qmldir"),
      QStringLiteral("Controls/holonight_controls_qml.qmltypes"),
      QStringLiteral("Controls/libholonight_controls_qml.so"),
      QStringLiteral("Controls/HnSurfaceFrame.qml"),
  };

  for (const QString& artifact : artifacts) {
    EXPECT_TRUE(QFile::exists(module_root + QLatin1Char('/') + artifact))
        << qPrintable(QStringLiteral("Missing canonical QML artifact: %1").arg(artifact));
  }
}

TEST(QmlSmoke, ApplicationQmlImportsTypesFromCanonicalOwners) {
  const QString source_root = QStringLiteral(TEST_SOURCE_DIR);
  const QStringList roots = {
      source_root + QStringLiteral("/apps/shell/qml"),
      source_root + QStringLiteral("/apps/settings/qml"),
  };
  const QStringList core_types = {
      QStringLiteral("HoloniightPalette"), QStringLiteral("HolonightTheme"),
      QStringLiteral("HnAppearance"),      QStringLiteral("HnShapeProfile"),
      QStringLiteral("HnSurfaceRole"),     QStringLiteral("HnCornerStyle"),
      QStringLiteral("HnShapeKind"),       QStringLiteral("HnCornerMask"),
      QStringLiteral("HnIconProvider"),    QStringLiteral("HnIcon"),
      QStringLiteral("HnControlSize"),     QStringLiteral("HnControlMetrics"),
  };
  const QStringList controls_types = {
      QStringLiteral("HnSurfaceFrame"),
      QStringLiteral("HnApplicationWindow"),
  };
  const QStringList style_types = {
      QStringLiteral("Button"),       QStringLiteral("CheckBox"),    QStringLiteral("ComboBox"),
      QStringLiteral("ItemDelegate"), QStringLiteral("Menu"),        QStringLiteral("MenuItem"),
      QStringLiteral("ProgressBar"),  QStringLiteral("RadioButton"), QStringLiteral("ScrollBar"),
      QStringLiteral("ScrollView"),   QStringLiteral("Slider"),      QStringLiteral("SpinBox"),
      QStringLiteral("Switch"),       QStringLiteral("TabBar"),      QStringLiteral("TabButton"),
      QStringLiteral("TextArea"),     QStringLiteral("TextField"),   QStringLiteral("ToolTip"),
  };

  for (const QString& root : roots) {
    QDirIterator qml_it(root, {QStringLiteral("*.qml")}, QDir::Files, QDirIterator::Subdirectories);
    while (qml_it.hasNext()) {
      const QString path = qml_it.next();
      QFile file(path);
      ASSERT_TRUE(file.open(QIODevice::ReadOnly)) << qPrintable(path);
      const QString source = QString::fromUtf8(file.readAll());
      const QString relative_path = QDir(source_root).relativeFilePath(path);
      const bool imports_core =
          source.contains(QRegularExpression(QStringLiteral(R"((^|\n)import Holonight\.Core\s*($|\n))")));
      const bool imports_controls =
          source.contains(QRegularExpression(QStringLiteral(R"((^|\n)import Holonight\.Controls\s*($|\n))")));
      const bool imports_style =
          source.contains(QRegularExpression(QStringLiteral(R"((^|\n)import Holonight\s*($|\n))")));

      EXPECT_FALSE(source.contains(QRegularExpression(QStringLiteral(R"((^|\n)import holonight\.(core|controls))"))))
          << qPrintable(relative_path);

      for (const QString& type : core_types) {
        if (source.contains(QRegularExpression(QStringLiteral(R"(\b%1\b)").arg(type)))) {
          EXPECT_TRUE(imports_core) << qPrintable(relative_path + QStringLiteral(" uses ") + type);
        }
      }
      for (const QString& type : controls_types) {
        if (source.contains(QRegularExpression(QStringLiteral(R"((^|\n)\s*%1\s*\{)").arg(type)))) {
          EXPECT_TRUE(imports_controls) << qPrintable(relative_path + QStringLiteral(" uses ") + type);
        }
      }

      bool uses_style_type = false;
      for (const QString& type : style_types) {
        uses_style_type =
            uses_style_type || source.contains(QRegularExpression(QStringLiteral(R"((^|\n)\s*%1\s*\{)").arg(type)));
      }
      EXPECT_EQ(imports_style, uses_style_type)
          << qPrintable(relative_path + QStringLiteral(" has an unjustified Holonight style import"));
    }
  }
}

TEST(QmlSmoke, LoadsTopbarTrayAndStatusComponentsWithFakeServices) {
  QTemporaryDir modules;
  ASSERT_TRUE(modules.isValid());
  ASSERT_TRUE(QDir(modules.path()).mkpath(QStringLiteral("HolonightShell")));
  ASSERT_TRUE(QDir(modules.path()).mkpath(QStringLiteral("Holonight/Components")));
  const QString source_root = QStringLiteral(TEST_SOURCE_DIR);

  ASSERT_TRUE(writeFile(modules.filePath(QStringLiteral("HolonightShell/qmldir")), shellQmldir(source_root)));
  ASSERT_TRUE(
      writeFile(modules.filePath(QStringLiteral("Holonight/Components/qmldir")), componentsQmldir(source_root)));
  EXPECT_EQ(qmlFilesFromModuleEntries(holonightQmlModuleEntries(source_root)), discoveredQmlFiles(source_root));

  FakeQmlServices services;
  ASSERT_TRUE(services.registerSingletons());

  QQmlEngine engine;
  engine.addImportPath(QStringLiteral("/tmp/holonight-qt-prefix/lib/qt6/qml"));
  engine.addImportPath(modules.path());

  expectFileLoads(&engine, QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Topbar/TopBar.qml")),
                  {{QStringLiteral("barMonitorName"), QStringLiteral("DP-1")},
                   {QStringLiteral("width"), 1200},
                   {QStringLiteral("height"), 40}});

  const QVariantMap bar_properties = {{QStringLiteral("barMonitorName"), QStringLiteral("DP-1")}};
  expectFileLoads(&engine,
                  QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Topbar/BatteryWidget.qml")),
                  bar_properties);
  expectFileLoads(&engine, QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Topbar/AudioWidget.qml")),
                  bar_properties);
  expectFileLoads(&engine,
                  QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Topbar/NetworkWidget.qml")),
                  bar_properties);
  expectFileLoads(&engine,
                  QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Topbar/WorkspaceSection.qml")),
                  bar_properties);
  expectFileLoads(&engine,
                  QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Topbar/ActiveWindowSection.qml")),
                  {{QStringLiteral("barMonitorName"), QStringLiteral("DP-1")}, {QStringLiteral("width"), 300}});
  expectFileLoads(&engine, QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Tray/TraySection.qml")),
                  bar_properties);
  expectFileLoads(
      &engine,
      QUrl::fromLocalFile(source_root +
                          QStringLiteral("/apps/shell/qml/RightSidebar/Tabs/QuickSettings/SidebarQuickSettings.qml")),
      {{QStringLiteral("width"), 320}, {QStringLiteral("height"), 400}});
  expectFileLoads(
      &engine,
      QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Popups/Network/NetworkPopupContent.qml")),
      {{QStringLiteral("width"), 600}, {QStringLiteral("height"), 640}});
  expectFileLoads(&engine,
                  QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Popups/Status/StatusPopup.qml")),
                  {{QStringLiteral("popupId"), QStringLiteral("network")},
                   {QStringLiteral("width"), 688},
                   {QStringLiteral("height"), 730}});
  expectFileLoads(&engine, QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Launcher/Launcher.qml")),
                  {{QStringLiteral("width"), 1200}, {QStringLiteral("height"), 800}});
  expectFileLoads(&engine,
                  QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/RightSidebar/RightSidebar.qml")),
                  {{QStringLiteral("barMonitorName"), QStringLiteral("DP-1")},
                   {QStringLiteral("active"), true},
                   {QStringLiteral("width"), 380},
                   {QStringLiteral("height"), 760}});
  expectFileLoads(
      &engine, QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Popups/Audio/AudioPopupContent.qml")),
      {{QStringLiteral("width"), 600}, {QStringLiteral("height"), 640}});
  expectFileLoads(
      &engine,
      QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Popups/Battery/BatteryPopupContent.qml")),
      {{QStringLiteral("width"), 300}, {QStringLiteral("height"), 360}});
  expectFileLoads(&engine,
                  QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Notifications/ToastStack.qml")),
                  {{QStringLiteral("monitorName"), QStringLiteral("DP-1")}});
  expectFileLoads(&engine,
                  QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Widgets/WidgetSurface.qml")),
                  {{QStringLiteral("widgetType"), QStringLiteral("time-to-event")},
                   {QStringLiteral("barMonitorName"), QStringLiteral("DP-1")},
                   {QStringLiteral("titleText"), QStringLiteral("Launch Event")},
                   {QStringLiteral("remainingText"), QStringLiteral("02:14:05")},
                   {QStringLiteral("deadlineLabelText"), QStringLiteral("June 20, 2026")}});
  expectFileLoads(&engine,
                  QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Widgets/WidgetSurface.qml")),
                  {{QStringLiteral("widgetType"), QStringLiteral("clock")},
                   {QStringLiteral("barMonitorName"), QStringLiteral("DP-1")},
                   {QStringLiteral("timeText"), QStringLiteral("12:34")},
                   {QStringLiteral("secondsText"), QStringLiteral("56")},
                   {QStringLiteral("dateText"), QStringLiteral("Friday, June 19")}});

  expectLoads(&engine, R"(
      import QtQuick
      import HolonightShell

      Item {
          BarIcon { name: "audio-volume-muted"; bodyOpacity: 0.52 }
          BarIcon { name: "battery_low"; batteryPercent: 12 }
          BarIcon { name: "wifi_offline"; signalStrength: 0 }
          BarIcon { name: "network-wired-symbolic" }
          BarIcon { name: "system-lock-screen" }
          BarIcon { name: "system-log-out" }
          BarIcon { name: "system-reboot" }
          BarIcon { name: "system-shutdown" }
      }
  )");

  expectBatteryIndicatorState(&engine, source_root, 100, false, false, true, false, false, false, false,
                              QColor(QStringLiteral("#56d7ff")));
  expectBatteryIndicatorState(&engine, source_root, 74, false, true, false, true, false, false, true,
                              QColor(QStringLiteral("#56d7ff")));
  expectBatteryIndicatorState(&engine, source_root, 19, false, true, false, false, true, false, true,
                              QColor(QStringLiteral("#ff718c")));
  expectBatteryIndicatorState(&engine, source_root, 9, false, true, false, false, true, true, true,
                              QColor(QStringLiteral("#ff718c")));
  expectBatteryIndicatorState(&engine, source_root, 8, true, false, false, false, false, false, true,
                              QColor(QStringLiteral("#9a8cff")));

  services.notifications().setHistoryGroups({
      QVariantMap{{QStringLiteral("appName"), QStringLiteral("Mail")},
                  {QStringLiteral("latestSummary"), QStringLiteral("Inbox")},
                  {QStringLiteral("totalCount"), 5},
                  {QStringLiteral("latestTimestampMs"), 1718000000000LL}},
      QVariantMap{{QStringLiteral("appName"), QStringLiteral("Chat")},
                  {QStringLiteral("latestSummary"), QStringLiteral("Ping")},
                  {QStringLiteral("totalCount"), 1},
                  {QStringLiteral("latestTimestampMs"), 1718000005000LL}},
      QVariantMap{{QStringLiteral("appName"), QStringLiteral("System")},
                  {QStringLiteral("latestSummary"), QStringLiteral("Update")},
                  {QStringLiteral("totalCount"), 2},
                  {QStringLiteral("latestTimestampMs"), 1718000010000LL}},
      QVariantMap{{QStringLiteral("appName"), QStringLiteral("Calendar")},
                  {QStringLiteral("latestSummary"), QStringLiteral("Event")},
                  {QStringLiteral("totalCount"), 3},
                  {QStringLiteral("latestTimestampMs"), 1718000015000LL}},
      QVariantMap{{QStringLiteral("appName"), QStringLiteral("Weather")},
                  {QStringLiteral("latestSummary"), QStringLiteral("Rain")},
                  {QStringLiteral("totalCount"), 4},
                  {QStringLiteral("latestTimestampMs"), 1718000020000LL}},
  });

  const auto sidebar_overview = createQmlObject(
      &engine,
      QUrl::fromLocalFile(source_root +
                          QStringLiteral("/apps/shell/qml/RightSidebar/Tabs/Overview/SidebarOverview.qml")),
      {{QStringLiteral("width"), 380}, {QStringLiteral("height"), 760}});
  ASSERT_NE(sidebar_overview, nullptr);
  EXPECT_EQ(sidebar_overview->property("totalNotificationCount").toInt(), 15);
  EXPECT_EQ(sidebar_overview->property("notificationOverflowCount").toInt(), 7);

  QVariant day_model;
  ASSERT_TRUE(QMetaObject::invokeMethod(sidebar_overview.get(), "buildDayModel", Q_RETURN_ARG(QVariant, day_model),
                                        Q_ARG(QVariant, 2026), Q_ARG(QVariant, 5),
                                        Q_ARG(QVariant, QStringLiteral("Mon"))));
  const QVariantList days = day_model.toList();
  ASSERT_EQ(days.size(), 35);
  EXPECT_EQ(days.front().toMap().value(QStringLiteral("day")).toInt(), 1);
  EXPECT_TRUE(days.front().toMap().value(QStringLiteral("isCurrentMonth")).toBool());
  EXPECT_TRUE(days.at(5).toMap().value(QStringLiteral("isWeekend")).toBool());
  EXPECT_TRUE(days.at(6).toMap().value(QStringLiteral("isWeekend")).toBool());

  const auto notifications_widget = createQmlObject(
      &engine, QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Topbar/NotificationsWidget.qml")),
      {{QStringLiteral("barMonitorName"), QStringLiteral("DP-1")},
       {QStringLiteral("width"), 40},
       {QStringLiteral("height"), 30}});
  ASSERT_NE(notifications_widget, nullptr);
  EXPECT_DOUBLE_EQ(notifications_widget->property("opacity").toDouble(), 0.55);
  services.notifications().setUnreadState(3, QStringLiteral("Mail, Chat"));
  QTest::qWait(250);
  QCoreApplication::processEvents();
  EXPECT_NEAR(notifications_widget->property("opacity").toDouble(), 1.0, 0.001);

  const QVariantMap action_toast_model = {
      {QStringLiteral("notifId"), 42},
      {QStringLiteral("summary"), QStringLiteral("Update available")},
      {QStringLiteral("body"), QStringLiteral("Read <a href=\"https://example.test\">details</a><img src=\"x\">")},
      {QStringLiteral("appIcon"), QStringLiteral("dialog-information")},
      {QStringLiteral("actions"), QVariantList{}},
      {QStringLiteral("hasDefaultAction"), true},
      {QStringLiteral("accentKind"), QStringLiteral("cyan")},
      {QStringLiteral("createdAtMs"), 1718000000000LL},
  };
  const auto action_toast = createQmlObject(
      &engine, QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Notifications/ToastItem.qml")),
      {{QStringLiteral("model"), action_toast_model}});
  ASSERT_NE(action_toast, nullptr);

  QVariant sanitized_body;
  ASSERT_TRUE(QMetaObject::invokeMethod(action_toast.get(), "sanitizeBody", Q_RETURN_ARG(QVariant, sanitized_body),
                                        Q_ARG(QVariant, action_toast_model.value(QStringLiteral("body")))));
  EXPECT_FALSE(sanitized_body.toString().contains(QStringLiteral("<img"), Qt::CaseInsensitive));
  EXPECT_FALSE(sanitized_body.toString().contains(QStringLiteral("<a"), Qt::CaseInsensitive));
  EXPECT_TRUE(sanitized_body.toString().contains(QStringLiteral("<font color=")));

  ASSERT_TRUE(QMetaObject::invokeMethod(action_toast.get(), "activateBody"));
  EXPECT_EQ(services.notifications().lastActionId(), 42U);
  EXPECT_EQ(services.notifications().lastActionKey(), QStringLiteral("default"));

  QVariantMap dismiss_toast_model = action_toast_model;
  dismiss_toast_model[QStringLiteral("notifId")] = 43;
  dismiss_toast_model[QStringLiteral("hasDefaultAction")] = false;
  const auto dismiss_toast = createQmlObject(
      &engine, QUrl::fromLocalFile(source_root + QStringLiteral("/apps/shell/qml/Notifications/ToastItem.qml")),
      {{QStringLiteral("model"), dismiss_toast_model}});
  ASSERT_NE(dismiss_toast, nullptr);
  ASSERT_TRUE(QMetaObject::invokeMethod(dismiss_toast.get(), "activateBody"));
  EXPECT_EQ(services.notifications().lastDismissId(), 43U);
}

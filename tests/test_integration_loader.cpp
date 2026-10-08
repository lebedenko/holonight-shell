#include "IntegrationLoader.h"
#include "WorkspacePresentation.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace {
QList<IntegrationDescriptor> descriptors() {
  return {
      {
          .path = "a",
          .metadata = {{"id", "a"}, {"desktops", QJsonArray{"Alpha"}}, {"markers", QJsonArray{"ALPHA_SOCKET"}}},
      },
      {
          .path = "b",
          .metadata = {{"id", "b"}, {"desktops", QJsonArray{"Beta"}}, {"markers", QJsonArray{"BETA_SOCKET"}}},
      },
      {.path = "fallback", .metadata = {{"id", "fallback"}, {"fallback", true}}},
  };
}
QByteArray mappings() {
  QFile file("/proc/self/maps");
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}
void copy(const QString& source, const QString& directory) {
  ASSERT_TRUE(QDir().mkpath(directory));
  ASSERT_TRUE(QFile::copy(source, QDir(directory).filePath(QFileInfo(source).fileName())));
}
}  // namespace
TEST(IntegrationSelection, DeclarationsPrecedeMarkersAndAmbiguityUsesFallback) {
  QProcessEnvironment environment;
  environment.insert("XDG_CURRENT_DESKTOP", "X:aLpHa");
  environment.insert("BETA_SOCKET", "socket");
  EXPECT_EQ(IntegrationLoader::select(descriptors(), environment), "a");
  environment.insert("XDG_CURRENT_DESKTOP", "Alpha:Beta");
  EXPECT_EQ(IntegrationLoader::select(descriptors(), environment), "fallback");
  environment.remove("XDG_CURRENT_DESKTOP");
  EXPECT_EQ(IntegrationLoader::select(descriptors(), environment), "b");
  environment.insert("ALPHA_SOCKET", "socket");
  EXPECT_EQ(IntegrationLoader::select(descriptors(), environment), "fallback");
  EXPECT_EQ(IntegrationLoader::select(descriptors(), {}), "fallback");
}
TEST(IntegrationLoader, MetadataInspectionDoesNotLoadImplementations) {
  const auto found = IntegrationLoader::discover({PLUGIN_DIRECTORY});
  EXPECT_GE(found.size(), 3);
  EXPECT_FALSE(mappings().contains("libholonight_backend_hyprland"));
  EXPECT_FALSE(QFile::exists(":/HolonightShell/Integrations/Hyprland/SpecialWorkspaces.qml"));
}
TEST(IntegrationLoader, AdditionalPluginWorksThroughUnchangedContractsAfterRelocation) {
  QTemporaryDir relocated;
  const auto directory = relocated.filePath("lib/holonight/backends");
  copy(FAKE_PLUGIN, directory);
  QProcessEnvironment environment;
  environment.insert("XDG_CURRENT_DESKTOP", "TestDesktop");
  IntegrationLoader loader({directory}, environment);
  ASSERT_NE(loader.integration(), nullptr);
  EXPECT_EQ(loader.backendName(), "test-compositor");
  CompositorService service(loader.createCompositor());
  WorkspacePresentation presentation(&service, dynamic_cast<NumberedWorkspaceProvider*>(service.backend()));
  service.start();
  EXPECT_TRUE(service.connected());
  EXPECT_EQ(service.workspaces()->rowCount(), 1);
  EXPECT_TRUE(presentation.useNumericWorkspacePresentation());
  EXPECT_EQ(presentation.activeNumericWorkspaceForOutput("FAKE-1"), 7);
  presentation.activateNumberedSlot(5);
  EXPECT_EQ(service.backend()->property("activatedSlot").toInt(), 5);
  EXPECT_TRUE(loader.componentUrl().isEmpty());
  EXPECT_EQ(loader.integration()->createKeyboard(), nullptr);
}
TEST(IntegrationLoader, LabwcSelectsByDesktopOrMarkerAndDelegatesNamedWorkspaces) {
  const auto found = IntegrationLoader::discover({PLUGIN_DIRECTORY});
  QProcessEnvironment env;
  env.insert("XDG_CURRENT_DESKTOP", "HoloNight:labwc");
  env.insert("SWAYSOCK", "stale");
  EXPECT_EQ(IntegrationLoader::select(found, env), "labwc");
  env.remove("XDG_CURRENT_DESKTOP");
  env.remove("SWAYSOCK");
  env.insert("LABWC_PID", "1234");
  EXPECT_EQ(IntegrationLoader::select(found, env), "labwc");
  EXPECT_EQ(IntegrationLoader::select(found, {}), "wayland");
  QTemporaryDir isolated;
  copy(LABWC_PLUGIN, isolated.path());
  IntegrationLoader loader({isolated.path()}, env);
  ASSERT_NE(loader.integration(), nullptr);
  EXPECT_EQ(loader.backendName(), "labwc");
  auto backend = loader.createCompositor();
  ASSERT_NE(backend, nullptr);
  EXPECT_STREQ(backend->metaObject()->className(), "LabwcBackend");
  EXPECT_EQ(dynamic_cast<NumberedWorkspaceProvider*>(backend.get()), nullptr);
  EXPECT_EQ(loader.integration()->createKeyboard(), nullptr);
  EXPECT_TRUE(loader.componentUrl().isEmpty());
  EXPECT_FALSE(mappings().contains("libholonight_backend_sway"));
  EXPECT_FALSE(mappings().contains("libholonight_backend_hyprland"));
  EXPECT_FALSE(mappings().contains("libholonight_backend_wayland"));
}
TEST(IntegrationLoader, SwayLoadsWithAllInactivePluginFilesAbsent) {
  QTemporaryDir isolated;
  copy(SWAY_PLUGIN, isolated.path());
  QProcessEnvironment environment;
  environment.insert("XDG_CURRENT_DESKTOP", "Sway");
  IntegrationLoader loader({isolated.path()}, environment);
  ASSERT_NE(loader.integration(), nullptr);
  auto backend = loader.createCompositor();
  EXPECT_NE(backend, nullptr);
  EXPECT_EQ(loader.backendName(), "sway");
  EXPECT_EQ(loader.integration()->createKeyboard(), nullptr);
  EXPECT_FALSE(mappings().contains("libholonight_backend_hyprland"));
  EXPECT_FALSE(QFile::exists(":/HolonightShell/Integrations/Hyprland/SpecialWorkspaces.qml"));
}
TEST(IntegrationLoader, MissingAndIncompatibleSelectedPluginFallBackWithoutInstantiatingIt) {
  for (const bool incompatible : {false, true}) {
    QTemporaryDir isolated;
    copy(WAYLAND_PLUGIN, isolated.path());
    QFile catalog(isolated.filePath("selected.json"));
    ASSERT_TRUE(catalog.open(QIODevice::WriteOnly));
    catalog.write(QJsonDocument(QJsonObject{
                                    {"iid", HolonightIntegration_iid},
                                    {"id", "missing"},
                                    {"desktops", QJsonArray{"Missing"}},
                                    {"library", "missing.so"},
                                    {"shellVersion", incompatible ? "incompatible" : HOLONIGHT_INTEGRATION_VERSION},
                                })
                      .toJson());
    catalog.close();
    QProcessEnvironment environment;
    environment.insert("XDG_CURRENT_DESKTOP", "Missing");
    IntegrationLoader loader({isolated.path()}, environment);
    EXPECT_NE(loader.integration(), nullptr);
    EXPECT_EQ(loader.backendName(), "wayland");
    EXPECT_FALSE(loader.diagnostic().isEmpty());
  }
}
TEST(IntegrationLoader, MissingFallbackLeavesServicesUnavailable) {
  QTemporaryDir empty;
  IntegrationLoader loader({empty.path()}, {});
  EXPECT_EQ(loader.integration(), nullptr);
  CompositorService service(loader.createCompositor());
  service.start();
  EXPECT_FALSE(service.connected());
  EXPECT_EQ(service.workspaces()->rowCount(), 0);
  EXPECT_FALSE(loader.diagnostic().isEmpty());
}

TEST(IntegrationLoader, RejectsPreviousAbiInLibraryAndCatalog) {
  QTemporaryDir isolated;
  copy(OLD_PLUGIN, isolated.path());
  QFile catalog(isolated.filePath("old.json"));
  ASSERT_TRUE(catalog.open(QIODevice::WriteOnly));
  catalog.write(QJsonDocument(QJsonObject{
                                  {"iid", "org.holonight.Integration/2.0"},
                                  {"id", "old"},
                                  {"library", QFileInfo(OLD_PLUGIN).fileName()},
                                  {"shellVersion", HOLONIGHT_INTEGRATION_VERSION},
                              })
                    .toJson());
  catalog.close();
  EXPECT_TRUE(IntegrationLoader::discover({isolated.path()}).isEmpty());
  copy(WAYLAND_PLUGIN, isolated.path());
  IntegrationLoader loader({isolated.path()}, {});
  EXPECT_EQ(loader.backendName(), "wayland");
  EXPECT_FALSE(mappings().contains(QFileInfo(OLD_PLUGIN).fileName().toUtf8()));
}

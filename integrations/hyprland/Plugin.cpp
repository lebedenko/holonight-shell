#include "HyprlandBackend.h"
#include "HyprlandLayoutProvider.h"
#include "HyprlandSessionBackend.h"
#include "IntegrationPlugin.h"

class Plugin final : public QObject, public IntegrationPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID HolonightIntegration_iid FILE "metadata.json")
  Q_INTERFACES(IntegrationPlugin)
 public:
  std::unique_ptr<CompositorBackend> createCompositor() override { return std::make_unique<HyprlandBackend>(); }
  std::unique_ptr<SessionBackend> createSession(const ProcessEnvironment* env, CommandRunner* runner) override {
    return std::make_unique<HyprlandSessionBackend>(env, runner);
  }
  std::unique_ptr<KeyboardLayoutProvider> createKeyboard() override {
    return std::make_unique<HyprlandLayoutProvider>();
  }
  [[nodiscard]] QUrl topbarComponent() const override {
    return {QStringLiteral("qrc:/HolonightShell/Integrations/Hyprland/SpecialWorkspaces.qml")};
  }
};
#include "Plugin.moc"

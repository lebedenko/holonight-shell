#include "IntegrationPlugin.h"
#include "SwayBackend.h"
#include "SwaySessionBackend.h"

class Plugin final : public QObject, public IntegrationPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID HolonightIntegration_iid FILE "metadata.json")
  Q_INTERFACES(IntegrationPlugin)
 public:
  std::unique_ptr<CompositorBackend> createCompositor() override { return std::make_unique<SwayBackend>(); }
  std::unique_ptr<SessionBackend> createSession(const ProcessEnvironment* env, CommandRunner* runner) override {
    return std::make_unique<SwaySessionBackend>(env, runner);
  }
};
#include "Plugin.moc"

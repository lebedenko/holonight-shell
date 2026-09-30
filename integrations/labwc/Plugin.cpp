#include "LabwcBackend.h"
#include "IntegrationPlugin.h"
#include "LabwcSessionBackend.h"

class Plugin final : public QObject, public IntegrationPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID HolonightIntegration_iid FILE "metadata.json")
  Q_INTERFACES(IntegrationPlugin)
 public:
  std::unique_ptr<CompositorBackend> createCompositor() override { return std::make_unique<LabwcBackend>(); }
  std::unique_ptr<SessionBackend> createSession(const ProcessEnvironment* env, CommandRunner* runner) override {
    return std::make_unique<LabwcSessionBackend>(env, runner);
  }
};
#include "Plugin.moc"

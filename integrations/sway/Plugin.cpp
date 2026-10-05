#include "IntegrationPlugin.h"
#include "SwaySessionBackend.h"

#include <holonight_system/compositor/CompositorFactory.h>

class Plugin final : public QObject, public IntegrationPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID HolonightIntegration_iid FILE "metadata.json")
  Q_INTERFACES(IntegrationPlugin)
 public:
  std::unique_ptr<CompositorBackend> createCompositor() override {
    return createCompositorBackend(QStringLiteral("sway"));
  }
  std::unique_ptr<SessionBackend> createSession(const ProcessEnvironment* env, CommandRunner* runner) override {
    return std::make_unique<SwaySessionBackend>(env, runner);
  }
};
#include "Plugin.moc"

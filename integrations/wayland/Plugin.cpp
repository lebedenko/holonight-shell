#include "IntegrationPlugin.h"
#include "LogindSessionBackend.h"

#include <holonight_system/compositor/CompositorFactory.h>

class Plugin final : public QObject, public IntegrationPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID HolonightIntegration_iid FILE "metadata.json")
  Q_INTERFACES(IntegrationPlugin)
 public:
  std::unique_ptr<CompositorBackend> createCompositor() override {
    return createCompositorBackend(QStringLiteral("wayland"));
  }
  std::unique_ptr<SessionBackend> createSession(const ProcessEnvironment* env, CommandRunner* runner) override {
    return std::make_unique<LogindSessionBackend>(env, runner);
  }
};
#include "Plugin.moc"

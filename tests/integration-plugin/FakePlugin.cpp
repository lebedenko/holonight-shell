#include "IntegrationPlugin.h"
#include "NumberedWorkspaceProvider.h"
#include "session/LogindSessionBackend.h"

class FakeBackend final : public CompositorBackend, public NumberedWorkspaceProvider {
 public:
  [[nodiscard]] NumberedWorkspaceState numberedWorkspaces() const override {
    return {.eligible = true, .assignments = {{"opaque", 7}}};
  }
  void start() override { publish(); }
  void activateWorkspace(const QString& identifier) override {
    if (identifier == "opaque") {
      publish();
    }
  }
  void activateNumberedSlot(int slot) override { setProperty("activatedSlot", slot); }

 private:
  void publish() {
    emit snapshotReady({
        .connected = true,
        .capabilities = {.workspace_listing = true, .workspace_activation = true},
        .workspaces = {{.id = "opaque", .display_name = "seventh", .outputs = {"FAKE-1"}, .active = true}},
    });
  }
};
class FakePlugin final : public QObject, public IntegrationPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID HolonightIntegration_iid FILE "fake.json")
  Q_INTERFACES(IntegrationPlugin)
 public:
  std::unique_ptr<CompositorBackend> createCompositor() override { return std::make_unique<FakeBackend>(); }
  std::unique_ptr<SessionBackend> createSession(const ProcessEnvironment* env, CommandRunner* runner) override {
    return std::make_unique<LogindSessionBackend>(env, runner);
  }
};
#include "FakePlugin.moc"

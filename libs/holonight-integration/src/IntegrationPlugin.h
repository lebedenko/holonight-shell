#pragma once
#include "CompositorBackend.h"
#include "KeyboardLayoutProvider.h"
#include "session/SessionBackend.h"

#include <QUrl>
#include <QtPlugin>

#include <memory>

enum class WindowPresentationPolicy { Legacy, TaskManagement };

// Private, shell-versioned ABI. Providers are instance-owned, never global singletons.
class IntegrationPlugin {
 public:
  virtual ~IntegrationPlugin() = default;
  virtual std::unique_ptr<CompositorBackend> createCompositor() = 0;
  virtual std::unique_ptr<KeyboardLayoutProvider> createKeyboard() { return {}; }
  virtual std::unique_ptr<SessionBackend> createSession(const ProcessEnvironment* env, CommandRunner* runner) = 0;
  virtual WindowPresentationPolicy windowPresentationPolicy() const { return WindowPresentationPolicy::Legacy; }
  virtual QUrl topbarComponent() const { return {}; }
};
#define HolonightIntegration_iid "org.holonight.Integration/2.0"
Q_DECLARE_INTERFACE(IntegrationPlugin, HolonightIntegration_iid)

#pragma once
#include "CompositorBackend.h"
#include "KeyboardLayoutProvider.h"
#include "session/SessionBackend.h"

#include <QUrl>
#include <QtPlugin>

#include <memory>

// NOLINTNEXTLINE(performance-enum-size): Preserve plugin ABI.
enum class WindowPresentationPolicy { Legacy, TaskManagement };

// Private, shell-versioned ABI. Providers are instance-owned, never global singletons.
class IntegrationPlugin {
 public:
  IntegrationPlugin() = default;
  IntegrationPlugin(const IntegrationPlugin&) = default;
  IntegrationPlugin& operator=(const IntegrationPlugin&) = default;
  IntegrationPlugin(IntegrationPlugin&&) = default;
  IntegrationPlugin& operator=(IntegrationPlugin&&) = default;
  virtual ~IntegrationPlugin() = default;
  virtual std::unique_ptr<CompositorBackend> createCompositor() = 0;
  virtual std::unique_ptr<KeyboardLayoutProvider> createKeyboard() { return {}; }
  virtual std::unique_ptr<SessionBackend> createSession(const ProcessEnvironment* env, CommandRunner* runner) = 0;
  [[nodiscard]] virtual WindowPresentationPolicy windowPresentationPolicy() const {
    return WindowPresentationPolicy::Legacy;
  }
  [[nodiscard]] virtual QUrl topbarComponent() const { return {}; }
};
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Qt plugin literal.
#define HolonightIntegration_iid "org.holonight.Integration/3.0"
Q_DECLARE_INTERFACE(IntegrationPlugin, HolonightIntegration_iid)

#pragma once

#include "PolkitRequestCoordinator.h"

#include <QObject>
#include <QPointer>

#pragma push_macro("signals")
#pragma push_macro("slots")
#undef signals
#undef slots
#include <polkitagent/polkitagent.h>
#pragma pop_macro("slots")
#pragma pop_macro("signals")

#include <functional>

namespace Holonight::Authentication {

class PolkitListenerBridge final : public QObject {
 public:
  using RequestHandler = std::function<void(PolkitRequest)>;
  using CancelHandler = std::function<void(const QString&)>;
  struct RegistrationHooks {
    std::function<void*(PolkitAgentListener*, const QByteArray&, QString*)> register_session;
    std::function<void(void*)> unregister;
  };

  PolkitListenerBridge(RequestHandler request_handler, CancelHandler cancel_handler, RegistrationHooks hooks = {},
                       QObject* parent = nullptr);
  ~PolkitListenerBridge() override;

  bool registerForSession(const QByteArray& session_id, QString* error_message);
  void unregister();
  static bool registrationErrorIsConflict(const QString& message);
  void notifyAuthorityCancellation(const QString& token);
  void receive(const char* action_id, const char* message, PolkitDetails* details, const char* cookie,
               GList* identities, GCancellable* cancellable, GAsyncReadyCallback callback, void* user_data);

 private:
  RequestHandler request_handler_;
  CancelHandler cancel_handler_;
  RegistrationHooks registration_hooks_;
  PolkitAgentListener* listener_{};
  void* registration_{};
  quint64 next_token_{};
};

}  // namespace Holonight::Authentication

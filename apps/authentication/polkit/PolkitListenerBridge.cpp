#include "PolkitListenerBridge.h"

#include "ExternalText.h"
#include "PolkitStartupPolicy.h"

#include <QMetaObject>

#pragma push_macro("signals")
#pragma push_macro("slots")
#undef signals
#undef slots
#include <gio/gio.h>
#include <polkit/polkit.h>
#include <polkitagent/polkitagent.h>
#pragma pop_macro("slots")
#pragma pop_macro("signals")

#include <atomic>
#include <memory>

using Holonight::Authentication::PolkitListenerBridge;

struct HolonightListener {
  PolkitAgentListener parent;
  PolkitListenerBridge* bridge;
};
struct HolonightListenerClass {
  PolkitAgentListenerClass parent;
};
using HolonightListener = HolonightListener;
using HolonightListenerClass = HolonightListenerClass;
namespace {
G_DEFINE_TYPE(HolonightListener, holonight_listener, POLKIT_AGENT_TYPE_LISTENER)

struct Completion {
  explicit Completion(GTask* value) : task(value) {}
  Q_DISABLE_COPY_MOVE(Completion)
  ~Completion() { g_object_unref(task); }
  void finish(bool authorized) {
    if (done.exchange(true)) {
      return;
    }
    g_task_return_boolean(task, static_cast<gboolean>(authorized));
  }
  GTask* task;
  std::atomic_bool done;
};
struct CancellationContext {
  QPointer<PolkitListenerBridge> bridge;
  QString token;
};
void cancellableTriggered(GCancellable* /*unused*/, gpointer data) {
  auto* context = static_cast<CancellationContext*>(data);
  if (!context->bridge) {
    return;
  }
  QMetaObject::invokeMethod(
      context->bridge,
      [bridge = context->bridge, token = context->token] {
        if (bridge) {
          bridge->notifyAuthorityCancellation(token);
        }
      },
      Qt::QueuedConnection);
}
void destroyCancellationContext(gpointer data) { delete static_cast<CancellationContext*>(data); }

void initiateAuthentication(PolkitAgentListener* listener, const gchar* action_id, const gchar* message,
                            const gchar* /*unused*/, PolkitDetails* details, const gchar* cookie, GList* identities,
                            GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data) {
  // GObject instance layout/API.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-type-vararg)
  auto* self = reinterpret_cast<HolonightListener*>(listener);
  self->bridge->receive(action_id, message, details, cookie, identities, cancellable, callback, user_data);
}
gboolean finishAuthentication(PolkitAgentListener* /*unused*/, GAsyncResult* result, GError** error) {
  return g_task_propagate_boolean(G_TASK(result), error);
}
void holonight_listener_class_init(HolonightListenerClass* klass) {
  auto* listener_class = POLKIT_AGENT_LISTENER_CLASS(klass);
  listener_class->initiate_authentication = initiateAuthentication;
  listener_class->initiate_authentication_finish = finishAuthentication;
}
void holonight_listener_init(HolonightListener* listener) { listener->bridge = nullptr; }
}  // namespace

namespace Holonight::Authentication {
PolkitListenerBridge::PolkitListenerBridge(RequestHandler request_handler, CancelHandler cancel_handler,
                                           RegistrationHooks hooks, QObject* parent)
    : QObject(parent),
      request_handler_(std::move(request_handler)),
      cancel_handler_(std::move(cancel_handler)),
      registration_hooks_(std::move(hooks)) {
  // GObject instance layout/API.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-type-vararg)
  auto* listener = reinterpret_cast<HolonightListener*>(g_object_new(holonight_listener_get_type(), nullptr));
  listener->bridge = this;
  listener_ = POLKIT_AGENT_LISTENER(listener);
}
PolkitListenerBridge::~PolkitListenerBridge() {
  unregister();
  // GObject instance layout/API.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-type-vararg)
  reinterpret_cast<HolonightListener*>(listener_)->bridge = nullptr;
  g_object_unref(listener_);
}
bool PolkitListenerBridge::registerForSession(const QByteArray& session_id, QString* error_message) {
  if (registration_hooks_.register_session) {
    registration_ = registration_hooks_.register_session(listener_, session_id, error_message);
    return registration_ != nullptr;
  }
  PolkitSubject* subject = polkit_unix_session_new(session_id.constData());
  GError* error = nullptr;
  registration_ = polkit_agent_listener_register(listener_, POLKIT_AGENT_REGISTER_FLAGS_NONE, subject,
                                                 "/org/holonight/PolkitAgent", nullptr, &error);
  g_object_unref(subject);
  if (registration_ != nullptr) {
    return true;
  }
  if (error_message != nullptr) {
    *error_message = (error != nullptr) ? QString::fromUtf8(error->message) : QString{};
  }
  g_clear_error(&error);
  return false;
}
void PolkitListenerBridge::unregister() {
  if (registration_ == nullptr) {
    return;
  }
  if (registration_hooks_.unregister) {
    registration_hooks_.unregister(registration_);
  } else {
    polkit_agent_listener_unregister(registration_);
  }
  registration_ = nullptr;
}
bool PolkitListenerBridge::registrationErrorIsConflict(const QString& message) {
  return classifyPolkitRegistration(false, message) == PolkitRegistrationOutcome::Conflict;
}
void PolkitListenerBridge::notifyAuthorityCancellation(const QString& token) { cancel_handler_(token); }
void PolkitListenerBridge::receive(const char* action_id, const char* message, PolkitDetails* polkit_details,
                                   const char* cookie, GList* raw_identities, GCancellable* cancellable,
                                   GAsyncReadyCallback callback, void* user_data) {
  const QString token = QStringLiteral("request-%1").arg(++next_token_);
  auto completion = std::make_shared<Completion>(g_task_new(listener_, cancellable, callback, user_data));
  QMap<QString, QString> raw_details;
  gchar** keys = polkit_details_get_keys(polkit_details);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) -- GLib null-terminated string vector.
  for (gchar** key = keys; (key != nullptr) && ((*key) != nullptr); ++key) {
    const char* value = polkit_details_lookup(polkit_details, *key);
    raw_details.insert(QString::fromUtf8(*key), QString::fromUtf8((value != nullptr) ? value : ""));
  }
  g_strfreev(keys);
  QVariantMap details = safeRequesterDetails(raw_details);
  QList<Identity> identities;
  for (GList* item = raw_identities; item != nullptr; item = item->next) {
    auto* identity = POLKIT_IDENTITY(item->data);
    gchar* serialized = polkit_identity_to_string(identity);
    Identity mapped{.stable_id = QString::fromUtf8(serialized), .display_label = QString::fromUtf8(serialized)};
    g_free(serialized);
    if (POLKIT_IS_UNIX_USER(identity)) {
      mapped.uid = static_cast<uint>(polkit_unix_user_get_uid(POLKIT_UNIX_USER(identity)));
      mapped.has_uid = true;
      const char* name = polkit_unix_user_get_name(POLKIT_UNIX_USER(identity));
      if (name != nullptr) {
        mapped.display_label = QString::fromUtf8(name);
      }
    }
    identities.append(std::move(mapped));
  }
  auto retained_cancellable = std::shared_ptr<GCancellable>(G_CANCELLABLE(g_object_ref(cancellable)),
                                                            [](GCancellable* value) { g_object_unref(value); });
  QMetaObject::invokeMethod(
      this,
      [this, token, action = QString::fromUtf8((action_id != nullptr) ? action_id : ""),
       text = QString::fromUtf8((message != nullptr) ? message : ""), details = std::move(details),
       cookie_text = QString::fromUtf8((cookie != nullptr) ? cookie : ""), identities = std::move(identities),
       completion, retained_cancellable]() mutable {
        request_handler_(
            {.token = token,
             .action_id = action,
             .message = text,
             .details = std::move(details),
             .cookie = cookie_text,
             .identities = std::move(identities),
             .complete = [completion, retained_cancellable](bool authorized) { completion->finish(authorized); }});
      },
      Qt::QueuedConnection);
  g_cancellable_connect(cancellable, G_CALLBACK(cancellableTriggered),
                        new CancellationContext{.bridge = this, .token = token}, destroyCancellationContext);
}
}  // namespace Holonight::Authentication

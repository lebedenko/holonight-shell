#include "AuthenticationPromptModel.h"
#include "PolkitListenerBridge.h"
#include "PolkitRequestCoordinator.h"

#include <QCoreApplication>

#pragma push_macro("signals")
#pragma push_macro("slots")
#undef signals
#undef slots
#include <gio/gio.h>
#include <polkit/polkit.h>
#pragma pop_macro("slots")
#pragma pop_macro("signals")

#include <gtest/gtest.h>
#include <map>
#include <memory>

using namespace Holonight::Authentication;

namespace {
struct SessionState {
  PamSession::Callbacks callbacks;
  QStringList responses;
};

class Session final : public PamSession {
 public:
  explicit Session(std::shared_ptr<SessionState> state) : state_(std::move(state)) {}
  void initiate() override {}
  void respond(const QString& response) override { state_->responses.append(response); }
  void cancel() override {}

 private:
  std::shared_ptr<SessionState> state_;
};

struct Agent {
  AuthenticationPromptModel model;
  QList<std::shared_ptr<SessionState>> sessions;
  PolkitRequestCoordinator coordinator{
      &model, 1000, [this](const QString&, const QString&, quint64, PamSession::Callbacks callbacks) {
        auto state = std::make_shared<SessionState>(SessionState{.callbacks = std::move(callbacks)});
        sessions.append(state);
        return std::make_unique<Session>(state);
      }};
};

class FakeAuthority {
 public:
  PolkitListenerBridge::RegistrationHooks hooks() {
    return {.register_session = [this](PolkitAgentListener*, const QByteArray& session, QString* error) -> void* {
              if (registrations_.contains(session)) {
                if (error) {
                  *error = QStringLiteral("Agent already registered for session");
                }
                return nullptr;
              }
              auto token = std::make_unique<QByteArray>(session);
              void* address = token.get();
              registrations_.emplace(session, std::move(token));
              return address;
            },
            .unregister =
                [this](void* registration) {
                  for (auto iterator = registrations_.begin(); iterator != registrations_.end(); ++iterator) {
                    if (iterator->second.get() == registration) {
                      registrations_.erase(iterator);
                      return;
                    }
                  }
                }};
  }

 private:
  std::map<QByteArray, std::unique_ptr<QByteArray>> registrations_;
};

struct AsyncResult {
  int calls{};
  bool authorized{};
};

void authenticationFinished(GObject* /*unused*/, GAsyncResult* result, gpointer data) {
  auto* state = static_cast<AsyncResult*>(data);
  GError* error = nullptr;
  state->authorized = (g_task_propagate_boolean(G_TASK(result), &error) != 0);
  EXPECT_EQ(error, nullptr);
  g_clear_error(&error);
  ++state->calls;
}

void submitRequest(PolkitListenerBridge& bridge, AsyncResult* result) {
  PolkitDetails* details = polkit_details_new();
  polkit_details_insert(details, "application", "Integration test");
  PolkitIdentity* identity = polkit_unix_user_new(1000);
  GList* identities = g_list_append(nullptr, identity);
  GCancellable* cancellable = g_cancellable_new();
  bridge.receive("org.example.authenticate", "Authenticate", details, "cookie", identities, cancellable,
                 authenticationFinished, result);
  g_object_unref(cancellable);
  g_list_free(identities);
  g_object_unref(identity);
  g_object_unref(details);
}

void drainEvents() {
  for (int iteration = 0; iteration < 20; ++iteration) {
    QCoreApplication::processEvents();
    while (g_main_context_iteration(nullptr, 0) != 0) {
    }
  }
}

TEST(PolkitBridgeIntegration, RegistrationConflictPreservesFirstRegistration) {
  FakeAuthority authority;
  PolkitListenerBridge first([](const PolkitRequest&) {}, [](const QString&) {}, authority.hooks());
  PolkitListenerBridge competitor([](const PolkitRequest&) {}, [](const QString&) {}, authority.hooks());
  QString error;
  EXPECT_TRUE(first.registerForSession("session-a", &error));
  EXPECT_FALSE(competitor.registerForSession("session-a", &error));
  EXPECT_TRUE(PolkitListenerBridge::registrationErrorIsConflict(error));
  first.unregister();
  error.clear();
  EXPECT_TRUE(competitor.registerForSession("session-a", &error));
}

TEST(PolkitBridgeIntegration, TwoSessionsForOneUidRemainIndependentlyRouted) {
  FakeAuthority authority;
  Agent first_agent;
  Agent second_agent;
  PolkitListenerBridge first_bridge(
      [&first_agent](PolkitRequest request) { first_agent.coordinator.enqueue(std::move(request)); },
      [&first_agent](const QString& token) { first_agent.coordinator.cancel(token); }, authority.hooks());
  PolkitListenerBridge second_bridge(
      [&second_agent](PolkitRequest request) { second_agent.coordinator.enqueue(std::move(request)); },
      [&second_agent](const QString& token) { second_agent.coordinator.cancel(token); }, authority.hooks());
  QString error;
  ASSERT_TRUE(first_bridge.registerForSession("session-a", &error));
  ASSERT_TRUE(second_bridge.registerForSession("session-b", &error));

  AsyncResult first_result;
  AsyncResult second_result;
  submitRequest(first_bridge, &first_result);
  submitRequest(second_bridge, &second_result);
  drainEvents();
  ASSERT_EQ(first_agent.sessions.size(), 1);
  ASSERT_EQ(second_agent.sessions.size(), 1);
  first_agent.sessions.front()->callbacks.prompt(QStringLiteral("First password"), false);
  second_agent.sessions.front()->callbacks.prompt(QStringLiteral("Second password"), false);
  first_agent.model.respond(QStringLiteral("first-secret"));
  second_agent.model.respond(QStringLiteral("second-secret"));
  EXPECT_EQ(first_agent.sessions.front()->responses, QStringList({QStringLiteral("first-secret")}));
  EXPECT_EQ(second_agent.sessions.front()->responses, QStringList({QStringLiteral("second-secret")}));

  first_agent.sessions.front()->callbacks.completed(true);
  second_agent.coordinator.cancel(second_agent.coordinator.activeToken());
  drainEvents();
  EXPECT_EQ(first_result.calls, 1);
  EXPECT_TRUE(first_result.authorized);
  EXPECT_EQ(second_result.calls, 1);
  EXPECT_FALSE(second_result.authorized);
}
}  // namespace

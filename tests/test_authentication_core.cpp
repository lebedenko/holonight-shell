#include "AskpassMode.h"
#include "AuthenticationPromptModel.h"
#include "ExternalText.h"
#include "PolkitRequestCoordinator.h"
#include "PolkitStartupPolicy.h"
#include "ProtocolWriter.h"
#include "SecretValidator.h"

#include <QCoreApplication>
#include <QFile>
#include <QMetaMethod>
#include <QMetaProperty>

#include <cerrno>
#include <gtest/gtest.h>

using namespace Holonight::Authentication;

namespace {
struct FakeSessionState {
  PamSession::Callbacks callbacks;
  QString identity;
  QString cookie;
  quint64 generation{};
  QStringList responses;
  int initiated{};
  int cancelled{};
};
class FakeSession final : public PamSession {
 public:
  explicit FakeSession(std::shared_ptr<FakeSessionState> state) : state_(std::move(state)) {}
  void initiate() override { ++state_->initiated; }
  void respond(const QString& response) override { state_->responses.append(response); }
  void cancel() override { ++state_->cancelled; }

 private:
  std::shared_ptr<FakeSessionState> state_;
};
struct CoordinatorFixture {
  AuthenticationPromptModel model;
  QList<std::shared_ptr<FakeSessionState>> sessions;
  PolkitRequestCoordinator coordinator{
      &model, 1000,
      [this](const QString& identity, const QString& cookie, quint64 generation, PamSession::Callbacks callbacks) {
        auto state = std::make_shared<FakeSessionState>(FakeSessionState{
            .callbacks = std::move(callbacks), .identity = identity, .cookie = cookie, .generation = generation});
        sessions.append(state);
        return std::make_unique<FakeSession>(state);
      }};
};
Identity identity(const QString& stable_id, uint uid) {
  return {.stable_id = stable_id, .display_label = stable_id, .uid = uid, .has_uid = true};
}
PolkitRequest request(const QString& token, QList<Identity> identities, QList<bool>* results) {
  return {.token = token,
          .action_id = QStringLiteral("org.example.action"),
          .message = QStringLiteral("Authenticate"),
          .cookie = QStringLiteral("cookie-%1").arg(token),
          .identities = std::move(identities),
          .complete = [results](bool result) { results->append(result); }};
}
void drainEvents() { QCoreApplication::processEvents(); }
}  // namespace

TEST(AuthenticationText, NormalizesControlsLinesAndLimits) {
  EXPECT_EQ(normalizeExternalText(QStringLiteral("a\r\nb\rc\0d"), {100, 100, 3}), QStringLiteral("a\nb\nc�d"));
  EXPECT_EQ(normalizeExternalText(QStringLiteral("a\n\n\n\nb"), {100, 100, 10}), QStringLiteral("a\n\nb"));
  EXPECT_EQ(normalizeExternalText(QStringLiteral("one\ntwo\nthree\nfour"), {100, 100, 3}),
            QStringLiteral("one\ntwo\nthree…"));
  const QString bounded =
      normalizeExternalText(QStringLiteral("123456"), {.max_bytes = 5, .max_code_points = 5, .max_lines = 1});
  EXPECT_LE(bounded.toUtf8().size(), 5);
  EXPECT_TRUE(bounded.endsWith(QChar(0x2026)));
}

TEST(AuthenticationText, FiltersUnsafeRequesterDetails) {
  const auto details = safeRequesterDetails({{QStringLiteral("application"), QStringLiteral("Settings")},
                                             {QStringLiteral("vendor"), QStringLiteral("/usr/bin/tool")},
                                             {QStringLiteral("command"), QStringLiteral("safe-looking")}});
  EXPECT_EQ(details.size(), 1);
  EXPECT_EQ(details.value(QStringLiteral("application")).toString(), QStringLiteral("Settings"));
}

TEST(AuthenticationSecret, EnforcesProtocolBoundary) {
  EXPECT_TRUE(validateSecret(QString::fromUtf8("pÃ¤ss")));
  EXPECT_TRUE(validateSecret(QString(1022, QLatin1Char('x'))));
  EXPECT_EQ(validateSecret(QString()).error, SecretValidationError::Empty);
  EXPECT_EQ(validateSecret(QStringLiteral("a\nb")).error, SecretValidationError::ForbiddenCharacter);
  EXPECT_EQ(validateSecret(QString(1023, QLatin1Char('x'))).error, SecretValidationError::TooLong);
  QString unpaired;
  unpaired.append(QChar(0xD800));
  EXPECT_EQ(validateSecret(unpaired).error, SecretValidationError::InvalidUnicode);
}

TEST(AuthenticationWriter, RetriesInterruptionsAndPartialWrites) {
  QByteArray output;
  int calls = 0;
  const bool okay = writeProtocolSecret(1, QByteArrayLiteral("secret"), [&](int, const void* data, size_t length) {
    ++calls;
    if (calls == 1) {
      errno = EINTR;
      return ssize_t{-1};
    }
    const auto count = qMin(length, size_t{2});
    output.append(static_cast<const char*>(data), static_cast<qsizetype>(count));
    return static_cast<ssize_t>(count);
  });
  EXPECT_TRUE(okay);
  EXPECT_EQ(output, QByteArrayLiteral("secret\n"));
}

TEST(AuthenticationAskpass, DispatchesFromBasenameAndExactHint) {
  using Mode = AuthenticationPromptModel::InputMode;
  EXPECT_EQ(askpassMode(QStringLiteral("holonight-sudo-askpass"), QStringLiteral("confirm")), Mode::Secret);
  EXPECT_EQ(askpassMode(QStringLiteral("holonight-ssh-askpass"), QStringLiteral("confirm")), Mode::Confirmation);
  EXPECT_EQ(askpassMode(QStringLiteral("holonight-askpass"), QStringLiteral("none")), Mode::Notification);
  EXPECT_EQ(askpassMode(QStringLiteral("holonight-ssh-askpass"), QStringLiteral("Confirm")), Mode::Secret);
}

TEST(AuthenticationModels, PreserveIdentityOrderAndStableSelection) {
  IdentityListModel model;
  model.setItems(
      {{.stable_id = QStringLiteral("first"), .display_label = QStringLiteral("Same"), .uid = 0, .has_uid = false},
       {.stable_id = QStringLiteral("current"),
        .display_label = QStringLiteral("Same"),
        .uid = 1000,
        .has_uid = true}});
  EXPECT_EQ(model.rowCount(), 2);
  EXPECT_EQ(model.data(model.index(0), IdentityListModel::StableIdRole).toString(), QStringLiteral("first"));
  EXPECT_EQ(model.preferred(1000), QStringLiteral("current"));
}

TEST(AuthenticationPrompt, RejectsInvalidOperationsAndClearsOnSubmission) {
  AuthenticationPromptModel model;
  int callbacks = 0;
  QString response;
  QObject::connect(&model, &AuthenticationPromptModel::clearSensitiveInput, [&] { response.clear(); });
  model.beginRequest({.token = QStringLiteral("opaque"),
                      .prompt = QStringLiteral("Password"),
                      .input_mode = AuthenticationPromptModel::InputMode::Secret},
                     [&](auto kind, const QString& value) {
                       ++callbacks;
                       EXPECT_EQ(kind, AuthenticationPromptModel::ResponseKind::Text);
                       response = value;
                     });
  model.confirm(true);
  EXPECT_EQ(callbacks, 0);
  model.respond(QStringLiteral("marker-secret"));
  EXPECT_EQ(callbacks, 1);
  EXPECT_EQ(response, QStringLiteral("marker-secret"));
  EXPECT_FALSE(model.property("response").isValid());
}

TEST(PolkitCoordinator, SelectsCurrentUidButRequiresExplicitMultipleIdentityConfirmation) {
  CoordinatorFixture fixture;
  QList<bool> results;
  fixture.coordinator.enqueue(request(
      QStringLiteral("one"), {identity(QStringLiteral("root"), 0), identity(QStringLiteral("user"), 1000)}, &results));
  drainEvents();
  EXPECT_EQ(fixture.model.lifecycleState(), AuthenticationPromptModel::LifecycleState::SelectingIdentity);
  EXPECT_EQ(fixture.model.selectedIdentity(), QStringLiteral("user"));
  EXPECT_TRUE(fixture.sessions.isEmpty());
  fixture.model.selectIdentity(QStringLiteral("root"));
  ASSERT_EQ(fixture.sessions.size(), 1);
  EXPECT_EQ(fixture.sessions.front()->identity, QStringLiteral("root"));
  EXPECT_EQ(fixture.sessions.front()->initiated, 1);
}

TEST(PolkitCoordinator, PreservesDuplicatesAndUsesFirstFallbackIdentity) {
  CoordinatorFixture fixture;
  QList<bool> results;
  fixture.coordinator.enqueue(request(
      QStringLiteral("one"), {identity(QStringLiteral("admin"), 10), identity(QStringLiteral("admin"), 10)}, &results));
  drainEvents();
  EXPECT_EQ(fixture.model.selectedIdentity(), QStringLiteral("admin"));
  EXPECT_EQ(fixture.model.identities()->rowCount(), 2);
  fixture.model.selectIdentity(QStringLiteral("admin"));
  ASSERT_EQ(fixture.sessions.size(), 1);
}

TEST(PolkitCoordinator, RejectsEmptyIdentitiesWithoutPresenting) {
  CoordinatorFixture fixture;
  QList<bool> results;
  int presentations = 0;
  QObject::connect(&fixture.coordinator, &PolkitRequestCoordinator::requestPresented, [&] { ++presentations; });
  fixture.coordinator.enqueue(request(QStringLiteral("empty"), {}, &results));
  drainEvents();
  EXPECT_EQ(results, QList<bool>({false}));
  EXPECT_EQ(presentations, 0);
  EXPECT_TRUE(fixture.sessions.isEmpty());
}

TEST(PolkitCoordinator, PresentsFifoAndCancelsQueuedRequestExactlyOnce) {
  CoordinatorFixture fixture;
  QList<bool> first_results;
  QList<bool> second_results;
  QList<bool> third_results;
  fixture.coordinator.enqueue(
      request(QStringLiteral("first"), {identity(QStringLiteral("user"), 1000)}, &first_results));
  fixture.coordinator.enqueue(
      request(QStringLiteral("second"), {identity(QStringLiteral("user"), 1000)}, &second_results));
  fixture.coordinator.enqueue(
      request(QStringLiteral("third"), {identity(QStringLiteral("user"), 1000)}, &third_results));
  fixture.coordinator.cancel(QStringLiteral("second"));
  drainEvents();
  ASSERT_EQ(fixture.sessions.size(), 1);
  fixture.sessions[0]->callbacks.completed(true);
  drainEvents();
  ASSERT_EQ(fixture.sessions.size(), 2);
  EXPECT_EQ(fixture.coordinator.activeToken(), QStringLiteral("third"));
  EXPECT_EQ(first_results, QList<bool>({true}));
  EXPECT_EQ(second_results, QList<bool>({false}));
  fixture.sessions[0]->callbacks.completed(false);
  EXPECT_EQ(first_results.size(), 1);
}

TEST(PolkitCoordinator, MapsMixedPromptsMessagesAndRoutesResponsesToOriginatingSession) {
  CoordinatorFixture fixture;
  QList<bool> results;
  fixture.coordinator.enqueue(request(QStringLiteral("one"), {identity(QStringLiteral("user"), 1000)}, &results));
  drainEvents();
  auto session = fixture.sessions.front();
  session->callbacks.information(QStringLiteral("Information"));
  session->callbacks.prompt(QStringLiteral("Login"), true);
  EXPECT_EQ(fixture.model.inputMode(), AuthenticationPromptModel::InputMode::Visible);
  fixture.model.respond(QStringLiteral("alice"));
  session->callbacks.error(QStringLiteral("Try a password"));
  session->callbacks.prompt(QStringLiteral("Password"), false);
  EXPECT_EQ(fixture.model.inputMode(), AuthenticationPromptModel::InputMode::Secret);
  fixture.model.respond(QStringLiteral("secret"));
  EXPECT_EQ(session->responses, QStringList({QStringLiteral("alice"), QStringLiteral("secret")}));
  EXPECT_EQ(fixture.model.messages()->rowCount(), 2);
}

TEST(PolkitCoordinator, FailedAttemptRetriesWithFreshSessionAndRejectsStaleGeneration) {
  CoordinatorFixture fixture;
  QList<bool> results;
  fixture.coordinator.enqueue(request(
      QStringLiteral("one"), {identity(QStringLiteral("root"), 0), identity(QStringLiteral("user"), 1000)}, &results));
  drainEvents();
  fixture.model.selectIdentity(QStringLiteral("user"));
  auto old_session = fixture.sessions.front();
  old_session->callbacks.completed(false);
  EXPECT_EQ(fixture.model.lifecycleState(), AuthenticationPromptModel::LifecycleState::RetryableError);
  fixture.model.retry();
  EXPECT_EQ(fixture.model.lifecycleState(), AuthenticationPromptModel::LifecycleState::SelectingIdentity);
  fixture.model.selectIdentity(QStringLiteral("root"));
  ASSERT_EQ(fixture.sessions.size(), 2);
  EXPECT_GT(fixture.sessions[1]->generation, old_session->generation);
  EXPECT_EQ(fixture.sessions[1]->identity, QStringLiteral("root"));
  old_session->callbacks.completed(true);
  EXPECT_TRUE(results.isEmpty());
  fixture.sessions[1]->callbacks.completed(true);
  EXPECT_EQ(results, QList<bool>({true}));
}

TEST(PolkitCoordinator, ActiveCancellationShutdownAndReentrantCompletionAreExactlyOnce) {
  CoordinatorFixture fixture;
  QList<bool> first_results;
  QList<bool> second_results;
  auto first = request(QStringLiteral("first"), {identity(QStringLiteral("user"), 1000)}, &first_results);
  first.complete = [&](bool result) {
    first_results.append(result);
    fixture.coordinator.cancel(QStringLiteral("first"));
  };
  fixture.coordinator.enqueue(std::move(first));
  fixture.coordinator.enqueue(
      request(QStringLiteral("second"), {identity(QStringLiteral("user"), 1000)}, &second_results));
  drainEvents();
  auto active_session = fixture.sessions.front();
  fixture.coordinator.cancel(QStringLiteral("first"));
  EXPECT_EQ(active_session->cancelled, 1);
  EXPECT_EQ(first_results, QList<bool>({false}));
  fixture.coordinator.shutdown();
  EXPECT_EQ(second_results, QList<bool>({false}));
  active_session->callbacks.completed(true);
  EXPECT_EQ(first_results.size(), 1);
}

TEST(PolkitStartup, ClassifiesRegistrationSuccessFailureAndConflict) {
  EXPECT_EQ(classifyPolkitRegistration(true, QStringLiteral("ignored")), PolkitRegistrationOutcome::Success);
  EXPECT_EQ(classifyPolkitRegistration(false, QStringLiteral("connection unavailable")),
            PolkitRegistrationOutcome::Failure);
  EXPECT_EQ(classifyPolkitRegistration(false, QStringLiteral("Agent already registered")),
            PolkitRegistrationOutcome::Conflict);
  EXPECT_EQ(classifyPolkitRegistration(false, QStringLiteral("object already exists")),
            PolkitRegistrationOutcome::Conflict);
}

TEST(PolkitIntegration, IndependentAgentInstancesDoNotCrossRouteSameUidSessions) {
  CoordinatorFixture first;
  CoordinatorFixture second;
  QList<bool> first_results;
  QList<bool> second_results;
  first.coordinator.enqueue(
      request(QStringLiteral("session-a"), {identity(QStringLiteral("user"), 1000)}, &first_results));
  second.coordinator.enqueue(
      request(QStringLiteral("session-b"), {identity(QStringLiteral("user"), 1000)}, &second_results));
  drainEvents();
  first.sessions.front()->callbacks.prompt(QStringLiteral("First"), false);
  second.sessions.front()->callbacks.prompt(QStringLiteral("Second"), false);
  first.model.respond(QStringLiteral("first-secret"));
  second.model.respond(QStringLiteral("second-secret"));
  EXPECT_EQ(first.sessions.front()->responses, QStringList({QStringLiteral("first-secret")}));
  EXPECT_EQ(second.sessions.front()->responses, QStringList({QStringLiteral("second-secret")}));
}

TEST(PolkitSecurity, HasNoPasswordOutputOrRemoteControlPath) {
  const QString source_root = QString::fromUtf8(TEST_SOURCE_DIR) + QStringLiteral("/apps/authentication/polkit/");
  for (const QString& name : {QStringLiteral("main.cpp"), QStringLiteral("PolkitListenerBridge.cpp"),
                              QStringLiteral("PolkitSessionAdapter.cpp")}) {
    QFile file(source_root + name);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QByteArray source = file.readAll();
    EXPECT_FALSE(source.contains("STDOUT_FILENO"));
    EXPECT_FALSE(source.contains("QDBus"));
    EXPECT_FALSE(source.contains("control.sock"));
    EXPECT_FALSE(source.contains("QProcess"));
  }
}

TEST(AuthenticationText, PreservesSupplementaryUnicodeAtByteAndCodePointBoundaries) {
  const QString face = QString::fromUtf8("\xF0\x9F\x98\x80");
  EXPECT_EQ(normalizeExternalText(face, {4, 1, 1}), face);
  EXPECT_EQ(normalizeExternalText(face + "x", {4, 1, 1}), QStringLiteral("…"));
  EXPECT_EQ(normalizeExternalText(QStringLiteral("x"), {0, 0, 0}), QString{});
}

TEST(AuthenticationWriter, StopsOnZeroAndHardFailureIncludingAfterPartialOutput) {
  for (const ssize_t failure : {ssize_t{0}, ssize_t{-1}}) {
    int calls = 0;
    EXPECT_FALSE(writeProtocolSecret(1, QByteArrayLiteral("marker"), [&](int, const void*, size_t) {
      ++calls;
      errno = EIO;
      return failure;
    }));
    EXPECT_EQ(calls, 1);
  }
  int calls = 0;
  EXPECT_FALSE(writeProtocolSecret(1, QByteArrayLiteral("marker"), [&](int, const void*, size_t) {
    if (++calls == 1) return ssize_t{2};
    errno = EPIPE;
    return ssize_t{-1};
  }));
  EXPECT_EQ(calls, 2);
}

TEST(AuthenticationPrompt, ConsumesEachPromptBeforeCallingController) {
  using Model = AuthenticationPromptModel;
  for (const auto mode : {Model::InputMode::Visible, Model::InputMode::Secret, Model::InputMode::Confirmation,
                          Model::InputMode::Notification}) {
    Model model;
    int calls = 0;
    bool cleared = false;
    QObject::connect(&model, &Model::clearSensitiveInput, [&] { cleared = true; });
    ASSERT_TRUE(model.beginRequest({.token = QStringLiteral("one"), .input_mode = mode}, [&](auto, const QString&) {
      EXPECT_TRUE(cleared);
      EXPECT_EQ(model.lifecycleState(), Model::LifecycleState::Busy);
      ++calls;
    }));
    cleared = false;
    const auto submit = [&] {
      if (mode == Model::InputMode::Confirmation) {
        model.confirm(true);
      } else if (mode == Model::InputMode::Notification) {
        model.acknowledge();
      } else {
        model.respond(QStringLiteral("synthetic-marker"));
      }
    };
    submit();
    submit();
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(model.complete());
    EXPECT_FALSE(model.complete());
    model.cancel();
    EXPECT_EQ(calls, 1);
  }
}

TEST(PolkitCoordinator, IgnoresAllCallbacksAfterFailedSessionBeforeRetry) {
  CoordinatorFixture fixture;
  QList<bool> results;
  fixture.coordinator.enqueue(request(QStringLiteral("one"), {identity(QStringLiteral("user"), 1000)}, &results));
  drainEvents();
  auto old_session = fixture.sessions.front();
  old_session->callbacks.completed(false);
  old_session->callbacks.completed(true);
  old_session->callbacks.prompt(QStringLiteral("stale"), false);
  old_session->callbacks.information(QStringLiteral("stale"));
  EXPECT_TRUE(results.isEmpty());
  EXPECT_EQ(fixture.model.lifecycleState(), AuthenticationPromptModel::LifecycleState::RetryableError);
  EXPECT_NE(fixture.model.currentPrompt(), QStringLiteral("stale"));
  EXPECT_EQ(fixture.model.messages()->rowCount(), 1);
  fixture.coordinator.shutdown();
}

// This enumerates the complete state/mode rejection matrix in one place.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(AuthenticationPrompt, RejectsOperationsOutsideTheirModeAndLifecycle) {
  using Model = AuthenticationPromptModel;
  for (const auto mode : {Model::InputMode::None, Model::InputMode::Visible, Model::InputMode::Secret,
                          Model::InputMode::Confirmation, Model::InputMode::Notification}) {
    for (const auto state :
         {Model::LifecycleState::Idle, Model::LifecycleState::SelectingIdentity, Model::LifecycleState::AwaitingInput,
          Model::LifecycleState::Busy, Model::LifecycleState::RetryableError, Model::LifecycleState::Completed,
          Model::LifecycleState::Cancelled}) {
      Model model;
      int calls = 0;
      if (state != Model::LifecycleState::Idle) {
        Model::Request context{.token = QStringLiteral("matrix"), .input_mode = mode};
        if (state == Model::LifecycleState::SelectingIdentity) {
          context.identities = {identity(QStringLiteral("first"), 0), identity(QStringLiteral("second"), 1000)};
        }
        ASSERT_TRUE(model.beginRequest(std::move(context), [&](auto, const QString&) { ++calls; }));
        if (state == Model::LifecycleState::Busy) {
          ASSERT_TRUE(model.markBusy());
        }
        if (state == Model::LifecycleState::RetryableError) {
          ASSERT_TRUE(model.markRetryableError({}));
        }
        if (state == Model::LifecycleState::Completed) {
          ASSERT_TRUE(model.complete());
        }
        if (state == Model::LifecycleState::Cancelled) {
          model.cancel();
        }
      }
      const int before = calls;
      const bool awaiting = state == Model::LifecycleState::AwaitingInput;
      if (!awaiting || (mode != Model::InputMode::Visible && mode != Model::InputMode::Secret)) {
        model.respond(QStringLiteral("invalid-marker"));
      }
      if (!awaiting || mode != Model::InputMode::Confirmation) {
        model.confirm(true);
      }
      if (!awaiting || mode != Model::InputMode::Notification) {
        model.acknowledge();
      }
      model.selectIdentity(QStringLiteral("ineligible"));
      if (state != Model::LifecycleState::RetryableError) {
        model.retry();
      }
      EXPECT_EQ(calls, before);
      EXPECT_EQ(model.lifecycleState(), state);
    }
  }
}

TEST(AuthenticationPrompt, ClearsEverySensitiveEdgeAndNeverPublishesResponse) {
  using Model = AuthenticationPromptModel;
  Model model;
  const QString marker = QStringLiteral("synthetic-response-exposure-marker");
  QString field;
  int deliveries = 0;
  QObject::connect(&model, &Model::clearSensitiveInput, [&] { field.clear(); });
  const auto scan = [&] {
    EXPECT_TRUE(field.isEmpty());
    const auto* meta = model.metaObject();
    for (int index = 0; index < meta->propertyCount(); ++index) {
      EXPECT_FALSE(meta->property(index).read(&model).toString().contains(marker));
    }
    for (int index = meta->methodOffset(); index < meta->methodCount(); ++index) {
      const auto method = meta->method(index);
      if (method.methodType() == QMetaMethod::Signal) {
        EXPECT_EQ(method.parameterCount(), 0);
      }
    }
  };
  const auto begin = [&](const QString& token) {
    return model.beginRequest({.token = token, .input_mode = Model::InputMode::Secret},
                              [&](auto kind, const QString& value) {
                                EXPECT_TRUE(field.isEmpty());
                                if (kind == Model::ResponseKind::Text) {
                                  EXPECT_EQ(value, marker);
                                  ++deliveries;
                                }
                              });
  };
  field = marker;
  ASSERT_TRUE(begin(QStringLiteral("one")));
  scan();
  field = marker;
  model.respond(marker);
  scan();
  field = marker;
  ASSERT_TRUE(model.markRetryableError({}));
  scan();
  field = marker;
  model.retry();
  scan();
  field = marker;
  ASSERT_TRUE(model.presentPrompt(QStringLiteral("Again"), Model::InputMode::Secret));
  scan();
  field = marker;
  model.cancel();
  scan();
  field = marker;
  ASSERT_TRUE(begin(QStringLiteral("two")));
  scan();
  field = marker;
  ASSERT_TRUE(model.complete());
  scan();
  ASSERT_TRUE(begin(QStringLiteral("three")));
  field = marker;
  model.shutdown();
  scan();
  EXPECT_EQ(deliveries, 1);
}

TEST(PolkitCoordinator, LateCallbacksAfterDestructionAreHarmless) {
  QList<bool> results;
  std::shared_ptr<FakeSessionState> session;
  {
    CoordinatorFixture fixture;
    fixture.coordinator.enqueue(request(QStringLiteral("one"), {identity(QStringLiteral("user"), 1000)}, &results));
    drainEvents();
    session = fixture.sessions.front();
  }
  session->callbacks.prompt(QStringLiteral("stale"), false);
  session->callbacks.information(QStringLiteral("stale"));
  session->callbacks.error(QStringLiteral("stale"));
  session->callbacks.completed(true);
  EXPECT_EQ(results, QList<bool>({false}));
}

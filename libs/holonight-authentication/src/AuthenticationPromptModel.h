#pragma once

#include "IdentityListModel.h"
#include "MessageListModel.h"

#include <QObject>
#include <QVariantMap>

#include <functional>

namespace Holonight::Authentication {

class AuthenticationPromptModel : public QObject {
  Q_OBJECT
  Q_PROPERTY(FrontendKind frontendKind READ frontendKind NOTIFY contextChanged FINAL)
  Q_PROPERTY(QVariantMap selectedAccount READ selectedAccount NOTIFY selectedIdentityChanged FINAL)
  Q_PROPERTY(QString requestMessage READ requestMessage NOTIFY contextChanged FINAL)
  Q_PROPERTY(QString requestReference READ requestReference NOTIFY contextChanged FINAL)
  Q_PROPERTY(QVariantMap requesterDetails READ requesterDetails NOTIFY contextChanged FINAL)
  Q_PROPERTY(QString currentPrompt READ currentPrompt NOTIFY promptChanged FINAL)
  Q_PROPERTY(InputMode inputMode READ inputMode NOTIFY promptChanged FINAL)
  Q_PROPERTY(QAbstractItemModel* identities READ identities CONSTANT FINAL)
  Q_PROPERTY(QString selectedIdentity READ selectedIdentity NOTIFY selectedIdentityChanged FINAL)
  Q_PROPERTY(QAbstractItemModel* messages READ messages CONSTANT FINAL)
  Q_PROPERTY(LifecycleState lifecycleState READ lifecycleState NOTIFY lifecycleStateChanged FINAL)
  Q_PROPERTY(QString requestToken READ requestToken NOTIFY requestTokenChanged FINAL)
  Q_PROPERTY(qulonglong sessionGeneration READ sessionGeneration NOTIFY sessionGenerationChanged FINAL)

 public:
  enum class FrontendKind : quint8 { GenericAskpass, Polkit, SudoAskpass, SshAskpass };
  Q_ENUM(FrontendKind)
  enum class InputMode : quint8 { None, Visible, Secret, Confirmation, Notification };
  Q_ENUM(InputMode)
  enum class LifecycleState : quint8 {
    Idle,
    SelectingIdentity,
    AwaitingInput,
    Busy,
    RetryableError,
    Completed,
    Cancelled
  };
  Q_ENUM(LifecycleState)
  enum class ResponseKind : quint8 { Text, Confirmation, Acknowledgement, Cancellation, Identity, Retry };

  struct Request {
    QString token;
    QString message;
    QString reference;
    QVariantMap details;
    QList<Identity> identities;
    QString preferred_identity;
    QString prompt;
    InputMode input_mode{InputMode::None};
    FrontendKind frontend_kind{FrontendKind::GenericAskpass};
  };
  using ResponseCallback = std::function<void(ResponseKind, const QString&)>;

  explicit AuthenticationPromptModel(QObject* parent = nullptr);
  [[nodiscard]] QString requestMessage() const { return request_message_; }
  [[nodiscard]] QString requestReference() const { return request_reference_; }
  [[nodiscard]] QVariantMap requesterDetails() const { return requester_details_; }
  [[nodiscard]] QString currentPrompt() const { return current_prompt_; }
  [[nodiscard]] InputMode inputMode() const { return input_mode_; }
  IdentityListModel* identities() { return &identities_; }
  [[nodiscard]] FrontendKind frontendKind() const { return frontend_kind_; }
  [[nodiscard]] QVariantMap selectedAccount() const { return identities_.profile(selected_identity_); }
  bool updateIdentityProfile(const QString& request_token, const Identity& profile);
  [[nodiscard]] QString selectedIdentity() const { return selected_identity_; }
  QAbstractItemModel* messages() { return &messages_; }
  [[nodiscard]] LifecycleState lifecycleState() const { return lifecycle_state_; }
  [[nodiscard]] QString requestToken() const { return request_token_; }
  [[nodiscard]] qulonglong sessionGeneration() const { return generation_; }

  bool beginRequest(Request request, ResponseCallback callback);
  bool presentPrompt(const QString& prompt, InputMode mode, QList<AuthenticationMessage> messages = {});
  bool appendMessage(AuthenticationMessage message);
  bool markBusy();
  bool markRetryableError(QList<AuthenticationMessage> messages);
  bool complete();
  bool reset();
  void shutdown();

  Q_INVOKABLE void respond(const QString& value);
  Q_INVOKABLE void confirm(bool accepted);
  Q_INVOKABLE void acknowledge();
  Q_INVOKABLE void cancel();
  Q_INVOKABLE void selectIdentity(const QString& stable_id);
  Q_INVOKABLE void retry();

 signals:
  void contextChanged();
  void promptChanged();
  void selectedIdentityChanged();
  void lifecycleStateChanged();
  void requestTokenChanged();
  void sessionGenerationChanged();
  void clearSensitiveInput();
  void invalidOperation();

 private:
  [[nodiscard]] bool isActive() const;
  void setLifecycle(LifecycleState state);
  void deliver(ResponseKind kind, const QString& value = {});
  void clearRequest();

  QString request_message_;
  QString request_reference_;
  QVariantMap requester_details_;
  QString current_prompt_;
  FrontendKind frontend_kind_{FrontendKind::GenericAskpass};
  InputMode input_mode_{InputMode::None};
  IdentityListModel identities_;
  QString selected_identity_;
  MessageListModel messages_;
  LifecycleState lifecycle_state_{LifecycleState::Idle};
  QString request_token_;
  qulonglong generation_{};
  ResponseCallback callback_;
  bool terminal_{};
};
}  // namespace Holonight::Authentication

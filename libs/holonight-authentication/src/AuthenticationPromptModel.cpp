#include "AuthenticationPromptModel.h"

#include "ExternalText.h"

namespace Holonight::Authentication {
AuthenticationPromptModel::AuthenticationPromptModel(QObject* parent)
    : QObject(parent), identities_(this), messages_(this) {}

bool AuthenticationPromptModel::beginRequest(Request request, ResponseCallback callback) {
  if (isActive() || request.token.isEmpty() || !callback) {
    return false;
  }
  if (lifecycle_state_ != LifecycleState::Idle) {
    clearRequest();
  }
  request_token_ = std::move(request.token);
  request_message_ = normalizeExternalText(request.message, kRequestMessageLimits);
  request_reference_ = normalizeExternalText(request.reference, kReferenceLimits);
  QMap<QString, QString> details;
  for (auto it = request.details.cbegin(); it != request.details.cend(); ++it) {
    details.insert(it.key(), it.value().toString());
  }
  requester_details_ = safeRequesterDetails(details);
  identities_.setItems(std::move(request.identities));
  selected_identity_ = identities_.contains(request.preferred_identity) ? request.preferred_identity : QString{};
  current_prompt_ = normalizeExternalText(request.prompt, kPromptLimits);
  input_mode_ = request.input_mode;
  frontend_kind_ = request.frontend_kind;
  callback_ = std::move(callback);
  terminal_ = false;
  ++generation_;
  emit clearSensitiveInput();
  emit requestTokenChanged();
  emit sessionGenerationChanged();
  emit contextChanged();
  emit promptChanged();
  emit selectedIdentityChanged();
  if (identities_.rowCount() > 1) {
    setLifecycle(LifecycleState::SelectingIdentity);
  } else {
    setLifecycle(LifecycleState::AwaitingInput);
  }
  return true;
}

bool AuthenticationPromptModel::updateIdentityProfile(const QString& request_token, const Identity& profile) {
  if (!isActive() || request_token_ != request_token || !identities_.updateProfile(profile)) {
    return false;
  }
  if (selected_identity_ == profile.stable_id) {
    emit selectedIdentityChanged();
  }
  return true;
}

bool AuthenticationPromptModel::presentPrompt(const QString& prompt, InputMode mode,
                                              QList<AuthenticationMessage> messages) {
  if (lifecycle_state_ != LifecycleState::Busy && lifecycle_state_ != LifecycleState::AwaitingInput) {
    return false;
  }
  emit clearSensitiveInput();
  current_prompt_ = normalizeExternalText(prompt, kPromptLimits);
  input_mode_ = mode;
  if (!messages.isEmpty()) {
    messages_.setItems(std::move(messages));
  }
  emit promptChanged();
  setLifecycle(LifecycleState::AwaitingInput);
  return true;
}
bool AuthenticationPromptModel::appendMessage(AuthenticationMessage message) {
  if (!isActive()) {
    return false;
  }
  message.text = normalizeExternalText(message.text, kPromptLimits);
  messages_.append(std::move(message));
  return true;
}
bool AuthenticationPromptModel::markBusy() {
  if (lifecycle_state_ != LifecycleState::AwaitingInput && lifecycle_state_ != LifecycleState::SelectingIdentity) {
    return false;
  }
  emit clearSensitiveInput();
  setLifecycle(LifecycleState::Busy);
  return true;
}
bool AuthenticationPromptModel::markRetryableError(QList<AuthenticationMessage> messages) {
  if (lifecycle_state_ != LifecycleState::Busy && lifecycle_state_ != LifecycleState::AwaitingInput) {
    return false;
  }
  emit clearSensitiveInput();
  messages_.setItems(std::move(messages));
  setLifecycle(LifecycleState::RetryableError);
  return true;
}
bool AuthenticationPromptModel::complete() {
  if (!isActive() || terminal_) {
    return false;
  }
  terminal_ = true;
  emit clearSensitiveInput();
  callback_ = {};
  setLifecycle(LifecycleState::Completed);
  return true;
}
bool AuthenticationPromptModel::reset() {
  if (isActive()) {
    return false;
  }
  clearRequest();
  setLifecycle(LifecycleState::Idle);
  return true;
}
void AuthenticationPromptModel::shutdown() {
  emit clearSensitiveInput();
  if (isActive()) {
    terminal_ = true;
    setLifecycle(LifecycleState::Cancelled);
    deliver(ResponseKind::Cancellation);
  }
}

void AuthenticationPromptModel::respond(const QString& value) {
  if (lifecycle_state_ != LifecycleState::AwaitingInput ||
      (input_mode_ != InputMode::Secret && input_mode_ != InputMode::Visible)) {
    emit invalidOperation();
    return;
  }
  markBusy();
  deliver(ResponseKind::Text, value);
}
void AuthenticationPromptModel::confirm(bool accepted) {
  if (lifecycle_state_ != LifecycleState::AwaitingInput || input_mode_ != InputMode::Confirmation) {
    emit invalidOperation();
    return;
  }
  markBusy();
  deliver(ResponseKind::Confirmation, accepted ? QStringLiteral("accepted") : QStringLiteral("rejected"));
}
void AuthenticationPromptModel::acknowledge() {
  if (lifecycle_state_ != LifecycleState::AwaitingInput || input_mode_ != InputMode::Notification) {
    emit invalidOperation();
    return;
  }
  markBusy();
  deliver(ResponseKind::Acknowledgement);
}
void AuthenticationPromptModel::cancel() {
  if (!isActive() || terminal_) {
    return;
  }
  terminal_ = true;
  emit clearSensitiveInput();
  setLifecycle(LifecycleState::Cancelled);
  deliver(ResponseKind::Cancellation);
}
void AuthenticationPromptModel::selectIdentity(const QString& stable_id) {
  if (lifecycle_state_ != LifecycleState::SelectingIdentity || !identities_.contains(stable_id)) {
    emit invalidOperation();
    return;
  }
  selected_identity_ = stable_id;
  emit selectedIdentityChanged();
  markBusy();
  deliver(ResponseKind::Identity, stable_id);
}
void AuthenticationPromptModel::retry() {
  if (lifecycle_state_ != LifecycleState::RetryableError) {
    emit invalidOperation();
    return;
  }
  emit clearSensitiveInput();
  messages_.setItems({});
  ++generation_;
  emit sessionGenerationChanged();
  setLifecycle(identities_.rowCount() > 1 ? LifecycleState::SelectingIdentity : LifecycleState::Busy);
  deliver(ResponseKind::Retry);
}
bool AuthenticationPromptModel::isActive() const {
  return lifecycle_state_ == LifecycleState::SelectingIdentity || lifecycle_state_ == LifecycleState::AwaitingInput ||
         lifecycle_state_ == LifecycleState::Busy || lifecycle_state_ == LifecycleState::RetryableError;
}
void AuthenticationPromptModel::setLifecycle(LifecycleState state) {
  if (lifecycle_state_ == state) {
    return;
  }
  lifecycle_state_ = state;
  emit lifecycleStateChanged();
}
void AuthenticationPromptModel::deliver(ResponseKind kind, const QString& value) {
  const auto callback = callback_;
  if (callback) {
    callback(kind, value);
  }
}
void AuthenticationPromptModel::clearRequest() {
  emit clearSensitiveInput();
  request_message_.clear();
  request_reference_.clear();
  requester_details_.clear();
  current_prompt_.clear();
  input_mode_ = InputMode::None;
  frontend_kind_ = FrontendKind::GenericAskpass;
  identities_.setItems({});
  messages_.setItems({});
  selected_identity_.clear();
  request_token_.clear();
  callback_ = {};
  terminal_ = false;
  emit contextChanged();
  emit promptChanged();
  emit selectedIdentityChanged();
  emit requestTokenChanged();
}
}  // namespace Holonight::Authentication

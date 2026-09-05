#include "PolkitRequestCoordinator.h"

#include <QPointer>
#include <QTimer>

namespace Holonight::Authentication {

PolkitRequestCoordinator::PolkitRequestCoordinator(AuthenticationPromptModel* model, uint current_uid,
                                                   SessionFactory session_factory, QObject* parent)
    : QObject(parent), model_(model), current_uid_(current_uid), session_factory_(std::move(session_factory)) {}

PolkitRequestCoordinator::~PolkitRequestCoordinator() { shutdown(); }

bool PolkitRequestCoordinator::enqueue(PolkitRequest request) {
  if (shutting_down_ || request.token.isEmpty() || !request.complete) {
    return false;
  }
  if ((active_ && active_->request.token == request.token)) {
    return false;
  }
  for (const auto& queued : queue_) {
    if (queued->request.token == request.token) {
      return false;
    }
  }
  queue_.push_back(std::make_unique<Record>(Record{.request = std::move(request)}));
  scheduleActivation();
  return true;
}

void PolkitRequestCoordinator::cancel(const QString& token) {
  if (active_ && active_->request.token == token) {
    auto session = std::move(session_);
    finishActive(false);
    if (session) {
      session->cancel();
    }
    return;
  }
  for (size_t index = 0; index < queue_.size(); ++index) {
    if (queue_.at(index)->request.token != token) {
      continue;
    }
    auto record = std::move(queue_[index]);
    queue_.erase(queue_.begin() + static_cast<ptrdiff_t>(index));
    if (!record->completed) {
      record->completed = true;
      record->request.complete(false);
    }
    return;
  }
}

void PolkitRequestCoordinator::shutdown() {
  if (shutting_down_) {
    return;
  }
  shutting_down_ = true;
  auto session = std::move(session_);
  if (active_) {
    finishActive(false);
  }
  if (session) {
    session->cancel();
  }
  while (!queue_.empty()) {
    auto record = std::move(queue_.front());
    queue_.pop_front();
    if (!record->completed) {
      record->request.complete(false);
    }
  }
  model_->shutdown();
}

QString PolkitRequestCoordinator::activeToken() const { return active_ ? active_->request.token : QString{}; }

void PolkitRequestCoordinator::scheduleActivation() {
  if (activation_scheduled_ || shutting_down_) {
    return;
  }
  activation_scheduled_ = true;
  QTimer::singleShot(0, this, [this] {
    activation_scheduled_ = false;
    activateNext();
  });
}

void PolkitRequestCoordinator::activateNext() {
  if (shutting_down_ || active_ || queue_.empty()) {
    return;
  }
  active_ = std::move(queue_.front());
  queue_.pop_front();
  if (active_->request.identities.isEmpty()) {
    emit requestRejected(QStringLiteral("empty-identities"));
    finishActive(false);
    return;
  }
  IdentityListModel policy;
  policy.setItems(active_->request.identities);
  active_->selected_identity = policy.preferred(current_uid_);
  const QString token = active_->request.token;
  const bool multiple = active_->request.identities.size() > 1;
  if (!model_->beginRequest({.token = token,
                             .message = active_->request.message,
                             .reference = active_->request.action_id,
                             .details = active_->request.details,
                             .identities = active_->request.identities,
                             .preferred_identity = active_->selected_identity,
                             .frontend_kind = AuthenticationPromptModel::FrontendKind::Polkit},
                            [this, token](auto kind, const QString& value) {
                              if (active_ && active_->request.token == token) {
                                handleResponse(kind, value);
                              }
                            })) {
    finishActive(false);
    return;
  }
  active_->generation = model_->sessionGeneration();
  emit requestPresented(token);
  if (!multiple) {
    startSession(active_->selected_identity);
  }
}

void PolkitRequestCoordinator::handleResponse(AuthenticationPromptModel::ResponseKind kind, const QString& value) {
  if (!active_) {
    return;
  }
  switch (kind) {
    case AuthenticationPromptModel::ResponseKind::Identity:
      startSession(value);
      break;
    case AuthenticationPromptModel::ResponseKind::Text:
      if (session_) {
        model_->markBusy();
        session_->respond(value);
      }
      break;
    case AuthenticationPromptModel::ResponseKind::Retry:
      active_->generation = model_->sessionGeneration();
      session_.reset();
      if (active_->request.identities.size() == 1) {
        startSession(active_->selected_identity);
      }
      break;
    case AuthenticationPromptModel::ResponseKind::Cancellation:
      cancel(active_->request.token);
      break;
    default:
      break;
  }
}

void PolkitRequestCoordinator::startSession(const QString& identity) {
  if (!active_) {
    return;
  }
  bool eligible = false;
  for (const auto& item : active_->request.identities) {
    if (item.stable_id == identity) {
      eligible = true;
      break;
    }
  }
  if (!eligible) {
    return;
  }
  active_->selected_identity = identity;
  const QString token = active_->request.token;
  const quint64 generation = active_->generation;
  const QPointer<PolkitRequestCoordinator> guard(this);
  PamSession::Callbacks callbacks{
      .prompt =
          [guard, token, generation](const QString& prompt, bool echo) {
            if (guard) {
              guard->sessionPrompt(token, generation, prompt, echo);
            }
          },
      .information =
          [guard, token, generation](QString text) {
            if (guard) {
              guard->sessionMessage(token, generation, std::move(text), MessageSeverity::Information);
            }
          },
      .error =
          [guard, token, generation](QString text) {
            if (guard) {
              guard->sessionMessage(token, generation, std::move(text), MessageSeverity::Error);
            }
          },
      .completed =
          [guard, token, generation](bool authorized) {
            if (guard) {
              guard->sessionCompleted(token, generation, authorized);
            }
          }};
  session_ = session_factory_(identity, active_->request.cookie, generation, std::move(callbacks));
  if (!session_) {
    finishActive(false);
    return;
  }
  model_->markBusy();
  session_->initiate();
}

void PolkitRequestCoordinator::finishActive(bool authorized) {
  if (!active_) {
    return;
  }
  auto record = std::move(active_);
  session_.reset();
  if (!record->completed) {
    record->completed = true;
    model_->complete();
    record->request.complete(authorized);
  }
  model_->reset();
  scheduleActivation();
}

bool PolkitRequestCoordinator::callbackIsCurrent(const QString& token, quint64 generation) const {
  return active_ && session_ && active_->request.token == token && active_->generation == generation;
}

void PolkitRequestCoordinator::sessionPrompt(const QString& token, quint64 generation, const QString& prompt,
                                             bool echo) {
  if (!callbackIsCurrent(token, generation)) {
    return;
  }
  model_->presentPrompt(
      prompt, echo ? AuthenticationPromptModel::InputMode::Visible : AuthenticationPromptModel::InputMode::Secret);
}

void PolkitRequestCoordinator::sessionMessage(const QString& token, quint64 generation, QString text,
                                              MessageSeverity severity) {
  if (!callbackIsCurrent(token, generation)) {
    return;
  }
  model_->appendMessage({.severity = severity, .text = std::move(text)});
}

void PolkitRequestCoordinator::sessionCompleted(const QString& token, quint64 generation, bool authorized) {
  if (!callbackIsCurrent(token, generation)) {
    return;
  }
  session_.reset();
  if (authorized) {
    finishActive(true);
    return;
  }
  model_->markRetryableError({{.severity = MessageSeverity::Error, .text = QStringLiteral("Authentication failed")}});
}

}  // namespace Holonight::Authentication

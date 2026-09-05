#pragma once

#include "AuthenticationPromptModel.h"

#include <QObject>

#include <deque>
#include <functional>
#include <memory>

namespace Holonight::Authentication {

class PamSession {
 public:
  struct Callbacks {
    std::function<void(QString, bool)> prompt;
    std::function<void(QString)> information;
    std::function<void(QString)> error;
    std::function<void(bool)> completed;
  };

  PamSession() = default;
  PamSession(const PamSession&) = delete;
  PamSession& operator=(const PamSession&) = delete;
  PamSession(PamSession&&) = delete;
  PamSession& operator=(PamSession&&) = delete;
  virtual ~PamSession() = default;
  virtual void initiate() = 0;
  virtual void respond(const QString& response) = 0;
  virtual void cancel() = 0;
};

struct PolkitRequest {
  QString token;
  QString action_id;
  QString message;
  QVariantMap details;
  QString cookie;
  QList<Identity> identities;
  std::function<void(bool)> complete;
};

class PolkitRequestCoordinator final : public QObject {
  Q_OBJECT

 public:
  using SessionFactory =
      std::function<std::unique_ptr<PamSession>(const QString&, const QString&, quint64, PamSession::Callbacks)>;

  PolkitRequestCoordinator(AuthenticationPromptModel* model, uint current_uid, SessionFactory session_factory,
                           QObject* parent = nullptr);
  ~PolkitRequestCoordinator() override;
  Q_DISABLE_COPY_MOVE(PolkitRequestCoordinator)

  bool enqueue(PolkitRequest request);
  void cancel(const QString& token);
  void shutdown();
  [[nodiscard]] QString activeToken() const;
  [[nodiscard]] qsizetype queuedCount() const { return static_cast<qsizetype>(queue_.size()); }

 signals:
  void requestPresented(const QString& token);
  void requestRejected(const QString& classification);

 private:
  struct Record {
    PolkitRequest request;
    QString selected_identity;
    quint64 generation{};
    bool completed{};
  };

  void scheduleActivation();
  void activateNext();
  void handleResponse(AuthenticationPromptModel::ResponseKind kind, const QString& value);
  void startSession(const QString& identity);
  void finishActive(bool authorized);
  [[nodiscard]] bool callbackIsCurrent(const QString& token, quint64 generation) const;
  void sessionPrompt(const QString& token, quint64 generation, const QString& prompt, bool echo);
  void sessionMessage(const QString& token, quint64 generation, QString text, MessageSeverity severity);
  void sessionCompleted(const QString& token, quint64 generation, bool authorized);

  AuthenticationPromptModel* model_{};
  uint current_uid_{};
  SessionFactory session_factory_;
  std::deque<std::unique_ptr<Record>> queue_;
  std::unique_ptr<Record> active_;
  std::unique_ptr<PamSession> session_;
  bool activation_scheduled_{};
  bool shutting_down_{};
};

}  // namespace Holonight::Authentication

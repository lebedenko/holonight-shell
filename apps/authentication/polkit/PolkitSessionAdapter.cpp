#include "PolkitSessionAdapter.h"

#include <QCoreApplication>

#include <polkitqt1-agent-session.h>

namespace Holonight::Authentication {
namespace {
class PolkitSessionAdapter final : public PamSession {
 public:
  PolkitSessionAdapter(const QString& identity, const QString& cookie, Callbacks callbacks)
      : callbacks_(std::move(callbacks)),
        session_(std::make_unique<PolkitQt1::Agent::Session>(PolkitQt1::Identity::fromString(identity), cookie)) {
    QObject::connect(session_.get(), &PolkitQt1::Agent::Session::request,
                     [this](const QString& prompt, bool echo) { callbacks_.prompt(prompt, echo); });
    QObject::connect(session_.get(), &PolkitQt1::Agent::Session::showInfo,
                     [this](const QString& text) { callbacks_.information(text); });
    QObject::connect(session_.get(), &PolkitQt1::Agent::Session::showError,
                     [this](const QString& text) { callbacks_.error(text); });
    QObject::connect(session_.get(), &PolkitQt1::Agent::Session::completed, [this](bool authorized) {
      const auto completed = callbacks_.completed;
      QMetaObject::invokeMethod(
          QCoreApplication::instance(), [completed, authorized] { completed(authorized); }, Qt::QueuedConnection);
    });
  }

  void initiate() override { session_->initiate(); }
  void respond(const QString& response) override { session_->setResponse(response); }
  void cancel() override { session_->cancel(); }

 private:
  Callbacks callbacks_;
  std::unique_ptr<PolkitQt1::Agent::Session> session_;
};
}  // namespace

std::unique_ptr<PamSession> createPolkitSession(const QString& identity, const QString& cookie, quint64 /*unused*/,
                                                PamSession::Callbacks callbacks) {
  const auto polkit_identity = PolkitQt1::Identity::fromString(identity);
  if (!polkit_identity.isValid()) {
    return {};
  }
  return std::make_unique<PolkitSessionAdapter>(identity, cookie, std::move(callbacks));
}
}  // namespace Holonight::Authentication

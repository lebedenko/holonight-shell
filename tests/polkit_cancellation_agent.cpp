#include "AuthenticationPromptModel.h"
#include "PolkitListenerBridge.h"
#include "PolkitRequestCoordinator.h"

#include <QCoreApplication>
#include <QTimer>

using namespace Holonight::Authentication;

namespace {
class Session final : public PamSession {
 public:
  explicit Session(Callbacks callbacks) : callbacks_(std::move(callbacks)) {}
  void initiate() override { callbacks_.prompt(QStringLiteral("Synthetic prompt"), false); }
  void respond(const QString& /*response*/) override {}
  void cancel() override { callbacks_.completed(false); }

 private:
  Callbacks callbacks_;
};
}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  AuthenticationPromptModel model;
  PamSession::Callbacks current;
  const bool failure = QCoreApplication::arguments().contains(QStringLiteral("failure"));
  PolkitRequestCoordinator coordinator(&model, 1000,
                                       [&](const QString&, const QString&, quint64, PamSession::Callbacks callbacks) {
                                         current = callbacks;
                                         return std::make_unique<Session>(std::move(callbacks));
                                       });
  QObject::connect(&coordinator, &PolkitRequestCoordinator::requestPresented, &model, [&] {
    QTimer::singleShot(0, &model, [&] {
      const auto& stale = current;
      if (failure) {
        stale.error(QStringLiteral("Synthetic failure"));
        stale.completed(false);
        if (model.lifecycleState() != AuthenticationPromptModel::LifecycleState::RetryableError) {
          QCoreApplication::exit(2);
          return;
        }
      }
      model.cancel();
      model.cancel();
      stale.completed(true);
      stale.prompt(QStringLiteral("Late prompt"), false);
    });
  });
  PolkitListenerBridge bridge(
      [&](const PolkitRequest& request) {
        if (!coordinator.enqueue(request)) {
          request.complete(false);
        }
      },
      [&](const QString& token) { coordinator.cancel(token); });
  QString error;
  if (!bridge.registerForSession("isolated-cancellation", &error)) {
    return 1;
  }
  return QCoreApplication::exec();
}

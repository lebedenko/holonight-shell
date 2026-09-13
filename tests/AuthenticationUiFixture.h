#pragma once

#include "AuthenticationPromptModel.h"
#include "PolkitRequestCoordinator.h"

#include <QDir>
#include <QObject>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTemporaryDir>

class AuthenticationImage : public QObject {
  Q_OBJECT
  Q_PROPERTY(int width READ width CONSTANT)
  Q_PROPERTY(int height READ height CONSTANT)
 public:
  AuthenticationImage(QImage image, QObject* parent) : QObject(parent), image_(std::move(image)) {}
  [[nodiscard]] int width() const { return image_.width(); }
  [[nodiscard]] int height() const { return image_.height(); }
  [[nodiscard]] Q_INVOKABLE QColor pixel(int column, int row) const { return image_.pixelColor(column, row); }
  [[nodiscard]] Q_INVOKABLE bool save(const QString& path) const { return image_.save(path); }

 private:
  QImage image_;
};

// Only the external PAM session is synthetic; QML observes the production models.
class AuthenticationUiFixture : public QObject {
  Q_OBJECT
  Q_PROPERTY(QObject* model READ model CONSTANT)

 public:
  explicit AuthenticationUiFixture(QObject* parent = nullptr) : QObject(parent) {}
  QObject* model() { return &model_; }

  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  [[nodiscard]] Q_INVOKABLE QString evidencePath(const QString& name) const {
    const QString directory = qEnvironmentVariable("UQC_AUTH_EVIDENCE_DIR");
    if (directory.isEmpty()) {
      return {};
    }
    QDir().mkpath(directory);
    return QDir(directory).filePath(qEnvironmentVariable("QT_QUICK_CONTROLS_STYLE") + QLatin1Char('-') +
                                    qEnvironmentVariable("QT_SCALE_FACTOR") + QLatin1Char('-') + name +
                                    QStringLiteral(".png"));
  }
  Q_INVOKABLE QObject* capture(QQuickWindow* window) { return new AuthenticationImage(window->grabWindow(), this); }
  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  Q_INVOKABLE bool shaderRendering(QQuickWindow* window) const {
    return window->rendererInterface()->graphicsApi() != QSGRendererInterface::Software;
  }
  Q_INVOKABLE QUrl localAvatar() {
    const QString path = assets_.filePath(QStringLiteral("avatar.png"));
    QImage image(56, 56, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(QStringLiteral("#c63e88")));
    if (!image.save(path)) {
      return {};
    }
    return QUrl::fromLocalFile(path);
  }
  Q_INVOKABLE bool beginPolkit(int count) {
    model_.cancel();
    QList<Holonight::Authentication::Identity> identities;
    for (int index = 0; index < count; ++index) {
      identities.append({.stable_id = QStringLiteral("account-%1").arg(index),
                         .display_label = QStringLiteral("Account %1").arg(index),
                         .uid = static_cast<uint>(1000 + index),
                         .has_uid = true,
                         .username = index == 2 ? QString{} : QStringLiteral("user-%1").arg(index),
                         .full_name = index == 0 ? QStringLiteral("<b>Full Name</b>") : QString{}});
    }
    return coordinator_.enqueue({.token = QStringLiteral("ui-%1").arg(++request_index_),
                                 .action_id = QStringLiteral("org.example.synthetic"),
                                 .message = QStringLiteral("Synthetic authentication request"),
                                 .cookie = QStringLiteral("test-only"),
                                 .identities = identities,
                                 .complete = [](bool) {}});
  }
  Q_INVOKABLE bool beginAskpass() {
    model_.cancel();
    return model_.beginRequest({.token = QStringLiteral("askpass-%1").arg(++request_index_),
                                .message = QStringLiteral("Synthetic Askpass request"),
                                .prompt = QStringLiteral("Password:"),
                                .input_mode = Model::InputMode::Secret,
                                .frontend_kind = Model::FrontendKind::SudoAskpass},
                               [](auto, const QString&) {});
  }
  Q_INVOKABLE bool updateProfile(const QString& stable_id, const QString& username, const QString& full_name,
                                 const QUrl& avatar) {
    return model_.updateIdentityProfile(
        model_.requestToken(),
        {.stable_id = stable_id, .username = username, .full_name = full_name, .avatar_url = avatar});
  }
  Q_INVOKABLE void prompt(bool echo) const { callbacks_.prompt(QStringLiteral("Password:"), echo); }
  Q_INVOKABLE void fail() const { callbacks_.completed(false); }

 private:
  using Model = Holonight::Authentication::AuthenticationPromptModel;
  using Session = Holonight::Authentication::PamSession;
  class SyntheticSession final : public Session {
   public:
    void initiate() override {}
    void respond(const QString& /*response*/) override {}
    void cancel() override {}
  };
  QTemporaryDir assets_;
  int request_index_{};
  Model model_;
  Session::Callbacks callbacks_;
  Holonight::Authentication::PolkitRequestCoordinator coordinator_{
      &model_, 1000, [this](const QString&, const QString&, quint64, Session::Callbacks callbacks) {
        callbacks_ = std::move(callbacks);
        return std::make_unique<SyntheticSession>();
      }};
};

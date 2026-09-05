#include "AccountProfileResolver.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFileInfo>
#include <QPointer>

#include <cerrno>
#include <pwd.h>
#include <vector>

namespace Holonight::Authentication {
namespace {
constexpr int kProfileTimeoutMs = 1500;
void queryAccountsService(uint uid, QObject* context, AccountProfileResolver::ProfileCallback callback) {
  auto find = QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.Accounts"), QStringLiteral("/org/freedesktop/Accounts"),
      QStringLiteral("org.freedesktop.Accounts"), QStringLiteral("FindUserById"));
  find << QVariant::fromValue(static_cast<qlonglong>(uid));
  auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(find, kProfileTimeoutMs), context);
  QObject::connect(
      watcher, &QDBusPendingCallWatcher::finished, context,
      [context, callback = std::move(callback)](QDBusPendingCallWatcher* result) {
        const QDBusPendingReply<QDBusObjectPath> reply = *result;
        result->deleteLater();
        if (reply.isError() || !reply.value().path().startsWith(QStringLiteral("/org/freedesktop/Accounts/User"))) {
          return;
        }
        auto get =
            QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.Accounts"), reply.value().path(),
                                           QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"));
        get << QStringLiteral("org.freedesktop.Accounts.User");
        auto* properties =
            new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(get, kProfileTimeoutMs), context);
        QObject::connect(properties, &QDBusPendingCallWatcher::finished, context,
                         [callback](QDBusPendingCallWatcher* result) {
                           const QDBusPendingReply<QVariantMap> reply = *result;
                           result->deleteLater();
                           if (!reply.isError()) {
                             callback(reply.value());
                           }
                         });
      });
}
}  // namespace

AccountProfileResolver::AccountProfileResolver(AuthenticationPromptModel* model, ProfileLookup lookup, QObject* parent)
    : QObject(parent), model_(model), lookup_(lookup ? std::move(lookup) : queryAccountsService) {
  connect(model_, &AuthenticationPromptModel::requestTokenChanged, this, [this] { request_scope_.reset(); });
  connect(model_, &AuthenticationPromptModel::lifecycleStateChanged, this, [this] {
    const auto state = model_->lifecycleState();
    if (state == AuthenticationPromptModel::LifecycleState::Completed ||
        state == AuthenticationPromptModel::LifecycleState::Cancelled ||
        state == AuthenticationPromptModel::LifecycleState::Idle) {
      request_scope_.reset();
    }
  });
}

QUrl AccountProfileResolver::localAvatar(const QString& path) {
  // AccountsService IconFile is a filesystem path, never a remote URL.
  const QFileInfo file(path);
  if (!file.isAbsolute() || !file.isFile() || !file.isReadable()) {
    return {};
  }
  return QUrl::fromLocalFile(file.canonicalFilePath());
}

Identity AccountProfileResolver::localProfile(Identity identity) {
  if (!identity.has_uid) {
    return identity;
  }
  passwd account{};
  passwd* result = nullptr;
  std::vector<char> buffer(4096);
  int status = 0;
  while ((status = getpwuid_r(identity.uid, &account, buffer.data(), buffer.size(), &result)) == ERANGE &&
         buffer.size() < 1024 * 1024) {
    buffer.resize(buffer.size() * 2);
  }
  if (status != 0 || result == nullptr) {
    return identity;
  }
  identity.username = QString::fromLocal8Bit(account.pw_name);
  identity.full_name = QString::fromLocal8Bit(account.pw_gecos).section(QLatin1Char(','), 0, 0).trimmed();
  const QString home = QString::fromLocal8Bit(account.pw_dir);
  identity.avatar_url = localAvatar(home + QStringLiteral("/.face"));
  if (identity.avatar_url.isEmpty()) {
    identity.avatar_url = localAvatar(home + QStringLiteral("/.face.icon"));
  }
  return identity;
}

Identity AccountProfileResolver::enrichedProfile(Identity identity, const QVariantMap& properties) {
  const auto username = properties.value(QStringLiteral("UserName")).toString().trimmed();
  const auto full_name = properties.value(QStringLiteral("RealName")).toString().trimmed();
  if (!username.isEmpty()) {
    identity.username = username;
  }
  if (!full_name.isEmpty()) {
    identity.full_name = full_name;
  }
  const auto avatar = localAvatar(properties.value(QStringLiteral("IconFile")).toString());
  if (!avatar.isEmpty()) {
    identity.avatar_url = avatar;
  }
  return identity;
}

void AccountProfileResolver::resolveCurrentRequest() {
  request_scope_ = std::make_unique<QObject>();
  const QPointer<QObject> scope(request_scope_.get());
  const QString token = model_->requestToken();
  const auto identities = model_->identities()->items();
  for (const auto& identity : identities) {
    if (!identity.has_uid) {
      continue;
    }
    const auto baseline = localProfile(identity);
    if (!model_->updateIdentityProfile(token, baseline)) {
      continue;
    }
    lookup_(identity.uid, scope, [this, scope, token, baseline](const QVariantMap& properties) {
      if (scope) {
        model_->updateIdentityProfile(token, enrichedProfile(baseline, properties));
      }
    });
  }
}
}  // namespace Holonight::Authentication

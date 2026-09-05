#include "AccountProfileResolver.h"
#include "AskpassMode.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <gtest/gtest.h>
#include <unistd.h>

using namespace Holonight::Authentication;

namespace {
Identity account() {
  return {.stable_id = QStringLiteral("unix-user:1000"),
          .display_label = QStringLiteral("Original label"),
          .uid = 1000,
          .has_uid = true,
          .username = QStringLiteral("local")};
}
void begin(AuthenticationPromptModel& model, QString token = QStringLiteral("request")) {
  ASSERT_TRUE(model.beginRequest({.token = std::move(token),
                                  .identities = {account()},
                                  .preferred_identity = account().stable_id,
                                  .input_mode = AuthenticationPromptModel::InputMode::Secret,
                                  .frontend_kind = AuthenticationPromptModel::FrontendKind::Polkit},
                                 [](auto, const QString&) {}));
}
}  // namespace

TEST(AuthenticationProfiles, ResolvesLocalUserAndPreservesIdentity) {
  Identity input{.stable_id = QStringLiteral("stable"),
                 .display_label = QStringLiteral("Existing label"),
                 .uid = static_cast<uint>(getuid()),
                 .has_uid = true};
  const auto profile = AccountProfileResolver::localProfile(input);
  EXPECT_FALSE(profile.username.isEmpty());
  EXPECT_EQ(profile.stable_id, input.stable_id);
  EXPECT_EQ(profile.display_label, input.display_label);
  input.has_uid = false;
  EXPECT_TRUE(AccountProfileResolver::localProfile(input).username.isEmpty());
}

TEST(AuthenticationProfiles, MissingNamesPreserveBaselineAndInvalidAvatarsAreIgnored) {
  auto baseline = account();
  baseline.full_name = QStringLiteral("Local Name");
  const auto profile = AccountProfileResolver::enrichedProfile(
      baseline, {{QStringLiteral("RealName"), "  "}, {QStringLiteral("IconFile"), "https://example.test/avatar"}});
  EXPECT_EQ(profile.full_name, baseline.full_name);
  EXPECT_EQ(profile.username, baseline.username);
  EXPECT_TRUE(profile.avatar_url.isEmpty());
  baseline.full_name.clear();
  EXPECT_TRUE(AccountProfileResolver::enrichedProfile(baseline, {}).full_name.isEmpty());
  EXPECT_TRUE(AccountProfileResolver::localAvatar(QStringLiteral("/missing/avatar.png")).isEmpty());
  EXPECT_TRUE(AccountProfileResolver::localAvatar(QStringLiteral("relative.png")).isEmpty());
  QTemporaryDir directory;
  EXPECT_TRUE(AccountProfileResolver::localAvatar(directory.path()).isEmpty());
  QFile avatar(directory.filePath(QStringLiteral("avatar.png")));
  ASSERT_TRUE(avatar.open(QIODevice::WriteOnly));
  avatar.write("invalid-image");
  avatar.close();
  // Image decoding failure is handled by the QML fallback; paths are validated here.
  EXPECT_EQ(AccountProfileResolver::localAvatar(avatar.fileName()), QUrl::fromLocalFile(avatar.fileName()));
  ASSERT_TRUE(avatar.setPermissions({}));
  EXPECT_TRUE(AccountProfileResolver::localAvatar(avatar.fileName()).isEmpty());
  ASSERT_TRUE(avatar.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));
}

TEST(AuthenticationProfiles, AsynchronousEnrichmentUpdatesRolesWithoutChangingSelectionOrPrompt) {
  AuthenticationPromptModel model;
  AccountProfileResolver::ProfileCallback complete;
  AccountProfileResolver resolver(&model, [&](uint, QObject*, auto callback) { complete = std::move(callback); });
  begin(model);
  resolver.resolveCurrentRequest();
  ASSERT_TRUE(complete);
  QSignalSpy roles(model.identities(), &QAbstractItemModel::dataChanged);
  complete({{QStringLiteral("RealName"), "Full Name"}, {QStringLiteral("UserName"), "username"}});
  EXPECT_EQ(model.selectedAccount().value("fullName"), QStringLiteral("Full Name"));
  EXPECT_EQ(model.identities()->data(model.identities()->index(0), IdentityListModel::UsernameRole), "username");
  EXPECT_EQ(model.selectedIdentity(), account().stable_id);
  EXPECT_EQ(model.identities()->data(model.identities()->index(0), IdentityListModel::DisplayLabelRole),
            account().display_label);
  EXPECT_EQ(model.lifecycleState(), AuthenticationPromptModel::LifecycleState::AwaitingInput);
  EXPECT_EQ(roles.count(), 1);
}

TEST(AuthenticationProfiles, IgnoresLateResultsAfterCompletionAndReplacementEvenWithReusedToken) {
  AuthenticationPromptModel model;
  QList<AccountProfileResolver::ProfileCallback> callbacks;
  AccountProfileResolver resolver(&model,
                                  [&](uint, QObject*, auto callback) { callbacks.append(std::move(callback)); });
  begin(model);
  resolver.resolveCurrentRequest();
  ASSERT_TRUE(model.complete());
  callbacks.first()({{QStringLiteral("RealName"), "Late name"}});
  EXPECT_NE(model.selectedAccount().value("fullName"), "Late name");
  begin(model);
  resolver.resolveCurrentRequest();
  callbacks.first()({{QStringLiteral("RealName"), "Stale name"}});
  EXPECT_NE(model.selectedAccount().value("fullName"), "Stale name");
  callbacks.last()({{QStringLiteral("RealName"), "Current name"}});
  EXPECT_EQ(model.selectedAccount().value("fullName"), "Current name");
  model.cancel();
  callbacks.last()({{QStringLiteral("RealName"), "Cancelled name"}});
  EXPECT_EQ(model.selectedAccount().value("fullName"), "Current name");
}

TEST(AuthenticationProfiles, ProfileUpdateRejectsUnknownRequestsAndRemoteAvatars) {
  AuthenticationPromptModel model;
  begin(model);
  auto profile = account();
  profile.avatar_url = QUrl(QStringLiteral("https://example.test/avatar.png"));
  EXPECT_FALSE(model.updateIdentityProfile(QStringLiteral("stale"), profile));
  EXPECT_TRUE(model.updateIdentityProfile(model.requestToken(), profile));
  EXPECT_TRUE(model.selectedAccount().value("avatarUrl").toUrl().isEmpty());
  profile.stable_id = QStringLiteral("unknown");
  EXPECT_FALSE(model.updateIdentityProfile(model.requestToken(), profile));
}

TEST(AuthenticationFrontend, DistinguishesAskpassHeadingsWithoutChangingInputMode) {
  EXPECT_EQ(askpassFrontendKind("holonight-sudo-askpass"), AuthenticationPromptModel::FrontendKind::SudoAskpass);
  EXPECT_EQ(askpassFrontendKind("holonight-ssh-askpass"), AuthenticationPromptModel::FrontendKind::SshAskpass);
  EXPECT_EQ(askpassFrontendKind("holonight-askpass"), AuthenticationPromptModel::FrontendKind::GenericAskpass);
  EXPECT_EQ(askpassMode("holonight-sudo-askpass", "confirm"), AuthenticationPromptModel::InputMode::Secret);
  EXPECT_EQ(askpassMode("holonight-ssh-askpass", "confirm"), AuthenticationPromptModel::InputMode::Confirmation);
}

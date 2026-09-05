#include "AskpassMode.h"
namespace Holonight::Authentication {
AuthenticationPromptModel::FrontendKind askpassFrontendKind(const QString& basename) {
  if (basename == QStringLiteral("holonight-sudo-askpass")) {
    return AuthenticationPromptModel::FrontendKind::SudoAskpass;
  }
  if (basename == QStringLiteral("holonight-ssh-askpass")) {
    return AuthenticationPromptModel::FrontendKind::SshAskpass;
  }
  return AuthenticationPromptModel::FrontendKind::GenericAskpass;
}
AuthenticationPromptModel::InputMode askpassMode(const QString& basename, const QString& hint) {
  if (basename == QStringLiteral("holonight-sudo-askpass")) {
    return AuthenticationPromptModel::InputMode::Secret;
  }
  if ((basename == QStringLiteral("holonight-ssh-askpass") || basename == QStringLiteral("holonight-askpass")) &&
      hint == QStringLiteral("confirm")) {
    return AuthenticationPromptModel::InputMode::Confirmation;
  }
  if ((basename == QStringLiteral("holonight-ssh-askpass") || basename == QStringLiteral("holonight-askpass")) &&
      hint == QStringLiteral("none")) {
    return AuthenticationPromptModel::InputMode::Notification;
  }
  return AuthenticationPromptModel::InputMode::Secret;
}
}  // namespace Holonight::Authentication

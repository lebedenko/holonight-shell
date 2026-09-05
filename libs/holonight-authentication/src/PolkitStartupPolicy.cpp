#include "PolkitStartupPolicy.h"

namespace Holonight::Authentication {
PolkitRegistrationOutcome classifyPolkitRegistration(bool registered, const QString& error_message) {
  if (registered) {
    return PolkitRegistrationOutcome::Success;
  }
  const QString normalized = error_message.toLower();
  if (normalized.contains(QStringLiteral("already exists")) ||
      normalized.contains(QStringLiteral("already registered"))) {
    return PolkitRegistrationOutcome::Conflict;
  }
  return PolkitRegistrationOutcome::Failure;
}
}  // namespace Holonight::Authentication

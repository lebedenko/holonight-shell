#pragma once

#include <QString>

namespace Holonight::Authentication {
enum class PolkitRegistrationOutcome : quint8 { Success, Conflict, Failure };
PolkitRegistrationOutcome classifyPolkitRegistration(bool registered, const QString& error_message);
}  // namespace Holonight::Authentication

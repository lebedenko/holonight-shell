#pragma once
#include <QByteArray>
#include <QString>

namespace Holonight::Authentication {
enum class SecretValidationError : quint8 { None, Empty, InvalidUnicode, ForbiddenCharacter, TooLong };
struct SecretValidationResult {
  QByteArray bytes;
  SecretValidationError error{SecretValidationError::None};
  explicit operator bool() const { return error == SecretValidationError::None; }
};
SecretValidationResult validateSecret(const QString& value);
}  // namespace Holonight::Authentication

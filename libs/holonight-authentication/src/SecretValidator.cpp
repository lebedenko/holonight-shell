#include "SecretValidator.h"
namespace Holonight::Authentication {
SecretValidationResult validateSecret(const QString& value) {
  if (value.isEmpty()) {
    return {.error = SecretValidationError::Empty};
  }
  if (value.contains('\r') || value.contains('\n') || value.contains(QChar::Null)) {
    return {.error = SecretValidationError::ForbiddenCharacter};
  }
  const QByteArray bytes = value.toUtf8();
  if (QString::fromUtf8(bytes) != value) {
    return {.error = SecretValidationError::InvalidUnicode};
  }
  if (bytes.size() > 1022) {
    return {.error = SecretValidationError::TooLong};
  }
  return {.bytes = bytes};
}
}  // namespace Holonight::Authentication

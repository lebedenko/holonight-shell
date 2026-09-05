#include "ProtocolWriter.h"

#include <cerrno>
#include <unistd.h>
namespace Holonight::Authentication {
bool writeProtocolSecret(int descriptor, const QByteArray& secret, const WriteFunction& supplied_writer) {
  const QByteArray payload = secret + '\n';
  const WriteFunction writer = supplied_writer ? supplied_writer : WriteFunction{::write};
  qsizetype offset = 0;
  while (offset < payload.size()) {
    const ssize_t count =
        writer(descriptor, QByteArrayView(payload).sliced(offset).data(), static_cast<size_t>(payload.size() - offset));
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count <= 0) {
      return false;
    }
    offset += count;
  }
  return true;
}
}  // namespace Holonight::Authentication

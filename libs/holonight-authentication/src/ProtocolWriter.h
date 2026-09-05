#pragma once
#include <QByteArray>

#include <functional>
#include <sys/types.h>
namespace Holonight::Authentication {
using WriteFunction = std::function<ssize_t(int, const void*, size_t)>;
bool writeProtocolSecret(int descriptor, const QByteArray& secret, const WriteFunction& writer = {});
}  // namespace Holonight::Authentication

#include "SwayIpc.h"

#include <cstring>
#include <utility>

namespace {
constexpr QByteArrayView kMagic{"i3-ipc"};
constexpr qsizetype kHeaderSize = 14;
}  // namespace

QByteArray encodeSwayIpcFrame(quint32 type, const QByteArray& payload) {
  QByteArray frame(kHeaderSize, Qt::Uninitialized);
  std::memcpy(frame.data(), kMagic.data(), kMagic.size());
  const quint32 length = static_cast<quint32>(payload.size());
  std::memcpy(frame.data() + 6, &length, sizeof(length));
  std::memcpy(frame.data() + 10, &type, sizeof(type));
  frame.append(payload);
  return frame;
}

bool SwayIpcDecoder::append(const QByteArray& bytes) {
  if (!error_.isEmpty()) return false;
  buffer_.append(bytes);
  while (buffer_.size() >= kHeaderSize) {
    if (QByteArrayView(buffer_).first(6) != kMagic) {
      error_ = QStringLiteral("invalid i3 IPC magic");
      return false;
    }
    quint32 length = 0;
    std::memcpy(&length, buffer_.constData() + 6, sizeof(length));
    if (length > kMaximumPayload) {
      error_ = QStringLiteral("i3 IPC payload exceeds 8 MiB");
      return false;
    }
    if (buffer_.size() < kHeaderSize + static_cast<qsizetype>(length)) break;
    quint32 type = 0;
    std::memcpy(&type, buffer_.constData() + 10, sizeof(type));
    frames_.append({.type = type, .payload = buffer_.mid(kHeaderSize, length)});
    buffer_.remove(0, kHeaderSize + length);
  }
  return true;
}

QList<SwayIpcFrame> SwayIpcDecoder::takeFrames() { return std::exchange(frames_, {}); }

QString escapeSwayWorkspaceName(const QString& name) {
  QString escaped = name;
  escaped.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
  escaped.replace(QStringLiteral("\""), QStringLiteral("\\\""));
  return escaped;
}

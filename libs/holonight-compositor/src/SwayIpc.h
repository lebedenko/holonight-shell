#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

struct SwayIpcFrame {
  quint32 type{0};
  QByteArray payload;
};

class SwayIpcDecoder {
 public:
  static constexpr quint32 kMaximumPayload = 8U * 1024U * 1024U;
  bool append(const QByteArray& bytes);
  [[nodiscard]] QList<SwayIpcFrame> takeFrames();
  [[nodiscard]] QString error() const { return error_; }

 private:
  QByteArray buffer_;
  QList<SwayIpcFrame> frames_;
  QString error_;
};

[[nodiscard]] QByteArray encodeSwayIpcFrame(quint32 type, const QByteArray& payload);
[[nodiscard]] QString escapeSwayWorkspaceName(const QString& name);

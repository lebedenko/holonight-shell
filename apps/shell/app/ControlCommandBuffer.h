#pragma once

#include <QByteArray>

#include <utility>

class ControlCommandBuffer {
 public:
  static constexpr qsizetype kMaxBytes = 4096;

  [[nodiscard]] bool append(const QByteArray& chunk) {
    if (chunk.size() > kMaxBytes - buffer_.size()) {
      return false;
    }
    buffer_.append(chunk);
    return true;
  }

  [[nodiscard]] QByteArray take() { return std::exchange(buffer_, {}); }

 private:
  QByteArray buffer_;
};

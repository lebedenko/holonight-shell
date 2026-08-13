#include "SwayIpc.h"

#include <cstring>
#include <gtest/gtest.h>

TEST(SwayIpc, DecodesPartialHeaderAndPayload) {
  const QByteArray encoded = encodeSwayIpcFrame(4, QByteArrayLiteral("{\"ok\":true}"));
  SwayIpcDecoder decoder;
  EXPECT_TRUE(decoder.append(encoded.first(5)));
  EXPECT_TRUE(decoder.takeFrames().isEmpty());
  EXPECT_TRUE(decoder.append(encoded.sliced(5, 10)));
  EXPECT_TRUE(decoder.takeFrames().isEmpty());
  EXPECT_TRUE(decoder.append(encoded.sliced(15)));
  const auto frames = decoder.takeFrames();
  ASSERT_EQ(frames.size(), 1);
  EXPECT_EQ(frames.front().type, 4U);
  EXPECT_EQ(frames.front().payload, QByteArrayLiteral("{\"ok\":true}"));
}

TEST(SwayIpc, RejectsInvalidMagicAndOversizedPayloadBeforeAllocation) {
  SwayIpcDecoder invalid;
  EXPECT_FALSE(invalid.append(QByteArrayLiteral("bad-ipc.......")));
  EXPECT_FALSE(invalid.error().isEmpty());

  QByteArray header = encodeSwayIpcFrame(1, {});
  const quint32 oversized_length = SwayIpcDecoder::kMaximumPayload + 1;
  std::memcpy(header.data() + 6, &oversized_length, sizeof(oversized_length));
  SwayIpcDecoder oversized;
  EXPECT_FALSE(oversized.append(header));
  EXPECT_TRUE(oversized.error().contains(QStringLiteral("8 MiB")));
}

TEST(SwayIpc, DecodesMultipleFramesWithoutLosingRemainder) {
  SwayIpcDecoder decoder;
  EXPECT_TRUE(decoder.append(encodeSwayIpcFrame(1, "a") + encodeSwayIpcFrame(2, "bc")));
  const auto frames = decoder.takeFrames();
  ASSERT_EQ(frames.size(), 2);
  EXPECT_EQ(frames.at(0).payload, QByteArrayLiteral("a"));
  EXPECT_EQ(frames.at(1).payload, QByteArrayLiteral("bc"));
}

TEST(SwayIpc, EscapesWorkspaceNamesForQuotedCommands) {
  EXPECT_EQ(escapeSwayWorkspaceName(QStringLiteral("dev \\\"quoted\\\" \\\\ path")),
            QStringLiteral("dev \\\\\\\"quoted\\\\\\\" \\\\\\\\ path"));
}

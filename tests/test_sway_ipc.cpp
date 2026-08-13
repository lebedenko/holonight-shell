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

TEST(SwayIpc, ParsesNamedWorkspacesFloatingWindowsAndFocusedOutput) {
  const auto snapshot = parseSwaySnapshot(
      R"([{"num":-1,"name":"dev:web","visible":true,"focused":true,"urgent":true,"output":"DP-1"},{"num":2,"name":"2:chat","visible":false,"focused":false,"urgent":false,"output":"HDMI-A-1"}])",
      R"([{"name":"DP-1","focused":true},{"name":"HDMI-A-1","focused":false}])",
      R"({"type":"root","nodes":[{"type":"output","name":"DP-1","nodes":[{"type":"workspace","name":"dev:web","nodes":[],"floating_nodes":[{"type":"con","name":"Editor","app_id":"code","focused":true,"nodes":[],"floating_nodes":[]}]}]},{"type":"output","name":"__i3","nodes":[{"type":"workspace","name":"__i3_scratch","nodes":[{"type":"con","name":"ignored","focused":false,"nodes":[],"floating_nodes":[]}],"floating_nodes":[]}]}],"floating_nodes":[]})");
  ASSERT_TRUE(snapshot.has_value());
  ASSERT_EQ(snapshot->workspaces.size(), 2);
  EXPECT_EQ(snapshot->focused_output, QStringLiteral("DP-1"));
  EXPECT_EQ(snapshot->workspaces.at(0).id, QStringLiteral("dev:web"));
  EXPECT_FALSE(snapshot->workspaces.at(0).numeric_slot.has_value());
  EXPECT_TRUE(snapshot->workspaces.at(0).occupied.value());
  ASSERT_TRUE(snapshot->workspaces.at(1).numeric_slot.has_value());
  EXPECT_EQ(*snapshot->workspaces.at(1).numeric_slot, 2);
  EXPECT_EQ(snapshot->active_windows.value(QStringLiteral("DP-1")).app_id, QStringLiteral("code"));
}

TEST(SwayIpc, RejectsIncompleteRefreshDocuments) {
  EXPECT_FALSE(parseSwaySnapshot("[]", "{}", "{}").has_value());
  EXPECT_FALSE(parseSwaySnapshot("not json", "[]", "{}").has_value());
}

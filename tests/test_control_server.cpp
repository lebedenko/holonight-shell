#include "ControlCommandBuffer.h"
#include "ControlServer.h"

#include <gtest/gtest.h>

TEST(ControlServer, DecodesToggleLauncherCommand) {
  const ControlServer::DecodedCommand decoded = ControlServer::decodeCommand(QByteArrayLiteral(" toggle-launcher\n"));

  EXPECT_EQ(decoded.type, ControlServer::CommandType::ToggleLauncher);
  EXPECT_TRUE(decoded.argument.isEmpty());
}

TEST(ControlServer, DecodesSidebarToggleCommand) {
  const ControlServer::DecodedCommand decoded = ControlServer::decodeCommand(QByteArrayLiteral("sidebar:toggle:DP-3"));

  EXPECT_EQ(decoded.type, ControlServer::CommandType::ToggleSidebar);
  EXPECT_EQ(decoded.argument, QStringLiteral("DP-3"));
}

TEST(ControlServer, DecodesChatToggleCommand) {
  const ControlServer::DecodedCommand decoded = ControlServer::decodeCommand(QByteArrayLiteral("chat:toggle:DP-5"));

  EXPECT_EQ(decoded.type, ControlServer::CommandType::ToggleChat);
  EXPECT_EQ(decoded.argument, QStringLiteral("DP-5"));
}

TEST(ControlServer, DecodesChatToggleWithDefaultMonitor) {
  const ControlServer::DecodedCommand decoded = ControlServer::decodeCommand(QByteArrayLiteral("chat:toggle:"));

  EXPECT_EQ(decoded.type, ControlServer::CommandType::ToggleChat);
  EXPECT_TRUE(decoded.argument.isEmpty());
}

TEST(ControlServer, IgnoresUnknownCommand) {
  const ControlServer::DecodedCommand decoded = ControlServer::decodeCommand(QByteArrayLiteral("sidebar:open:DP-3"));

  EXPECT_EQ(decoded.type, ControlServer::CommandType::Unknown);
  EXPECT_TRUE(decoded.argument.isEmpty());
}

TEST(ControlCommandBuffer, AccumulatesFragmentedCommand) {
  ControlCommandBuffer buffer;

  EXPECT_TRUE(buffer.append("toggle-"));
  EXPECT_TRUE(buffer.append("launcher"));

  const ControlServer::DecodedCommand decoded = ControlServer::decodeCommand(buffer.take());
  EXPECT_EQ(decoded.type, ControlServer::CommandType::ToggleLauncher);
}

TEST(ControlCommandBuffer, RejectsOversizedCommand) {
  ControlCommandBuffer buffer;

  EXPECT_FALSE(buffer.append(QByteArray(ControlCommandBuffer::kMaxBytes + 1, 'x')));
  EXPECT_TRUE(buffer.take().isEmpty());
}

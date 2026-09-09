#include "QuickControlsRuntime.h"

#include <QByteArray>
#include <QGuiApplication>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

int main(int argc, char* argv[]) {
  qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
  QGuiApplication app(argc, argv);
  holonight::configureQuickControls();
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}

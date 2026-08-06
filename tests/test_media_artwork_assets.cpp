#include <QImage>
#include <QImageReader>
#include <QString>

#include <gtest/gtest.h>

namespace {

[[nodiscard]] QImage loadArtworkAsset(const QString& file_name) {
  QImageReader reader{QStringLiteral(TEST_SOURCE_DIR "/assets/media/") + file_name};
  reader.setBackgroundColor(Qt::transparent);
  return reader.read();
}

}  // namespace

TEST(MediaArtworkAssets, FallbackKeepsTransparentCornersAndOpaqueInterior) {
  const QImage fallback = loadArtworkAsset(QStringLiteral("artwork-fallback.svg"));

  ASSERT_FALSE(fallback.isNull());
  EXPECT_EQ(fallback.pixelColor(0, 0).alpha(), 0);
  EXPECT_EQ(fallback.pixelColor(fallback.width() / 2, fallback.height() / 2).alpha(), 255);
}

TEST(MediaArtworkAssets, FrameKeepsTransparentCornersAndCenter) {
  const QImage frame = loadArtworkAsset(QStringLiteral("artwork-frame.svg"));

  ASSERT_FALSE(frame.isNull());
  EXPECT_EQ(frame.pixelColor(0, 0).alpha(), 0);
  EXPECT_EQ(frame.pixelColor(frame.width() / 2, frame.height() / 2).alpha(), 0);
}

#include "TrayItem.h"

#include <QColor>
#include <QDBusMetaType>
#include <QMetaType>

#include <gtest/gtest.h>

namespace {

QByteArray argbBytes(const QList<QRgb>& pixels) {
  QByteArray data;
  data.reserve(static_cast<qsizetype>(pixels.size() * 4));
  for (QRgb pixel : pixels) {
    data.append(static_cast<char>(qAlpha(pixel)));
    data.append(static_cast<char>(qRed(pixel)));
    data.append(static_cast<char>(qGreen(pixel)));
    data.append(static_cast<char>(qBlue(pixel)));
  }
  return data;
}

QByteArray argbBytes(std::initializer_list<QRgb> pixels) { return argbBytes(QList<QRgb>(pixels)); }

SniIconPixel pixmap(int width, int height, QRgb color) {
  QList<QRgb> pixels;
  pixels.reserve(width * height);
  for (int i = 0; i < width * height; ++i) {
    pixels.append(color);
  }
  return SniIconPixel{.width = width, .height = height, .data = argbBytes(pixels)};
}

}  // namespace

TEST(TrayItem, DecodePixmapListReturnsNullForEmptyList) {
  const QImage decoded = decodePixmapList({});

  EXPECT_TRUE(decoded.isNull());
}

TEST(TrayItem, DecodePixmapListReturnsNullForInvalidDimensions) {
  const QImage decoded = decodePixmapList({SniIconPixel{.width = 0, .height = 22, .data = QByteArray(88, '\0')}});

  EXPECT_TRUE(decoded.isNull());
}

TEST(TrayItem, DecodePixmapListReturnsNullForShortPixelData) {
  const QImage decoded = decodePixmapList({SniIconPixel{.width = 2, .height = 2, .data = QByteArray(15, '\0')}});

  EXPECT_TRUE(decoded.isNull());
}

TEST(TrayItem, DecodePixmapListDecodesExactSizeBigEndianArgbPixels) {
  const QRgb first = qRgba(0x11, 0x22, 0x33, 0x44);
  const QRgb second = qRgba(0xaa, 0xbb, 0xcc, 0xdd);
  const QImage decoded =
      decodePixmapList({SniIconPixel{.width = 2, .height = 1, .data = argbBytes({first, second})}}, 2);

  ASSERT_FALSE(decoded.isNull());
  EXPECT_EQ(decoded.size(), QSize(2, 1));
  EXPECT_EQ(decoded.pixel(0, 0), first);
  EXPECT_EQ(decoded.pixel(1, 0), second);
}

TEST(TrayItem, DecodePixmapListPreservesRowMajorPixelOrder) {
  const QRgb top_left = qRgba(0x10, 0x20, 0x30, 0x40);
  const QRgb top_right = qRgba(0x50, 0x60, 0x70, 0x80);
  const QRgb bottom_left = qRgba(0x90, 0xa0, 0xb0, 0xc0);
  const QRgb bottom_right = qRgba(0xd0, 0xe0, 0xf0, 0xff);
  const QImage decoded = decodePixmapList(
      {SniIconPixel{.width = 2, .height = 2, .data = argbBytes({top_left, top_right, bottom_left, bottom_right})}}, 2);

  ASSERT_FALSE(decoded.isNull());
  EXPECT_EQ(decoded.pixel(0, 0), top_left);
  EXPECT_EQ(decoded.pixel(1, 0), top_right);
  EXPECT_EQ(decoded.pixel(0, 1), bottom_left);
  EXPECT_EQ(decoded.pixel(1, 1), bottom_right);
}

TEST(TrayItem, DecodePixmapListScalesSelectedPixmapToTargetSize) {
  const QImage decoded = decodePixmapList({pixmap(2, 2, qRgba(10, 20, 30, 255))}, 4);

  ASSERT_FALSE(decoded.isNull());
  EXPECT_EQ(decoded.size(), QSize(4, 4));
  EXPECT_EQ(decoded.pixelColor(0, 0), QColor(10, 20, 30, 255));
}

TEST(TrayItem, DecodePixmapListPrefersClosestLargerCandidateOnTie) {
  const QRgb small_color = qRgba(1, 2, 3, 255);
  const QRgb large_color = qRgba(20, 30, 40, 255);
  const QImage decoded = decodePixmapList({pixmap(16, 16, small_color), pixmap(28, 28, large_color)}, 22);

  ASSERT_FALSE(decoded.isNull());
  EXPECT_EQ(decoded.size(), QSize(22, 22));
  EXPECT_EQ(decoded.pixelColor(0, 0), QColor(20, 30, 40, 255));
}

TEST(TrayItem, RegisterTrayMetaTypesSmoke) {
  registerTrayMetaTypes();

  EXPECT_NE(qMetaTypeId<SniIconPixel>(), QMetaType::UnknownType);
  EXPECT_NE(qMetaTypeId<SniIconPixmapList>(), QMetaType::UnknownType);
}

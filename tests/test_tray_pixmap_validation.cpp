#include "TrayItem.h"

#include <gtest/gtest.h>

TEST(TrayPixmapValidation, AcceptsValid32x32) {
  EXPECT_EQ(validateTrayPixmapDimensions(32, 32, 32 * 32 * 4), PixmapRejectReason::None);
}

TEST(TrayPixmapValidation, AcceptsValid512x512AtMaxBound) {
  EXPECT_EQ(validateTrayPixmapDimensions(kMaxTrayPixmapDim, kMaxTrayPixmapDim,
                                         static_cast<qsizetype>(kMaxTrayPixmapDim) * kMaxTrayPixmapDim * 4),
            PixmapRejectReason::None);
}

TEST(TrayPixmapValidation, RejectsWidthOverLimit) {
  EXPECT_EQ(validateTrayPixmapDimensions(kMaxTrayPixmapDim + 1, 32, 0), PixmapRejectReason::DimensionTooLarge);
}

TEST(TrayPixmapValidation, RejectsHeightOverLimit) {
  EXPECT_EQ(validateTrayPixmapDimensions(32, kMaxTrayPixmapDim + 1, 0), PixmapRejectReason::DimensionTooLarge);
}

TEST(TrayPixmapValidation, RejectsWidthNearIntMaxWithoutOverflow) {
  // This is the original CVE-class bug: width * height * 4 overflowing a 32-bit int
  // must not wrap around into a value that looks "in bounds".
  EXPECT_EQ(validateTrayPixmapDimensions(2147483647, 1, 4), PixmapRejectReason::DimensionTooLarge);
}

TEST(TrayPixmapValidation, RejectsDataLengthMismatch) {
  EXPECT_EQ(validateTrayPixmapDimensions(2, 2, 15), PixmapRejectReason::DataLengthMismatch);
  EXPECT_EQ(validateTrayPixmapDimensions(2, 2, 17), PixmapRejectReason::DataLengthMismatch);
}

TEST(TrayPixmapValidation, RejectsNonPositiveDimensions) {
  EXPECT_EQ(validateTrayPixmapDimensions(0, 22, 0), PixmapRejectReason::NonPositiveDimensions);
  EXPECT_EQ(validateTrayPixmapDimensions(22, 0, 0), PixmapRejectReason::NonPositiveDimensions);
  EXPECT_EQ(validateTrayPixmapDimensions(-1, 22, 0), PixmapRejectReason::NonPositiveDimensions);
  EXPECT_EQ(validateTrayPixmapDimensions(22, -1, 0), PixmapRejectReason::NonPositiveDimensions);
}

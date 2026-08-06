#include "SysfsValueReader.h"

#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace {

QString writeValueFile(QTemporaryDir& directory, const QString& contents) {
  const QString path = directory.filePath(QStringLiteral("value"));
  QFile file(path);
  EXPECT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
  EXPECT_EQ(file.write(contents.toUtf8()), contents.toUtf8().size());
  return path;
}

}  // namespace

TEST(SysfsValueReaderTest, ReadsValidInteger) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());

  const auto value = readSysfsInteger(writeValueFile(directory, QStringLiteral("4437\n")));

  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, 4437);
}

TEST(SysfsValueReaderTest, RejectsMalformedInteger) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());

  EXPECT_FALSE(readSysfsInteger(writeValueFile(directory, QStringLiteral("not-a-number\n"))).has_value());
}

TEST(SysfsValueReaderTest, RejectsUnreadableFile) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());

  EXPECT_FALSE(readSysfsInteger(directory.filePath(QStringLiteral("missing"))).has_value());
}

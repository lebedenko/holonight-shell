#include "SystemInfo.h"

#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace {

QString touchFile(const QTemporaryDir& dir, const QString& name) {
  QFile file(dir.filePath(name));
  EXPECT_TRUE(file.open(QIODevice::WriteOnly));
  file.close();
  return file.fileName();
}

}  // namespace

TEST(SystemInfo, ParsesQuotedOsReleaseValues) {
  const QHash<QString, QString> values =
      parseOsRelease("NAME=\"Arch Linux\"\nID=arch\nLOGO=archlinux-logo\n# ignored\n");

  EXPECT_EQ(values.value(QStringLiteral("NAME")), QStringLiteral("Arch Linux"));
  EXPECT_EQ(values.value(QStringLiteral("ID")), QStringLiteral("arch"));
  EXPECT_EQ(values.value(QStringLiteral("LOGO")), QStringLiteral("archlinux-logo"));
}

TEST(SystemInfo, ParsesEscapedAndPartiallyQuotedOsReleaseValues) {
  const QHash<QString, QString> values = parseOsRelease(
      "NAME=\"Quote \\\"Linux\\\"\"\n"
      "ESCAPED=\"line\\nnext\\tcol\\\\path\\$dollar\\`tick\"\n"
      "SINGLE='single quoted value'\n"
      "PARTIAL=\"unterminated\n");

  EXPECT_EQ(values.value(QStringLiteral("NAME")), QStringLiteral("Quote \"Linux\""));
  EXPECT_EQ(values.value(QStringLiteral("ESCAPED")), QStringLiteral("line\nnext\tcol\\path$dollar`tick"));
  EXPECT_EQ(values.value(QStringLiteral("SINGLE")), QStringLiteral("single quoted value"));
  EXPECT_EQ(values.value(QStringLiteral("PARTIAL")), QStringLiteral("\"unterminated"));
}

TEST(SystemInfo, IgnoresCommentsBlankMalformedLinesAndUsesLastDuplicateKey) {
  const QHash<QString, QString> values = parseOsRelease(
      "\n"
      "  # ignored comment\n"
      "MALFORMED\n"
      "=missing-key\n"
      "NAME=First\n"
      "NAME=Second\n"
      "ID = spaced-id\n");

  EXPECT_FALSE(values.contains(QStringLiteral("MALFORMED")));
  EXPECT_FALSE(values.contains(QString{}));
  EXPECT_EQ(values.value(QStringLiteral("NAME")), QStringLiteral("Second"));
  EXPECT_EQ(values.value(QStringLiteral("ID")), QStringLiteral("spaced-id"));
}

TEST(SystemInfo, PrefersPrettyNameAndLogoField) {
  const SystemInfoSnapshot snapshot = systemInfoFromOsRelease(
      parseOsRelease("NAME=Fedora\nPRETTY_NAME=\"Fedora Linux 40\"\nID=fedora\nLOGO=fedora-logo-icon\n"));

  EXPECT_EQ(snapshot.name, QStringLiteral("Fedora Linux 40"));
  EXPECT_EQ(snapshot.display_name, QStringLiteral("Fedora"));
  EXPECT_EQ(snapshot.logo_icon_name, QStringLiteral("fedora-logo-icon"));
}

TEST(SystemInfo, DerivesLogoFromIdWhenLogoIsMissing) {
  const SystemInfoSnapshot snapshot = systemInfoFromOsRelease(parseOsRelease("NAME=Debian\nID=debian\n"));

  EXPECT_EQ(snapshot.name, QStringLiteral("Debian"));
  EXPECT_EQ(snapshot.display_name, QStringLiteral("Debian"));
  EXPECT_EQ(snapshot.logo_icon_name, QStringLiteral("debian-logo"));
}

TEST(SystemInfo, ProvidesFallbacksForMissingOsRelease) {
  const SystemInfoSnapshot snapshot = systemInfoFromOsRelease({});

  EXPECT_EQ(snapshot.name, QStringLiteral("Linux"));
  EXPECT_EQ(snapshot.display_name, QStringLiteral("Linux"));
  EXPECT_EQ(snapshot.logo_icon_name, QStringLiteral("computer-symbolic"));
}

TEST(SystemInfo, ShortensCommonLinuxDisplayNameSuffixesAndPrefixes) {
  EXPECT_EQ(systemInfoFromOsRelease(parseOsRelease("NAME=\"Arch GNU/Linux\"\nID=arch\n")).display_name,
            QStringLiteral("Arch"));
  EXPECT_EQ(systemInfoFromOsRelease(parseOsRelease("NAME=\"Fedora Linux\"\nID=fedora\n")).display_name,
            QStringLiteral("Fedora"));
  EXPECT_EQ(systemInfoFromOsRelease(parseOsRelease("NAME=\"Linux Mint\"\nID=mint\n")).display_name,
            QStringLiteral("Mint"));
}

TEST(SystemInfo, FindsExactLogoFileInPixmapDirectory) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = touchFile(dir, QStringLiteral("archlinux-logo.svg"));

  const QString logo_path =
      findSystemLogoPath(parseOsRelease("NAME=\"Arch Linux\"\nID=arch\nLOGO=archlinux-logo\n"), {dir.path()});

  EXPECT_EQ(logo_path, path);
}

TEST(SystemInfo, PrefersSvgThenPngThenXpmForExactLogoFile) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString png_path = touchFile(dir, QStringLiteral("fedora-logo.png"));
  const QString svg_path = touchFile(dir, QStringLiteral("fedora-logo.svg"));
  const QString xpm_path = touchFile(dir, QStringLiteral("fedora-logo.xpm"));

  const QString svg_logo_path = findSystemLogoPath(parseOsRelease("ID=fedora\nLOGO=fedora-logo\n"), {dir.path()});
  EXPECT_EQ(svg_logo_path, svg_path);

  QFile::remove(svg_path);
  const QString png_logo_path = findSystemLogoPath(parseOsRelease("ID=fedora\nLOGO=fedora-logo\n"), {dir.path()});
  EXPECT_EQ(png_logo_path, png_path);

  QFile::remove(png_path);
  const QString xpm_logo_path = findSystemLogoPath(parseOsRelease("ID=fedora\nLOGO=fedora-logo\n"), {dir.path()});
  EXPECT_EQ(xpm_logo_path, xpm_path);
}

TEST(SystemInfo, PrefersPlainLogoOverTextVariant) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  touchFile(dir, QStringLiteral("archlinux-logo-text.svg"));
  const QString plain_logo = touchFile(dir, QStringLiteral("archlinux-logo.png"));

  const QString logo_path =
      findSystemLogoPath(parseOsRelease("NAME=\"Arch Linux\"\nID=arch\nLOGO=archlinux-logo\n"), {dir.path()});

  EXPECT_EQ(logo_path, plain_logo);
}

TEST(SystemInfo, IgnoresUnsupportedExtensionsAndMissingDirectories) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  touchFile(dir, QStringLiteral("archlinux-logo.ico"));
  touchFile(dir, QStringLiteral("archlinux-logo.txt"));

  const QString logo_path = findSystemLogoPath(parseOsRelease("NAME=\"Arch Linux\"\nID=arch\nLOGO=archlinux-logo\n"),
                                               {dir.filePath(QStringLiteral("missing")), dir.path()});

  EXPECT_TRUE(logo_path.isEmpty());
}

TEST(SystemInfo, MapsDistroIdToLogoNameForOneToOneMatches) {
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("arch")), QStringLiteral("archlinux"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("ubuntu")), QStringLiteral("ubuntu"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("debian")), QStringLiteral("debian"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("fedora")), QStringLiteral("fedora"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("centos")), QStringLiteral("centos"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("almalinux")), QStringLiteral("almalinux"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("gentoo")), QStringLiteral("gentoo"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("manjaro")), QStringLiteral("manjaro"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("endeavouros")), QStringLiteral("endeavouros"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("cachyos")), QStringLiteral("cachyos"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("linuxmint")), QStringLiteral("linuxmint"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("zorin")), QStringLiteral("zorin"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("solus")), QStringLiteral("solus"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("slackware")), QStringLiteral("slackware"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("deepin")), QStringLiteral("deepin"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("devuan")), QStringLiteral("devuan"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("elementary")), QStringLiteral("elementary"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("openwrt")), QStringLiteral("openwrt"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("tails")), QStringLiteral("tails"));
}

TEST(SystemInfo, MapsDistroIdToLogoNameForRenamedOrLongerIdAliases) {
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("opensuse-leap")), QStringLiteral("opensuse"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("opensuse-tumbleweed")), QStringLiteral("opensuse"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("sles")), QStringLiteral("opensuse"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("suse")), QStringLiteral("opensuse"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("fedora-asahi-remix")), QStringLiteral("asahilinux"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("rhel")), QStringLiteral("redhat"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("pop")), QStringLiteral("popos"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("mx")), QStringLiteral("mxlinux"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("neon")), QStringLiteral("kdeneon"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("void")), QStringLiteral("voidlinux"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("alpine")), QStringLiteral("alpinelinux"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("kali")), QStringLiteral("kalilinux"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("rocky")), QStringLiteral("rockylinux"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("garuda")), QStringLiteral("garudalinux"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("artix")), QStringLiteral("artixlinux"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("parrot")), QStringLiteral("parrotsecurity"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("qubes")), QStringLiteral("qubesos"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("nobara")), QStringLiteral("nobaralinux"));
}

TEST(SystemInfo, MapDistroIdToLogoNameIsCaseInsensitiveAndTrimmed) {
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("  Arch  ")), QStringLiteral("archlinux"));
  EXPECT_EQ(mapDistroIdToLogoName(QStringLiteral("FEDORA")), QStringLiteral("fedora"));
}

TEST(SystemInfo, MapDistroIdToLogoNameReturnsEmptyForUnmappedOrEmptyId) {
  EXPECT_TRUE(mapDistroIdToLogoName(QStringLiteral("")).isEmpty());
  EXPECT_TRUE(mapDistroIdToLogoName(QStringLiteral("some-unknown-distro")).isEmpty());
}

TEST(SystemInfo, FindsFallbackLogoCandidatesFromIdDisplayNameAndIdLike) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString display_name_path = touchFile(dir, QStringLiteral("nobara-logo.svg"));
  const QString id_like_path = touchFile(dir, QStringLiteral("fedora-logo.svg"));

  EXPECT_EQ(findSystemLogoPath(parseOsRelease("NAME=Nobara\nID=nobara\nID_LIKE=\"fedora rhel\"\n"), {dir.path()}),
            display_name_path);

  QFile::remove(display_name_path);

  EXPECT_EQ(findSystemLogoPath(parseOsRelease("NAME=Nobara\nID=nobara\nID_LIKE=\"fedora rhel\"\n"), {dir.path()}),
            id_like_path);
}

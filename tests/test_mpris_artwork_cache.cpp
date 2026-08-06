#include "mpris/MprisArtworkCache.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QSize>
#include <QString>
#include <QTemporaryDir>
#include <QUrl>

#include <gtest/gtest.h>
#include <memory>

namespace {

// Writes a real, decodable PNG at `dir`/`name` sized `size` and returns its absolute path.
QString writeTestImage(const QDir& dir, const QString& name, const QSize& size) {
  QImage image(size, QImage::Format_RGB32);
  image.fill(Qt::red);
  const QString path = dir.filePath(name);
  return image.save(path, "PNG") ? path : QString();
}

// Creates a sparse (near-instant, no real I/O for the payload) dummy cache file of `size_bytes`
// with its mtime backdated to `mtime` — used to seed MprisArtworkCache::entries_ via its
// construction-time directory scan for LRU eviction tests, without decoding real images.
QString makeDummyCacheFile(const QDir& dir, const QString& name, qint64 size_bytes, const QDateTime& mtime) {
  const QString path = dir.filePath(name);
  {
    QFile file(path);
    [[maybe_unused]] const bool opened = file.open(QIODevice::WriteOnly);
    file.resize(size_bytes);
  }
  QFile file(path);
  [[maybe_unused]] const bool reopened = file.open(QIODevice::ReadOnly);
  file.setFileTime(mtime, QFileDevice::FileModificationTime);
  return path;
}

// Pumps the event loop until `*done` flips true or `timeout_ms` elapses, returning elapsed ms —
// tighter and faster than a fixed QTest::qWait() for asserting sub-50ms cache-hit latency
// (REQ-NF-003) without over-waiting on every other test in the suite.
qint64 waitFor(const std::shared_ptr<bool>& done, int timeout_ms) {
  QElapsedTimer timer;
  timer.start();
  while (!*done && timer.elapsed() < timeout_ms) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
  }
  return timer.elapsed();
}

QString toFileUrl(const QString& local_path) { return QUrl::fromLocalFile(local_path).toString(); }

QString toDataUrl(const QByteArray& payload) {
  return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(payload.toBase64());
}

}  // namespace

// --- REQ-F-026: cache directory ---

TEST(MprisArtworkCacheInit, CreatesCacheDirectoryOnConstruction) {
  QTemporaryDir parent_dir;
  ASSERT_TRUE(parent_dir.isValid());
  const QString cache_root = parent_dir.filePath(QStringLiteral("mpris-artwork")) + QStringLiteral("/");

  ASSERT_FALSE(QDir(cache_root).exists());
  MprisArtworkCache cache(cache_root);
  EXPECT_TRUE(QDir(cache_root).exists());
}

// --- REQ-F-028/031/035/NF-003: resolve() happy path, keying, decode size, cache hits ---

TEST(MprisArtworkCacheResolve, FileUrlDecodesAtOrUnder512pxAndCaches) {
  QTemporaryDir cache_dir;
  QTemporaryDir source_dir;
  ASSERT_TRUE(cache_dir.isValid());
  ASSERT_TRUE(source_dir.isValid());

  MprisArtworkCache cache(cache_dir.path() + QStringLiteral("/"));
  const QString source_path = writeTestImage(QDir(source_dir.path()), QStringLiteral("large.png"), {1000, 800});
  ASSERT_FALSE(source_path.isEmpty());

  auto result = std::make_shared<QString>();
  auto done = std::make_shared<bool>(false);
  cache.resolve(toFileUrl(source_path), [result, done](QString path) {
    *result = std::move(path);
    *done = true;
  });

  ASSERT_LT(waitFor(done, 2000), 2000);
  ASSERT_FALSE(result->isEmpty());
  EXPECT_TRUE(QFileInfo::exists(*result));

  QImageReader reader(*result);
  const QSize decoded_size = reader.size();
  EXPECT_LE(decoded_size.width(), 512);
  EXPECT_LE(decoded_size.height(), 512);
  // Aspect ratio (5:4, source 1000x800) preserved, not forced square — allow ±1px for the
  // scaling algorithm's internal rounding.
  EXPECT_NE(decoded_size.width(), decoded_size.height());
  EXPECT_NEAR(static_cast<double>(decoded_size.width()) / decoded_size.height(), 1000.0 / 800.0, 0.01);
}

TEST(MprisArtworkCacheResolve, CacheHitReusesFileWithoutRefetchWithin50ms) {
  QTemporaryDir cache_dir;
  QTemporaryDir source_dir;
  ASSERT_TRUE(cache_dir.isValid());
  ASSERT_TRUE(source_dir.isValid());

  MprisArtworkCache cache(cache_dir.path() + QStringLiteral("/"));
  const QString source_path = writeTestImage(QDir(source_dir.path()), QStringLiteral("art.png"), {64, 64});
  ASSERT_FALSE(source_path.isEmpty());
  const QString art_url = toFileUrl(source_path);

  auto first_result = std::make_shared<QString>();
  auto first_done = std::make_shared<bool>(false);
  cache.resolve(art_url, [first_result, first_done](QString path) {
    *first_result = std::move(path);
    *first_done = true;
  });
  ASSERT_LT(waitFor(first_done, 2000), 2000);
  ASSERT_FALSE(first_result->isEmpty());

  // Remove the source: if the second resolve() re-fetched instead of hitting the cache, decoding
  // would fail and it would come back empty — proving a hit truly skips re-reading the source.
  ASSERT_TRUE(QFile::remove(source_path));

  auto second_result = std::make_shared<QString>();
  auto second_done = std::make_shared<bool>(false);
  const qint64 elapsed_ms = [&]() {
    QElapsedTimer timer;
    timer.start();
    cache.resolve(art_url, [second_result, second_done](QString path) {
      *second_result = std::move(path);
      *second_done = true;
    });
    while (!*second_done && timer.elapsed() < 500) {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    return timer.elapsed();
  }();

  ASSERT_TRUE(*second_done);
  EXPECT_EQ(*second_result, *first_result);
  EXPECT_LE(elapsed_ms, 50);  // REQ-NF-003
}

TEST(MprisArtworkCacheResolve, UnsupportedSchemeInvokesCallbackWithEmptyString) {
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  MprisArtworkCache cache(cache_dir.path() + QStringLiteral("/"));

  const QString url = QStringLiteral("custom://not-a-real-scheme");
  auto result = std::make_shared<QString>(QStringLiteral("unset"));
  auto done = std::make_shared<bool>(false);
  cache.resolve(url, [result, done](QString path) {
    *result = std::move(path);
    *done = true;
  });
  ASSERT_LT(waitFor(done, 500), 500);
  EXPECT_TRUE(result->isEmpty());
}

// --- REQ-F-027 (extended): inline base64 data: URIs, the common form for local-file players
// (Haruna, VLC) that embed cover art rather than exposing a file:// path ---

TEST(MprisArtworkCacheResolve, DataUrlWithValidBase64PngResolvesToLocalPath) {
  QTemporaryDir cache_dir;
  QTemporaryDir source_dir;
  ASSERT_TRUE(cache_dir.isValid());
  ASSERT_TRUE(source_dir.isValid());

  MprisArtworkCache cache(cache_dir.path() + QStringLiteral("/"));
  const QString source_path = writeTestImage(QDir(source_dir.path()), QStringLiteral("embedded.png"), {300, 300});
  ASSERT_FALSE(source_path.isEmpty());
  QFile source_file(source_path);
  ASSERT_TRUE(source_file.open(QIODevice::ReadOnly));
  const QString data_url = toDataUrl(source_file.readAll());

  auto result = std::make_shared<QString>();
  auto done = std::make_shared<bool>(false);
  cache.resolve(data_url, [result, done](QString path) {
    *result = std::move(path);
    *done = true;
  });

  ASSERT_LT(waitFor(done, 2000), 2000);
  ASSERT_FALSE(result->isEmpty());
  EXPECT_TRUE(QFileInfo::exists(*result));
  QImageReader reader(*result);
  EXPECT_FALSE(reader.read().isNull());
}

TEST(MprisArtworkCacheResolve, DataUrlWithoutBase64MarkerResolvesToEmptyString) {
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  MprisArtworkCache cache(cache_dir.path() + QStringLiteral("/"));

  const QString url = QStringLiteral("data:text/plain,hello");
  auto result = std::make_shared<QString>(QStringLiteral("unset"));
  auto done = std::make_shared<bool>(false);
  cache.resolve(url, [result, done](QString path) {
    *result = std::move(path);
    *done = true;
  });
  ASSERT_LT(waitFor(done, 500), 500);
  EXPECT_TRUE(result->isEmpty());
}

TEST(MprisArtworkCacheResolve, DataUrlWithInvalidImagePayloadResolvesToEmptyString) {
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  MprisArtworkCache cache(cache_dir.path() + QStringLiteral("/"));

  // "QUFB" base64-decodes to the ASCII bytes "AAA" — well-formed base64, but not a decodable image.
  const QString url = QStringLiteral("data:image/png;base64,QUFB");
  auto result = std::make_shared<QString>(QStringLiteral("unset"));
  auto done = std::make_shared<bool>(false);
  cache.resolve(url, [result, done](QString path) {
    *result = std::move(path);
    *done = true;
  });
  ASSERT_LT(waitFor(done, 500), 500);
  EXPECT_TRUE(result->isEmpty());
}

// --- REQ-F-057: corrupted source recovery ---

TEST(MprisArtworkCacheResolve, CorruptedSourceImageResolvesToEmptyString) {
  QTemporaryDir cache_dir;
  QTemporaryDir source_dir;
  ASSERT_TRUE(cache_dir.isValid());
  ASSERT_TRUE(source_dir.isValid());

  MprisArtworkCache cache(cache_dir.path() + QStringLiteral("/"));
  const QString source_path = QDir(source_dir.path()).filePath(QStringLiteral("corrupt.png"));
  QFile source_file(source_path);
  ASSERT_TRUE(source_file.open(QIODevice::WriteOnly));
  source_file.write("this is not a valid image file, just garbage bytes");
  source_file.close();

  auto result = std::make_shared<QString>(QStringLiteral("unset"));
  auto done = std::make_shared<bool>(false);
  cache.resolve(toFileUrl(source_path), [result, done](QString path) {
    *result = std::move(path);
    *done = true;
  });

  ASSERT_LT(waitFor(done, 2000), 2000);
  EXPECT_TRUE(result->isEmpty());
}

TEST(MprisArtworkCacheResolve, CorruptCacheHitIsRemovedAndRecoveredFromSource) {
  QTemporaryDir cache_dir;
  QTemporaryDir source_dir;
  ASSERT_TRUE(cache_dir.isValid());
  ASSERT_TRUE(source_dir.isValid());
  MprisArtworkCache cache(cache_dir.path() + QStringLiteral("/"));
  const QString source_path = writeTestImage(QDir(source_dir.path()), QStringLiteral("source.png"), {64, 64});
  const QString art_url = toFileUrl(source_path);
  auto first = std::make_shared<QString>();
  auto first_done = std::make_shared<bool>(false);
  cache.resolve(art_url, [first, first_done](QString path) {
    *first = std::move(path);
    *first_done = true;
  });
  ASSERT_LT(waitFor(first_done, 2000), 2000);
  QFile corrupt(*first);
  ASSERT_TRUE(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate));
  corrupt.write("broken");
  corrupt.close();

  auto recovered = std::make_shared<QString>();
  auto recovered_done = std::make_shared<bool>(false);
  cache.resolve(art_url, [recovered, recovered_done](QString path) {
    *recovered = std::move(path);
    *recovered_done = true;
  });

  ASSERT_LT(waitFor(recovered_done, 2000), 2000);
  EXPECT_EQ(*recovered, *first);
  EXPECT_TRUE(QImageReader(*recovered).canRead());
}

TEST(MprisArtworkCacheResolve, RejectsMalformedAndOversizedDataUrls) {
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  MprisArtworkCache cache(cache_dir.path() + QStringLiteral("/"));
  const QStringList urls{
      QStringLiteral("data:text/image;base64,QUFB"),
      QStringLiteral("data:image/png;base64,%%%"),
      QStringLiteral("data:image/png;charset=utf-8;base64,QUFB"),
      QStringLiteral("data:image/png;base64,") + QString(7 * 1024 * 1024, QLatin1Char('A')),
  };
  for (const QString& url : urls) {
    auto result = std::make_shared<QString>(QStringLiteral("unset"));
    auto done = std::make_shared<bool>(false);
    cache.resolve(url, [result, done](QString path) {
      *result = std::move(path);
      *done = true;
    });
    ASSERT_LT(waitFor(done, 2000), 2000);
    EXPECT_TRUE(result->isEmpty());
  }
}

// --- REQ-F-030: LRU eviction ---

TEST(MprisArtworkCacheEviction, DeletesOldestEntriesWhenOverBudget) {
  QTemporaryDir cache_dir;
  QTemporaryDir source_dir;
  ASSERT_TRUE(cache_dir.isValid());
  ASSERT_TRUE(source_dir.isValid());

  constexpr qint64 kOneMb = 1024 * 1024;
  const QDir dir(cache_dir.path());
  const QDateTime base = QDateTime::currentDateTime().addSecs(-3600);

  // 5 x 12 MB = 60 MB, already over the 50 MB budget by the time MprisArtworkCache scans the
  // directory at construction — but no eviction runs until the next write (DESIGN.md §5c).
  const QString oldest = makeDummyCacheFile(dir, QStringLiteral("aaa.png"), 12 * kOneMb, base.addSecs(0));
  const QString second = makeDummyCacheFile(dir, QStringLiteral("bbb.png"), 12 * kOneMb, base.addSecs(60));
  const QString third = makeDummyCacheFile(dir, QStringLiteral("ccc.png"), 12 * kOneMb, base.addSecs(120));
  const QString fourth = makeDummyCacheFile(dir, QStringLiteral("ddd.png"), 12 * kOneMb, base.addSecs(180));
  const QString fifth = makeDummyCacheFile(dir, QStringLiteral("eee.png"), 12 * kOneMb, base.addSecs(240));

  MprisArtworkCache cache(cache_dir.path() + QStringLiteral("/"));
  EXPECT_FALSE(QFileInfo::exists(oldest)) << "startup scan must enforce the budget immediately";
  const QString source_path = writeTestImage(QDir(source_dir.path()), QStringLiteral("small.png"), {8, 8});
  ASSERT_FALSE(source_path.isEmpty());

  auto result = std::make_shared<QString>();
  auto done = std::make_shared<bool>(false);
  cache.resolve(toFileUrl(source_path), [result, done](QString path) {
    *result = std::move(path);
    *done = true;
  });
  ASSERT_LT(waitFor(done, 2000), 2000);
  ASSERT_FALSE(result->isEmpty());

  EXPECT_FALSE(QFileInfo::exists(oldest)) << "oldest-mtime entry should have been evicted";
  EXPECT_TRUE(QFileInfo::exists(second));
  EXPECT_TRUE(QFileInfo::exists(third));
  EXPECT_TRUE(QFileInfo::exists(fourth));
  EXPECT_TRUE(QFileInfo::exists(fifth));
  EXPECT_TRUE(QFileInfo::exists(*result));

  qint64 total = 0;
  for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
    total += info.size();
  }
  EXPECT_LE(total, 50 * kOneMb);
}

// --- concurrent resolves for the same track_id ---

TEST(MprisArtworkCacheConcurrency, ConcurrentResolvesForSameTrackBothReceiveSameResult) {
  QTemporaryDir cache_dir;
  QTemporaryDir source_dir;
  ASSERT_TRUE(cache_dir.isValid());
  ASSERT_TRUE(source_dir.isValid());

  MprisArtworkCache cache(cache_dir.path() + QStringLiteral("/"));
  const QString source_path = writeTestImage(QDir(source_dir.path()), QStringLiteral("shared.png"), {300, 300});
  ASSERT_FALSE(source_path.isEmpty());
  const QString art_url = toFileUrl(source_path);

  auto call_count = std::make_shared<int>(0);
  auto first_result = std::make_shared<QString>(QStringLiteral("unset"));
  auto second_result = std::make_shared<QString>(QStringLiteral("unset"));
  auto both_done = std::make_shared<bool>(false);

  cache.resolve(art_url, [call_count, first_result, both_done](QString path) {
    ++(*call_count);
    *first_result = std::move(path);
    if (*call_count == 2) {
      *both_done = true;
    }
  });
  // Second call arrives while the first's file-read/decode is still in flight — must piggy-back
  // rather than starting a duplicate QtConcurrent decode task for the same track_id.
  cache.resolve(art_url, [call_count, second_result, both_done](QString path) {
    ++(*call_count);
    *second_result = std::move(path);
    if (*call_count == 2) {
      *both_done = true;
    }
  });

  ASSERT_LT(waitFor(both_done, 2000), 2000);
  EXPECT_EQ(*call_count, 2);
  EXPECT_FALSE(first_result->isEmpty());
  EXPECT_EQ(*first_result, *second_result);
}

TEST(MprisArtworkCacheConcurrency, DifferentUrlsNeverCoalesce) {
  QTemporaryDir cache_dir;
  QTemporaryDir source_dir;
  ASSERT_TRUE(cache_dir.isValid());
  ASSERT_TRUE(source_dir.isValid());
  MprisArtworkCache cache(cache_dir.path() + QStringLiteral("/"));
  const QString first_url = toFileUrl(writeTestImage(QDir(source_dir.path()), QStringLiteral("a.png"), {20, 20}));
  const QString second_url = toFileUrl(writeTestImage(QDir(source_dir.path()), QStringLiteral("b.png"), {30, 30}));
  auto results = std::make_shared<QStringList>();
  auto done = std::make_shared<bool>(false);
  const auto callback = [results, done](QString path) {
    results->append(std::move(path));
    *done = results->size() == 2;
  };
  cache.resolve(first_url, callback);
  cache.resolve(second_url, callback);

  ASSERT_LT(waitFor(done, 2000), 2000);
  ASSERT_EQ(results->size(), 2);
  EXPECT_NE(results->at(0), results->at(1));
}

TEST(MprisArtworkCacheConcurrency, SharedWorkCompletesForAllConsumers) {
  QTemporaryDir cache_dir;
  QTemporaryDir source_dir;
  ASSERT_TRUE(cache_dir.isValid());
  ASSERT_TRUE(source_dir.isValid());

  MprisArtworkCache cache(cache_dir.path() + QStringLiteral("/"));
  const QString source_path = writeTestImage(QDir(source_dir.path()), QStringLiteral("shared2.png"), {300, 300});
  ASSERT_FALSE(source_path.isEmpty());
  const QString art_url = toFileUrl(source_path);

  auto call_count = std::make_shared<int>(0);
  // Callers do not own shared work. Both callbacks remain valid until the URL-keyed decode
  // finishes, even if a widget would have become hidden in the meantime.
  cache.resolve(art_url, [call_count](const QString&) { ++(*call_count); });
  cache.resolve(art_url, [call_count](const QString&) { ++(*call_count); });
  QElapsedTimer timer;
  timer.start();
  while (*call_count < 2 && timer.elapsed() < 2000) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
  }
  EXPECT_EQ(*call_count, 2);
}

#include "MprisArtworkCache.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

Q_LOGGING_CATEGORY(lcMprisArtworkCache, "holonight.mpris.artwork")

namespace {
constexpr qint64 kMaxFileBytes = 5 * 1024 * 1024;       // REQ-F-029
constexpr qint64 kCacheBudgetBytes = 50 * 1024 * 1024;  // REQ-F-030
constexpr int kMaxDimension = 512;                      // REQ-F-031

QString computeCacheKey(const QString& url) {
  QCryptographicHash hash(QCryptographicHash::Sha256);
  hash.addData(url.toUtf8());
  return QString::fromLatin1(hash.result().toHex());
}

// Fits `original` inside a kMaxDimension x kMaxDimension box, preserving aspect ratio. Never
// upscales art that is already within budget (REQ-F-031 caps the maximum, it does not mandate
// enlarging small source images).
QSize scaledSizePreservingAspect(const QSize& original, int max_dimension) {
  if (!original.isValid() || original.isEmpty()) {
    return {max_dimension, max_dimension};
  }
  if (original.width() <= max_dimension && original.height() <= max_dimension) {
    return original;
  }
  return original.scaled(max_dimension, max_dimension, Qt::KeepAspectRatio);
}

// Runs on a QtConcurrent worker thread (REQ-NF-003). Decodes `image_bytes` at display size and
// atomically writes it to `dest_path` as PNG (QSaveFile: temp-file-plus-rename, so a crash mid-
// write never leaves a partial file at the final content-addressed path). Returns `dest_path` on
// success, an empty string on any failure.
QString decodeAndCache(const QByteArray& image_bytes, const QString& dest_path) {
  QBuffer buffer;
  buffer.setData(image_bytes);
  if (!buffer.open(QIODevice::ReadOnly)) {
    return {};
  }

  QImageReader reader(&buffer);
  reader.setScaledSize(scaledSizePreservingAspect(reader.size(), kMaxDimension));
  const QImage image = reader.read();
  if (image.isNull()) {
    return {};
  }

  QSaveFile file(dest_path);
  if (!file.open(QIODevice::WriteOnly)) {
    return {};
  }
  if (!image.save(&file, "PNG") || !file.commit()) {
    return {};
  }
  return dest_path;
}

// Runs on a QtConcurrent worker thread. Reads a local file (size-capped, REQ-F-029) then decodes/
// caches it via decodeAndCache() (REQ-F-027's file:// support).
QString resolveFileUrl(const QString& local_path, const QString& dest_path) {
  QFile file(local_path);
  if (!file.exists() || file.size() > kMaxFileBytes) {
    return {};
  }
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return decodeAndCache(file.readAll(), dest_path);
}

// Runs on a QtConcurrent worker thread. Parses a "data:<mediatype>;base64,<payload>" URI — the
// inline-artwork form several local-file players report (Haruna, VLC) when there is no file:// or
// http(s):// location to point at, only an embedded cover image. REQ-F-027 originally scoped this
// scheme out entirely as "unsupported, silently skipped"; extended here because that fallback
// was the common case in practice, not the exception. QUrl is not used to split the URI (the
// base64 payload is not itself a hierarchical URL and can contain characters QUrl would try to
// percent-decode); scheme detection alone happens via QUrl in resolve(), the payload is parsed
// directly from the raw string. Only the ";base64" form is supported (the form every observed
// MPRIS player actually emits) — anything else, or a decoded payload over the size cap, returns
// empty the same as any other unresolvable source (REQ-F-029/051).
QString resolveDataUrl(const QString& art_url, const QString& dest_path) {
  static const QLatin1String kPrefix("data:");
  if (!art_url.startsWith(kPrefix, Qt::CaseInsensitive)) {
    return {};
  }
  const qsizetype comma = art_url.indexOf(QLatin1Char(','), kPrefix.size());
  if (comma < 0) {
    return {};
  }
  const QString header = art_url.mid(kPrefix.size(), static_cast<int>(comma) - kPrefix.size()).toLower();
  if (!header.startsWith(QLatin1String("image/")) || !header.endsWith(QLatin1String(";base64")) ||
      header.count(QLatin1Char(';')) != 1) {
    return {};
  }
  const qsizetype encoded_size = art_url.size() - comma - 1;
  constexpr qsizetype kMaxEncodedBytes = ((kMaxFileBytes + 2) / 3) * 4;
  if (encoded_size <= 0 || encoded_size > kMaxEncodedBytes) {
    return {};
  }
  const auto decoded = QByteArray::fromBase64Encoding(art_url.mid(static_cast<int>(comma) + 1).toLatin1(),
                                                      QByteArray::AbortOnBase64DecodingErrors);
  if (!decoded) {
    return {};
  }
  const QByteArray& image_bytes = decoded.decoded;
  if (image_bytes.isEmpty() || image_bytes.size() > kMaxFileBytes) {
    return {};
  }
  return decodeAndCache(image_bytes, dest_path);
}
}  // namespace

QString MprisArtworkCache::defaultCacheRoot() {
  return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) +
         QStringLiteral("/holonight-shell/mpris-artwork/");
}

MprisArtworkCache::MprisArtworkCache(QString cache_root)
    : cache_root_(std::move(cache_root)), network_manager_(std::make_unique<QNetworkAccessManager>()) {
  QDir().mkpath(cache_root_);  // REQ-F-026: persists across restarts, created with default umask perms
  scanCacheRoot();             // REQ-F-030: seed LRU bookkeeping from whatever survived the restart
  evictIfOverBudget();
}

void MprisArtworkCache::scanCacheRoot() {
  const QFileInfoList files = QDir(cache_root_).entryInfoList(QDir::Files, QDir::NoSort);
  entries_.reserve(files.size());
  for (const QFileInfo& info : files) {
    entries_.append(
        CacheEntry{.path = info.absoluteFilePath(), .size = info.size(), .last_touch = info.lastModified()});
  }
}

void MprisArtworkCache::registerCacheEntry(const QString& path) {
  const QFileInfo info(path);
  entries_.removeIf([&path](const CacheEntry& entry) { return entry.path == path; });
  entries_.append(CacheEntry{.path = path, .size = info.size(), .last_touch = info.lastModified()});
  evictIfOverBudget();
}

void MprisArtworkCache::touchCacheEntry(const QString& path) {
  const QDateTime now = QDateTime::currentDateTime();
  QFile file(path);
  if (file.open(QIODevice::ReadOnly)) {
    file.setFileTime(now, QFileDevice::FileModificationTime);
    file.close();
  }
  const auto entry_it = std::ranges::find_if(entries_, [&path](const CacheEntry& entry) { return entry.path == path; });
  if (entry_it != entries_.end()) {
    entry_it->last_touch = now;
  } else {
    // Cache hit for a file the in-memory list does not know about (shouldn't normally happen,
    // but keeps bookkeeping self-healing rather than silently drifting from disk reality).
    entries_.append(CacheEntry{.path = path, .size = QFileInfo(path).size(), .last_touch = now});
  }
}

void MprisArtworkCache::evictIfOverBudget() {
  qint64 total = 0;
  for (const CacheEntry& entry : std::as_const(entries_)) {
    total += entry.size;
  }
  QList<QString> failed_paths;
  while (total > kCacheBudgetBytes && failed_paths.size() < entries_.size()) {
    const auto oldest_it =
        std::ranges::min_element(entries_, [&failed_paths](const CacheEntry& left, const CacheEntry& right) {
          const bool left_failed = failed_paths.contains(left.path);
          const bool right_failed = failed_paths.contains(right.path);
          return left_failed != right_failed ? !left_failed : left.last_touch < right.last_touch;
        });
    if (QFile::remove(oldest_it->path)) {
      total -= oldest_it->size;
      entries_.erase(oldest_it);
    } else {
      failed_paths.append(oldest_it->path);
    }
  }
}

MprisArtworkCache::~MprisArtworkCache() {
  // Aborting/deleting a QNetworkReply or a QFutureWatcher does not synchronously unwind whatever
  // work is already in flight on the network/thread-pool side — it just stops this object from
  // observing the result. That is fine: decodeAndCache()'s QSaveFile commit is what actually
  // matters for on-disk correctness, and it does not depend on this object's lifetime.
  for (QNetworkReply* reply : std::as_const(pending_replies_)) {
    QObject::disconnect(reply, nullptr, nullptr, nullptr);
    reply->abort();
    delete reply;
  }
  for (QFutureWatcher<QString>* watcher : std::as_const(pending_)) {
    QObject::disconnect(watcher, nullptr, nullptr, nullptr);
    delete watcher;
  }
}

void MprisArtworkCache::trackDecodeFuture(const QFuture<QString>& future, const QString& art_url,
                                          const std::function<void(QString)>& on_ready) {
  auto* watcher = new QFutureWatcher<QString>();
  QObject::connect(watcher, &QFutureWatcher<QString>::finished, watcher, [this, watcher, art_url, on_ready]() {
    const QString result = watcher->result();
    pending_.remove(art_url);
    watcher->deleteLater();
    if (!result.isEmpty()) {
      registerCacheEntry(result);  // REQ-F-030: bookkeep the freshly written file, evict if over budget
    }
    completeResolution(art_url, result, on_ready);
  });
  pending_.insert(art_url, watcher);
  watcher->setFuture(future);
}

void MprisArtworkCache::completeResolution(const QString& art_url, const QString& result,
                                           const std::function<void(QString)>& on_ready) {
  on_ready(result);
  const QList<std::function<void(QString)>> queued = queued_callbacks_.take(art_url);
  for (const auto& queued_ready : queued) {
    queued_ready(result);
  }
}

void MprisArtworkCache::resolve(const QString& art_url, std::function<void(QString local_path)> on_ready) {
  const QUrl url(art_url);
  const QString scheme = url.scheme().toLower();
  const bool is_file = scheme == QLatin1String("file");
  const bool is_http = scheme == QLatin1String("http") || scheme == QLatin1String("https");
  const bool is_data = scheme == QLatin1String("data");

  if (!is_file && !is_http && !is_data) {
    // Unsupported scheme (anything not file:/http(s):/data:) — "silently skipped" per REQ-F-027,
    // resolves to "no artwork" immediately, still via the queued path for a uniform caller
    // contract.
    QMetaObject::invokeMethod(qApp, [on_ready = std::move(on_ready)]() { on_ready(QString()); }, Qt::QueuedConnection);
    return;
  }

  const QString dest_path = cache_root_ + computeCacheKey(art_url) + QStringLiteral(".png");
  if (QFileInfo::exists(dest_path)) {
    QImageReader cached_reader(dest_path);
    if (cached_reader.canRead()) {
      touchCacheEntry(dest_path);
      QMetaObject::invokeMethod(
          qApp, [dest_path, on_ready = std::move(on_ready)]() { on_ready(dest_path); }, Qt::QueuedConnection);
      return;
    }
    if (QFile::remove(dest_path)) {
      entries_.removeIf([&dest_path](const CacheEntry& entry) { return entry.path == dest_path; });
    }
  }

  if (pending_.contains(art_url) || pending_replies_.contains(art_url)) {
    // REQ-F-028/DESIGN.md §2.4: a fetch/decode for this track_id is already in flight (e.g. two
    // MprisWidgetManagers on different monitors resolving the same now-playing track at once) —
    // piggy-back on it instead of starting a duplicate fetch.
    queued_callbacks_[art_url].append(std::move(on_ready));
    return;
  }

  if (is_file) {
    trackDecodeFuture(QtConcurrent::run(resolveFileUrl, url.toLocalFile(), dest_path), art_url, on_ready);
    return;
  }

  if (is_data) {
    trackDecodeFuture(QtConcurrent::run(resolveDataUrl, art_url, dest_path), art_url, on_ready);
    return;
  }

  // http(s):// — REQ-F-034: 10 s transfer timeout; REQ-F-029: abort once the download exceeds the
  // per-file size cap while still in flight. QNetworkAccessManager is inherently async and never
  // blocks the calling (GUI) thread regardless of which thread it runs on (REQ-NF-003).
  QNetworkRequest request(url);
  request.setTransferTimeout(10000);
  QNetworkReply* reply = network_manager_->get(request);
  pending_replies_.insert(art_url, reply);

  QObject::connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError>& errors) {
    qCDebug(lcMprisArtworkCache) << "artwork TLS validation failed" << reply->url() << errors;
  });

  QObject::connect(reply, &QNetworkReply::downloadProgress, reply,
                   [reply](qint64 bytes_received, qint64 /*bytes_total*/) {
                     if (bytes_received > kMaxFileBytes) {
                       reply->abort();
                     }
                   });

  QObject::connect(reply, &QNetworkReply::finished, reply, [this, reply, art_url, dest_path, on_ready]() {
    pending_replies_.remove(art_url);
    const bool fetch_ok = reply->error() == QNetworkReply::NoError;
    QByteArray bytes;
    if (fetch_ok) {
      bytes = reply->readAll();
    } else {
      // Timeout, HTTP error, or abort-on-oversize (REQ-F-034/029) — logged at debug level only,
      // per REQ-F-034's "no error popups or logged warnings that disrupt the user."
      qCDebug(lcMprisArtworkCache) << "artwork fetch failed" << reply->url() << reply->errorString();
    }
    reply->deleteLater();

    if (!fetch_ok || bytes.isEmpty()) {
      completeResolution(art_url, QString(), on_ready);
      return;
    }
    trackDecodeFuture(QtConcurrent::run(decodeAndCache, bytes, dest_path), art_url, on_ready);
  });
}

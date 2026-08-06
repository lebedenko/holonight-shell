#pragma once

#include <QDateTime>
#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QString>

#include <functional>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

// Async artwork resolver/cache for the MPRIS desktop widget (REQ-F-026...035). A plain class — not
// a QObject, not QML-exposed — owned by ShellApplication and shared by reference across every
// MprisWidgetManager instance (docs/sdd/mpris-desktop-widget/DESIGN.md §2.4/§5c). Deliberately
// kept off the QML_SINGLETON path: see project memory on the MPRIS topbar pill's registration
// gotcha (DESIGN.md §6/§7 risk #2) for why exposing this class to QML would need the same two-file
// ShellApplication::registerQmlTypes() wiring that has already bitten this codebase once.
class MprisArtworkCache {
 public:
  explicit MprisArtworkCache(QString cache_root = defaultCacheRoot());
  ~MprisArtworkCache();

  // Owns in-flight QFutureWatcher pointers directly (not parent/child-managed, since this class is
  // not a QObject) — copying or moving would leave two owners racing to delete the same watchers.
  MprisArtworkCache(const MprisArtworkCache&) = delete;
  MprisArtworkCache& operator=(const MprisArtworkCache&) = delete;
  MprisArtworkCache(MprisArtworkCache&&) = delete;
  MprisArtworkCache& operator=(MprisArtworkCache&&) = delete;

  // Resolves `art_url` (a file://, http(s)://, or base64 data: MPRIS mpris:artUrl value) to a
  // local, decoded/display-scaled cache file path. `on_ready` is invoked on the calling (GUI)
  // thread exactly once, always queued — never synchronously/reentrantly on the caller's stack,
  // even for a cache hit — with either a local file path or an empty string (unsupported scheme,
  // oversized fetch, timeout, or any error; caller falls back to the app icon per REQ-F-051/034/057).
  void resolve(const QString& art_url, std::function<void(QString local_path)> on_ready);

  [[nodiscard]] static QString defaultCacheRoot();

 private:
  // In-memory LRU bookkeeping entry for one cached file — no SQLite index, no JSON sidecar
  // (DESIGN.md §5c). `last_touch` mirrors the on-disk mtime; it is the eviction-order key.
  struct CacheEntry {
    QString path;
    qint64 size{0};
    QDateTime last_touch;
  };

  // Wires a QFutureWatcher around `future` (a background decode task, whatever produced it) so
  // its result reaches `on_ready` on the GUI thread and `track_id`'s pending entry is cleared —
  // shared by both the file:// and http(s):// resolve paths. A non-empty result is a freshly
  // written cache file: registers it for LRU bookkeeping and evicts if now over budget.
  void trackDecodeFuture(const QFuture<QString>& future, const QString& art_url,
                         const std::function<void(QString)>& on_ready);

  // Builds `entries_` from a directory scan of `cache_root_` at construction time (REQ-F-030) —
  // the filename-is-the-key design (§5c) means no persisted index is needed, just each file's
  // size and QFileInfo::lastModified().
  void scanCacheRoot();

  // Records a newly-written cache file in `entries_` (replacing any stale entry at the same
  // path) and runs eviction if the new total exceeds kCacheBudgetBytes.
  void registerCacheEntry(const QString& path);

  // REQ-F-030/035: refreshes `path`'s mtime on disk (QFileDevice::FileModificationTime, not
  // atime — many systems mount noatime) and its `entries_` bookkeeping, on every cache hit.
  void touchCacheEntry(const QString& path);

  // Deletes oldest-`last_touch` entries until total cached size is within kCacheBudgetBytes.
  void evictIfOverBudget();

  // Delivers `result` to `on_ready` plus every callback that piggy-backed onto this same exact
  // URL's in-flight fetch/decode via the dedup check in resolve() (§2.4: "sharing one cache
  // instance avoids duplicate downloads"). Called from every terminal path (decode success/
  // failure, http fetch failure) so no queued caller is ever left waiting forever.
  void completeResolution(const QString& art_url, const QString& result, const std::function<void(QString)>& on_ready);

  QString cache_root_;
  std::unique_ptr<QNetworkAccessManager> network_manager_;
  QHash<QString, QNetworkReply*> pending_replies_;    // exact raw URL -> in-flight http(s) fetch
  QHash<QString, QFutureWatcher<QString>*> pending_;  // exact raw URL -> in-flight decode
  // raw URL -> extra callbacks from concurrent resolve() calls that arrived while a fetch/decode
  // for that URL was already in flight; fanned out on completion instead of
  // starting a duplicate fetch.
  QHash<QString, QList<std::function<void(QString)>>> queued_callbacks_;
  QList<CacheEntry> entries_;  // LRU bookkeeping, oldest-first is not assumed
};

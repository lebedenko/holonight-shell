#include "RecentAppsTracker.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QStandardPaths>

#include <algorithm>

namespace {
Q_LOGGING_CATEGORY(lcRecent, "holonight.launcher.recent")
constexpr int kMaxEntries = 20;
}  // namespace

RecentAppsTracker* RecentAppsTracker::s_instance_ = nullptr;

RecentAppsTracker::RecentAppsTracker(QObject* parent) : QObject(parent) {
  s_instance_ = this;
  loadFromDisk();
}

RecentAppsTracker::~RecentAppsTracker() {
  if (s_instance_ == this) {
    s_instance_ = nullptr;
  }
}

void RecentAppsTracker::recordLaunch(const QString& desktop_file) {
  auto existing = std::ranges::find_if(
      entries_, [&desktop_file](const RecentRecord& rec) { return rec.desktop_file == desktop_file; });

  const QDateTime now = QDateTime::currentDateTimeUtc();
  if (existing != entries_.end()) {
    existing->last_used = now;
  } else {
    entries_.push_back({.desktop_file = desktop_file, .last_used = now});
    if (entries_.size() > kMaxEntries) {
      const auto oldest = std::ranges::min_element(
          entries_, [](const RecentRecord& lhs, const RecentRecord& rhs) { return lhs.last_used < rhs.last_used; });
      entries_.erase(oldest);
    }
  }

  saveToDisk();
  emit recentChanged();
}

void RecentAppsTracker::removeUnavailableDesktopFiles(const QSet<QString>& available_desktop_files) {
  const auto old_size = entries_.size();
  const auto removed = std::ranges::remove_if(entries_, [&available_desktop_files](const RecentRecord& rec) {
    return !available_desktop_files.contains(rec.desktop_file);
  });
  entries_.erase(removed.begin(), removed.end());
  if (entries_.size() == old_size) {
    return;
  }

  saveToDisk();
  emit recentChanged();
}

QVariantList RecentAppsTracker::recentEntries(int limit) const {
  QVector<const RecentRecord*> sorted;
  sorted.reserve(entries_.size());
  for (const auto& rec : entries_) {
    sorted.append(&rec);
  }
  std::ranges::sort(sorted,
                    [](const RecentRecord* lhs, const RecentRecord* rhs) { return lhs->last_used > rhs->last_used; });

  QVariantList result;
  const int count = std::min(limit, static_cast<int>(sorted.size()));
  for (int idx = 0; idx < count; ++idx) {
    QVariantMap entry;
    entry[QStringLiteral("desktopFile")] = sorted[idx]->desktop_file;
    entry[QStringLiteral("lastUsed")] = sorted[idx]->last_used;
    result.append(entry);
  }
  return result;
}

QDateTime RecentAppsTracker::lastUsedFor(const QString& desktop_file) const {
  const auto found = std::ranges::find_if(
      entries_, [&desktop_file](const RecentRecord& rec) { return rec.desktop_file == desktop_file; });
  return found != entries_.end() ? found->last_used : QDateTime{};
}

QString RecentAppsTracker::historyFilePath() {
  return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/launch-history.json");
}

void RecentAppsTracker::loadFromDisk() {
  const QString path = historyFilePath();
  QFile file(path);
  if (!file.exists()) {
    return;
  }
  if (!file.open(QIODevice::ReadOnly)) {
    qCWarning(lcRecent) << "Failed to open launch history:" << path;
    return;
  }

  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !doc.isArray()) {
    qCWarning(lcRecent) << "Invalid launch history JSON, starting fresh:" << error.errorString();
    return;
  }

  entries_.clear();
  for (const auto& val : doc.array()) {
    if (!val.isObject()) {
      continue;
    }
    const QJsonObject obj = val.toObject();
    const QString desktop_file = obj.value(QStringLiteral("desktopFile")).toString();
    const QString last_used_str = obj.value(QStringLiteral("lastUsed")).toString();
    if (desktop_file.isEmpty() || last_used_str.isEmpty()) {
      continue;
    }
    const QDateTime last_used = QDateTime::fromString(last_used_str, Qt::ISODate);
    if (!last_used.isValid()) {
      continue;
    }
    entries_.push_back({.desktop_file = desktop_file, .last_used = last_used});
  }
}

void RecentAppsTracker::saveToDisk() const {
  const QString path = historyFilePath();
  QDir dir = QFileInfo(path).dir();
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }

  QJsonArray arr;
  for (const auto& rec : entries_) {
    QJsonObject obj;
    obj[QStringLiteral("desktopFile")] = rec.desktop_file;
    obj[QStringLiteral("lastUsed")] = rec.last_used.toString(Qt::ISODate);
    arr.append(obj);
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    qCWarning(lcRecent) << "Failed to write launch history:" << path;
    return;
  }
  file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

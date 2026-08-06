#pragma once

#include <QDateTime>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVector>

class RecentAppsTracker : public QObject {
  Q_OBJECT

 public:
  explicit RecentAppsTracker(QObject* parent = nullptr);
  ~RecentAppsTracker() override;

  [[nodiscard]] static RecentAppsTracker* instance() { return s_instance_; }

  RecentAppsTracker(const RecentAppsTracker&) = delete;
  RecentAppsTracker& operator=(const RecentAppsTracker&) = delete;
  RecentAppsTracker(RecentAppsTracker&&) = delete;
  RecentAppsTracker& operator=(RecentAppsTracker&&) = delete;

  Q_INVOKABLE void recordLaunch(const QString& desktop_file);
  void removeUnavailableDesktopFiles(const QSet<QString>& available_desktop_files);
  [[nodiscard]] Q_INVOKABLE QVariantList recentEntries(int limit = 5) const;
  [[nodiscard]] Q_INVOKABLE QDateTime lastUsedFor(const QString& desktop_file) const;

 Q_SIGNALS:
  void recentChanged();

 private:
  struct RecentRecord {
    QString desktop_file;
    QDateTime last_used;
  };

  void loadFromDisk();
  void saveToDisk() const;
  [[nodiscard]] static QString historyFilePath();

  static RecentAppsTracker* s_instance_;
  QVector<RecentRecord> entries_;
};

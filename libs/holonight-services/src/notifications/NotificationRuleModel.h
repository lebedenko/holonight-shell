#pragma once

#include "DesktopEntryScanner.h"
#include "NotificationFilter.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QString>

#include <cstdint>

class NotificationRuleStore;

// Per-app notification rules, exposed as a QML singleton.
// Auto-populates via ensureApp() when new app_names are seen and optionally persists as user preferences.
class NotificationRuleModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

 public:
  enum Roles : std::uint16_t {  // NOLINT(cppcoreguidelines-use-enum-class): Qt model roles are int-compatible.
    AppNameRole = Qt::UserRole + 1,
    DisplayNameRole,
    DisplayIconRole,
    EnabledRole,
    UrgencyFilterRole,
    DesktopEntryRole,
    AppIconRole,
    LastSeenMsRole,
  };
  Q_ENUM(Roles)

  static constexpr qsizetype kMaxRuleCount = 256;

  explicit NotificationRuleModel(QObject* parent = nullptr);
  explicit NotificationRuleModel(NotificationRuleStore* store, const DesktopEntryScanner& scanner,
                                 QObject* parent = nullptr);

  void loadPersistedRules(NotificationRuleStore* store, const DesktopEntryScanner& scanner);

  // Called on every Notify() arrival with a non-empty app_name.
  void ensureApp(const QString& app_name);
  void ensureApp(const NotificationData& data);

  // Reference for the filter pipeline — passed to evaluateFilter() on each notification.
  [[nodiscard]] const QList<AppNotificationRule>& rules() const { return rules_; }

  Q_INVOKABLE void setEnabled(int row, bool enabled);
  Q_INVOKABLE void setUrgencyFilter(int row, int filter);

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

 Q_SIGNALS:
  void rulePersistenceFailed(const QString& action, const QString& reason);

 private:
  void persist(const QString& action) const;
  [[nodiscard]] DesktopEntry displayDesktopEntry(const AppNotificationRule& rule) const;
  static QString normalizedDesktopEntryId(QString desktop_entry);
  static QHash<QString, DesktopEntry> desktopEntriesById(const DesktopEntryScanner& scanner);
  static QList<AppNotificationRule> prunedRules(const QList<AppNotificationRule>& rules,
                                                const QSet<QString>& installed_ids);
  static QList<AppNotificationRule> boundedRules(QList<AppNotificationRule> rules);

  QList<AppNotificationRule> rules_;
  QHash<QString, DesktopEntry> desktop_entries_;
  NotificationRuleStore* store_{nullptr};
};

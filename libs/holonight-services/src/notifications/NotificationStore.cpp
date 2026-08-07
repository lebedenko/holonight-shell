#include "NotificationStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include <limits>
#include <optional>
#include <utility>

Q_LOGGING_CATEGORY(lcNotifStore, "holonight.notifications.store")

namespace {

constexpr int kHistoryVersion = 1;

QString resolveHistoryFilePath() {
  QString state_home = qEnvironmentVariable("XDG_STATE_HOME");
  if (state_home.isEmpty()) {
    state_home = QDir::homePath() + QLatin1String("/.local/state");
  }
  return state_home + QLatin1String("/holonight/notifications/history.json");
}

// QJsonObject/QJsonArray operator[] is the intended Qt API for keyed access;
// the check is done separately via isDouble/isObject on the retrieved value.
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

// Serializes one history item to a JSON object.
QJsonObject itemToJson(const NotificationHistoryItem& item, bool persist_body) {
  QJsonObject obj;
  obj[QStringLiteral("id")] = static_cast<qint64>(item.id);
  obj[QStringLiteral("appName")] = item.app_name;
  obj[QStringLiteral("appClass")] = item.app_class;
  obj[QStringLiteral("summary")] = item.summary;
  if (persist_body) {
    obj[QStringLiteral("body")] = item.body;
  }
  obj[QStringLiteral("appIcon")] = item.app_icon;
  obj[QStringLiteral("urgency")] = item.urgency;
  obj[QStringLiteral("timestampMs")] = item.timestamp_ms;
  obj[QStringLiteral("closedReason")] = item.closed_reason;
  obj[QStringLiteral("read")] = item.read;

  QJsonArray actions;
  for (const NotifAction& action : item.actions) {
    QJsonObject act;
    act[QStringLiteral("key")] = action.key;
    act[QStringLiteral("label")] = action.label;
    actions.append(act);
  }
  obj[QStringLiteral("actions")] = actions;
  return obj;
}

bool requireString(const QJsonObject& obj, QStringView key, QString* out) {
  const QJsonValue val = obj.value(key.toString());
  if (!val.isString()) {
    qCWarning(lcNotifStore) << "history entry missing or invalid" << key << "; skipping";
    return false;
  }
  *out = val.toString();
  return true;
}

bool requireInt(const QJsonObject& obj, QStringView key, qint64* out) {
  const QJsonValue val = obj.value(key.toString());
  if (!val.isDouble()) {
    qCWarning(lcNotifStore) << "history entry missing or invalid" << key << "; skipping";
    return false;
  }
  *out = val.toInteger();
  return true;
}

bool requireBool(const QJsonObject& obj, QStringView key, bool* out) {
  const QJsonValue val = obj.value(key.toString());
  if (!val.isBool()) {
    qCWarning(lcNotifStore) << "history entry missing or invalid" << key << "; skipping";
    return false;
  }
  *out = val.toBool();
  return true;
}

// Deserializes one action object. Returns false if required fields are missing.
bool actionFromJson(const QJsonValue& val, QList<NotifAction>& actions) {
  if (!val.isObject()) {
    qCWarning(lcNotifStore) << "history action is not an object; skipping entry";
    return false;
  }
  const QJsonObject act = val.toObject();
  NotifAction action;
  if (!requireString(act, QStringLiteral("key"), &action.key) ||
      !requireString(act, QStringLiteral("label"), &action.label)) {
    return false;
  }
  if (!action.key.isEmpty()) {
    actions.append(action);
  }
  return true;
}

// Deserializes one JSON object to a history item. Returns an invalid item
// (id==0) and logs a warning when required fields are missing or wrong type.
NotificationHistoryItem itemFromJson(const QJsonObject& obj) {
  NotificationHistoryItem item;

  qint64 notif_id = 0;
  if (!requireInt(obj, QStringLiteral("id"), &notif_id)) {
    return item;
  }
  if (notif_id <= 0 || std::cmp_greater(notif_id, std::numeric_limits<uint32_t>::max())) {
    qCWarning(lcNotifStore) << "history entry id out of range; skipping";
    return item;
  }
  item.id = static_cast<uint32_t>(notif_id);

  if (!requireString(obj, QStringLiteral("appName"), &item.app_name) ||
      !requireString(obj, QStringLiteral("appClass"), &item.app_class) ||
      !requireString(obj, QStringLiteral("summary"), &item.summary) ||
      !requireString(obj, QStringLiteral("appIcon"), &item.app_icon)) {
    item.id = 0;
    return item;
  }

  const QJsonValue body_val = obj.value(QStringLiteral("body"));
  if (!body_val.isUndefined()) {
    if (!body_val.isString()) {
      qCWarning(lcNotifStore) << "history entry invalid body; skipping";
      item.id = 0;
      return item;
    }
    item.body = body_val.toString();
  }

  qint64 int_value = 0;
  if (!requireInt(obj, QStringLiteral("urgency"), &int_value)) {
    item.id = 0;
    return item;
  }
  item.urgency = static_cast<int>(int_value);
  if (!requireInt(obj, QStringLiteral("timestampMs"), &item.timestamp_ms)) {
    item.id = 0;
    return item;
  }
  if (!requireInt(obj, QStringLiteral("closedReason"), &int_value)) {
    item.id = 0;
    return item;
  }
  item.closed_reason = static_cast<int>(int_value);
  if (!requireBool(obj, QStringLiteral("read"), &item.read)) {
    item.id = 0;
    return item;
  }

  const QJsonValue actions_val = obj.value(QStringLiteral("actions"));
  if (!actions_val.isArray()) {
    qCWarning(lcNotifStore) << "history entry missing or invalid actions; skipping";
    item.id = 0;
    return item;
  }
  const QJsonArray actions = actions_val.toArray();
  for (const auto& val : actions) {
    if (!actionFromJson(val, item.actions)) {
      item.id = 0;
      return item;
    }
  }

  return item;
}

// Pure function run on a worker thread — writes history to disk atomically.
void writeHistoryToDisk(const QString& file_path, const QList<NotificationHistoryItem>& items, bool persist_body) {
  QJsonArray entries;
  for (const NotificationHistoryItem& item : items) {
    entries.append(itemToJson(item, persist_body));
  }

  QJsonObject root;
  root[QStringLiteral("version")] = kHistoryVersion;
  root[QStringLiteral("entries")] = entries;

  const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact);

  QSaveFile file(file_path);
  if (!file.open(QIODevice::WriteOnly)) {
    qCWarning(lcNotifStore) << "Failed to open history file for writing:" << file_path;
    return;
  }
  file.write(data);
  if (!file.commit()) {
    qCWarning(lcNotifStore) << "Failed to commit history file:" << file_path;
  }
}

// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

std::optional<QJsonArray> parseHistoryEntries(const QByteArray& data) {
  QJsonParseError parse_error;
  const QJsonDocument doc = QJsonDocument::fromJson(data, &parse_error);
  if (doc.isNull()) {
    qCWarning(lcNotifStore) << "History JSON parse error:" << parse_error.errorString();
    return std::nullopt;
  }
  if (!doc.isObject()) {
    qCWarning(lcNotifStore) << "History JSON root is not an object; ignoring file";
    return std::nullopt;
  }
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  const QJsonObject root = doc.object();
  const QJsonValue version = root[QStringLiteral("version")];
  if (!version.isDouble() || version.toInt() != kHistoryVersion) {
    qCWarning(lcNotifStore) << "History JSON version mismatch; ignoring file";
    return std::nullopt;
  }
  const QJsonValue entries_val = root[QStringLiteral("entries")];
  // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  if (!entries_val.isArray()) {
    qCWarning(lcNotifStore) << "History JSON entries is not an array; ignoring file";
    return std::nullopt;
  }
  return entries_val.toArray();
}

}  // namespace

NotificationStore::NotificationStore(const HoloNight::ShellConfig::NotificationHistoryConfig& config, QObject* parent)
    : QObject(parent), config_(config), file_path_(resolveHistoryFilePath()) {}

QList<NotificationHistoryItem> NotificationStore::load() {
  QList<NotificationHistoryItem> result;

  QFile file(file_path_);
  if (!file.exists()) {
    return result;
  }
  if (!file.open(QIODevice::ReadOnly)) {
    qCWarning(lcNotifStore) << "Failed to open history file:" << file_path_;
    return result;
  }

  const QByteArray data = file.readAll();
  file.close();

  const std::optional<QJsonArray> maybe_entries = parseHistoryEntries(data);
  if (!maybe_entries.has_value()) {
    return result;
  }
  const QJsonArray& entries = *maybe_entries;
  result.reserve(entries.size());

  for (const auto& val : entries) {
    if (!val.isObject()) {
      qCWarning(lcNotifStore) << "History entry is not a JSON object; skipping";
      continue;
    }
    const NotificationHistoryItem item = itemFromJson(val.toObject());
    if (item.id != 0) {
      result.append(item);
    }
  }

  qCInfo(lcNotifStore) << "Loaded" << result.size() << "history entries from" << file_path_;
  return result;
}

void NotificationStore::persist(QList<NotificationHistoryItem> snapshot) {
  if (write_in_flight_) {
    write_dirty_ = true;
    pending_snapshot_ = std::move(snapshot);
    return;
  }
  launchWrite(std::move(snapshot));
}

void NotificationStore::updateConfig(const HoloNight::ShellConfig::NotificationHistoryConfig& config) {
  config_ = config;
}

void NotificationStore::launchWrite(QList<NotificationHistoryItem> snapshot) {
  ensureDirectoryExists();

  write_in_flight_ = true;
  write_dirty_ = false;

  const QString path = file_path_;
  const bool persist_body = config_.persist_body;

  if (watcher_ == nullptr) {
    watcher_ = new QFutureWatcher<void>(this);
    connect(watcher_, &QFutureWatcher<void>::finished, this, &NotificationStore::onWriteFinished);
  }

  watcher_->setFuture(QtConcurrent::run(
      [path, items = std::move(snapshot), persist_body]() { writeHistoryToDisk(path, items, persist_body); }));
}

void NotificationStore::onWriteFinished() {
  write_in_flight_ = false;
  emit writeCompleted();
  if (write_dirty_ && !write_in_flight_) {
    launchWrite(std::move(pending_snapshot_));
  }
}

void NotificationStore::ensureDirectoryExists() {
  const QDir dir = QFileInfo(file_path_).dir();
  if (!dir.exists() && !dir.mkpath(QLatin1String("."))) {
    qCWarning(lcNotifStore) << "Failed to create history directory:" << dir.absolutePath();
  }
}

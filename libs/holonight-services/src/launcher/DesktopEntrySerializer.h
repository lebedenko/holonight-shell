#pragma once

#include "DesktopEntryScanner.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <optional>

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
// QJsonObject::operator[] is the idiomatic API; bounds-safe alternatives don't exist.

namespace DesktopEntrySerializer {

inline QString jsonKey(const DesktopEntryTextField& field) { return QString::fromLatin1(field.json_key); }

inline QJsonObject toJson(const DesktopEntry& entry) {
  QJsonArray actions_array;
  for (const auto& action : entry.actions) {
    QJsonObject action_obj;
    action_obj[QStringLiteral("name")] = action.name;
    action_obj[QStringLiteral("exec")] = action.exec;
    actions_array.append(action_obj);
  }

  QJsonObject obj;
  for (const DesktopEntryTextField& field : kDesktopEntryTextFields) {
    obj[jsonKey(field)] = entry.*(field.member);
  }
  obj[QStringLiteral("terminal")] = entry.terminal;
  obj[QStringLiteral("no_display")] = entry.no_display;
  obj[QStringLiteral("actions")] = actions_array;

  QJsonArray mime_types_array;
  for (const QString& mime : entry.mime_types) {
    mime_types_array.append(mime);
  }
  obj[QStringLiteral("mime_types")] = mime_types_array;

  return obj;
}

inline std::optional<DesktopEntry> fromJson(const QJsonObject& obj) {
  if (!obj[QStringLiteral("name")].isString() || obj[QStringLiteral("name")].toString().isEmpty()) {
    return std::nullopt;
  }
  if (!obj[QStringLiteral("exec")].isString() || obj[QStringLiteral("exec")].toString().isEmpty()) {
    return std::nullopt;
  }
  if (!obj[QStringLiteral("desktop_file")].isString() || obj[QStringLiteral("desktop_file")].toString().isEmpty()) {
    return std::nullopt;
  }

  DesktopEntry entry;
  for (const DesktopEntryTextField& field : kDesktopEntryTextFields) {
    entry.*(field.member) = obj[jsonKey(field)].toString();
  }
  entry.terminal = obj[QStringLiteral("terminal")].toBool(false);
  entry.no_display = obj[QStringLiteral("no_display")].toBool(false);

  const QJsonArray actions_array = obj[QStringLiteral("actions")].toArray();
  entry.actions.reserve(static_cast<int>(actions_array.size()));
  for (const auto& val : actions_array) {
    const QJsonObject action_obj = val.toObject();
    const QString action_name = action_obj[QStringLiteral("name")].toString();
    const QString action_exec = action_obj[QStringLiteral("exec")].toString();
    if (!action_name.isEmpty() && !action_exec.isEmpty()) {
      entry.actions.append({.name = action_name, .exec = action_exec});
    }
  }

  const QJsonArray mime_types_array = obj[QStringLiteral("mime_types")].toArray();
  entry.mime_types.reserve(static_cast<int>(mime_types_array.size()));
  for (const auto& val : mime_types_array) {
    const QString mime = val.toString().trimmed();
    if (!mime.isEmpty()) {
      entry.mime_types.append(mime);
    }
  }

  return entry;
}

}  // namespace DesktopEntrySerializer
// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

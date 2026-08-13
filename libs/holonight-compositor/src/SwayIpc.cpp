#include "SwayIpc.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstring>
#include <utility>

namespace {
constexpr QByteArrayView kMagic{"i3-ipc"};
constexpr qsizetype kHeaderSize = 14;

void inspectSwayTree(const QJsonObject& node, const QString& output, const QString& workspace,
                     QHash<QString, bool>* occupied, QHash<QString, CompositorActiveWindow>* windows) {
  const QString type = node.value(QStringLiteral("type")).toString();
  QString next_output = output;
  QString next_workspace = workspace;
  if (type == QLatin1String("output")) {
    next_output = node.value(QStringLiteral("name")).toString();
  }
  if (type == QLatin1String("workspace")) {
    next_workspace = node.value(QStringLiteral("name")).toString();
    if (next_workspace == QLatin1String("__i3_scratch")) {
      return;
    }
  }

  const bool leaf = node.value(QStringLiteral("nodes")).toArray().isEmpty() &&
                    node.value(QStringLiteral("floating_nodes")).toArray().isEmpty();
  if (leaf && !next_workspace.isEmpty() && type == QLatin1String("con")) {
    occupied->insert(next_workspace, true);
    if (node.value(QStringLiteral("focused")).toBool(false) && !next_output.isEmpty()) {
      const QJsonObject properties = node.value(QStringLiteral("window_properties")).toObject();
      QString app_id = node.value(QStringLiteral("app_id")).toString();
      if (app_id.isEmpty()) {
        app_id = properties.value(QStringLiteral("class")).toString();
      }
      windows->insert(next_output, {.app_id = app_id, .title = node.value(QStringLiteral("name")).toString()});
    }
  }

  for (const QString& child_list : {QStringLiteral("nodes"), QStringLiteral("floating_nodes")}) {
    for (const QJsonValue& value : node.value(child_list).toArray()) {
      if (value.isObject()) {
        inspectSwayTree(value.toObject(), next_output, next_workspace, occupied, windows);
      }
    }
  }
}
}  // namespace

QByteArray encodeSwayIpcFrame(quint32 type, const QByteArray& payload) {
  QByteArray frame(kHeaderSize, Qt::Uninitialized);
  std::memcpy(frame.data(), kMagic.data(), kMagic.size());
  const auto length = static_cast<quint32>(payload.size());
  std::memcpy(frame.data() + 6, &length, sizeof(length));
  std::memcpy(frame.data() + 10, &type, sizeof(type));
  frame.append(payload);
  return frame;
}

bool SwayIpcDecoder::append(const QByteArray& bytes) {
  if (!error_.isEmpty()) {
    return false;
  }
  buffer_.append(bytes);
  while (buffer_.size() >= kHeaderSize) {
    if (QByteArrayView(buffer_).first(6) != kMagic) {
      error_ = QStringLiteral("invalid i3 IPC magic");
      return false;
    }
    quint32 length = 0;
    std::memcpy(&length, buffer_.constData() + 6, sizeof(length));
    if (length > kMaximumPayload) {
      error_ = QStringLiteral("i3 IPC payload exceeds 8 MiB");
      return false;
    }
    if (buffer_.size() < kHeaderSize + static_cast<qsizetype>(length)) {
      break;
    }
    quint32 type = 0;
    std::memcpy(&type, buffer_.constData() + 10, sizeof(type));
    frames_.append({.type = type, .payload = buffer_.mid(kHeaderSize, length)});
    buffer_.remove(0, kHeaderSize + length);
  }
  return true;
}

QList<SwayIpcFrame> SwayIpcDecoder::takeFrames() { return std::exchange(frames_, {}); }

QString escapeSwayWorkspaceName(const QString& name) {
  QString escaped = name;
  escaped.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
  escaped.replace(QStringLiteral("\""), QStringLiteral("\\\""));
  return escaped;
}

std::optional<CompositorSnapshot> parseSwaySnapshot(const QByteArray& workspaces_json, const QByteArray& outputs_json,
                                                    const QByteArray& tree_json) {
  QJsonParseError error;
  const QJsonDocument workspaces = QJsonDocument::fromJson(workspaces_json, &error);
  if (error.error != QJsonParseError::NoError || !workspaces.isArray()) {
    return std::nullopt;
  }
  const QJsonDocument outputs = QJsonDocument::fromJson(outputs_json, &error);
  if (error.error != QJsonParseError::NoError || !outputs.isArray()) {
    return std::nullopt;
  }
  const QJsonDocument tree = QJsonDocument::fromJson(tree_json, &error);
  if (error.error != QJsonParseError::NoError || !tree.isObject()) {
    return std::nullopt;
  }

  CompositorSnapshot snapshot{
      .connected = true,
      .capabilities = {.workspace_listing = true,
                       .workspace_activation = true,
                       .active_window = true,
                       .focused_output = true,
                       .urgency = true,
                       .occupancy = true},
  };
  QHash<QString, bool> occupied;
  inspectSwayTree(tree.object(), {}, {}, &occupied, &snapshot.active_windows);

  for (const QJsonValue& value : outputs.array()) {
    const QJsonObject output = value.toObject();
    if (output.value(QStringLiteral("focused")).toBool(false)) {
      snapshot.focused_output = output.value(QStringLiteral("name")).toString();
      break;
    }
  }

  int order = 0;
  for (const QJsonValue& value : workspaces.array()) {
    const QJsonObject workspace = value.toObject();
    const QString name = workspace.value(QStringLiteral("name")).toString();
    if (name.isEmpty() || name == QLatin1String("__i3_scratch")) {
      continue;
    }
    const int number = workspace.value(QStringLiteral("num")).toInt(-1);
    const QString output = workspace.value(QStringLiteral("output")).toString();
    const bool focused = workspace.value(QStringLiteral("focused")).toBool(false);
    snapshot.workspaces.append({.id = name,
                                .numeric_slot = number >= 0 ? std::optional<int>{number} : std::nullopt,
                                .display_name = name,
                                .stable_order = order++,
                                .outputs = output.isEmpty() ? QStringList{} : QStringList{output},
                                .active = workspace.value(QStringLiteral("visible")).toBool(false),
                                .focused = focused,
                                .urgent = workspace.value(QStringLiteral("urgent")).toBool(false),
                                .occupied = occupied.value(name, false)});
  }
  return snapshot;
}

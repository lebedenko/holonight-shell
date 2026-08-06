#include "HyprlandIpc.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QLoggingCategory>
#include <QRegularExpression>

#include <limits>

Q_LOGGING_CATEGORY(lcHyprlandIpc, "holonight.hyprland.ipc")

std::optional<HyprlandActiveWindow> parseHyprlandActiveWindowEvent(const QByteArray& line) {
  constexpr QByteArrayView kPrefix{"activewindow>>"};
  if (!line.startsWith(kPrefix)) {
    return std::nullopt;
  }

  const QByteArray payload = line.sliced(kPrefix.size());
  const qsizetype comma = payload.indexOf(',');
  if (comma < 0) {
    return HyprlandActiveWindow{};
  }

  return HyprlandActiveWindow{
      .app_class = QString::fromUtf8(payload.left(comma)),
      .title = QString::fromUtf8(payload.sliced(comma + 1)),
  };
}

std::optional<HyprlandActiveWindow> parseHyprlandActiveWindowJson(const QByteArray& response) {
  const QJsonDocument doc = QJsonDocument::fromJson(response);
  if (!doc.isObject()) {
    qCWarning(lcHyprlandIpc) << "parseHyprlandActiveWindowJson: expected JSON object";
    return std::nullopt;
  }

  const QJsonObject obj = doc.object();
  return HyprlandActiveWindow{
      .app_class = obj.value(QStringLiteral("class")).toString(),
      .title = obj.value(QStringLiteral("title")).toString(),
  };
}

std::optional<HyprlandKeyboardLayout> parseHyprlandKeyboardLayoutEvent(const QByteArray& line) {
  constexpr QByteArrayView kPrefix{"activelayout>>"};
  if (!line.startsWith(kPrefix)) {
    return std::nullopt;
  }

  const QByteArray payload = line.sliced(kPrefix.size());
  const qsizetype comma = payload.indexOf(',');
  if (comma < 0) {
    return std::nullopt;
  }

  return HyprlandKeyboardLayout{
      .keyboard_name = QString::fromUtf8(payload.left(comma)),
      .layout_name = QString::fromUtf8(payload.sliced(comma + 1)),
  };
}

std::optional<QString> parseHyprlandKeyboardLayoutDevicesJson(const QByteArray& response) {
  const QJsonDocument doc = QJsonDocument::fromJson(response);
  if (!doc.isObject()) {
    qCWarning(lcHyprlandIpc) << "parseHyprlandKeyboardLayoutDevicesJson: expected JSON object";
    return std::nullopt;
  }

  const QJsonArray keyboards = doc.object().value(QStringLiteral("keyboards")).toArray();
  QString first_layout;
  for (const auto& value : keyboards) {
    if (!value.isObject()) {
      continue;
    }

    const QJsonObject keyboard = value.toObject();
    QString layout = keyboard.value(QStringLiteral("active_keymap")).toString();
    if (layout.isEmpty()) {
      continue;
    }
    if (first_layout.isEmpty()) {
      first_layout = layout;
    }
    if (keyboard.value(QStringLiteral("main")).toBool(false)) {
      return layout;
    }
  }

  if (!first_layout.isEmpty()) {
    return first_layout;
  }
  return std::nullopt;
}

QString keyboardLayoutCode(const QString& layout_name) {
  const QString normalized = layout_name.trimmed().toLower();
  if (normalized.isEmpty()) {
    return {};
  }

  static const QHash<QString, QString> kKnownCodes = {
      {QStringLiteral("arabic"), QStringLiteral("AR")},     {QStringLiteral("armenian"), QStringLiteral("HY")},
      {QStringLiteral("belarusian"), QStringLiteral("BE")}, {QStringLiteral("bulgarian"), QStringLiteral("BG")},
      {QStringLiteral("chinese"), QStringLiteral("ZH")},    {QStringLiteral("croatian"), QStringLiteral("HR")},
      {QStringLiteral("czech"), QStringLiteral("CS")},      {QStringLiteral("danish"), QStringLiteral("DA")},
      {QStringLiteral("dutch"), QStringLiteral("NL")},      {QStringLiteral("english"), QStringLiteral("EN")},
      {QStringLiteral("estonian"), QStringLiteral("ET")},   {QStringLiteral("finnish"), QStringLiteral("FI")},
      {QStringLiteral("french"), QStringLiteral("FR")},     {QStringLiteral("georgian"), QStringLiteral("KA")},
      {QStringLiteral("german"), QStringLiteral("DE")},     {QStringLiteral("greek"), QStringLiteral("EL")},
      {QStringLiteral("hebrew"), QStringLiteral("HE")},     {QStringLiteral("hungarian"), QStringLiteral("HU")},
      {QStringLiteral("italian"), QStringLiteral("IT")},    {QStringLiteral("japanese"), QStringLiteral("JA")},
      {QStringLiteral("kazakh"), QStringLiteral("KK")},     {QStringLiteral("korean"), QStringLiteral("KO")},
      {QStringLiteral("latvian"), QStringLiteral("LV")},    {QStringLiteral("lithuanian"), QStringLiteral("LT")},
      {QStringLiteral("norwegian"), QStringLiteral("NO")},  {QStringLiteral("polish"), QStringLiteral("PL")},
      {QStringLiteral("portuguese"), QStringLiteral("PT")}, {QStringLiteral("romanian"), QStringLiteral("RO")},
      {QStringLiteral("russian"), QStringLiteral("RU")},    {QStringLiteral("serbian"), QStringLiteral("SR")},
      {QStringLiteral("slovak"), QStringLiteral("SK")},     {QStringLiteral("spanish"), QStringLiteral("ES")},
      {QStringLiteral("swedish"), QStringLiteral("SV")},    {QStringLiteral("turkish"), QStringLiteral("TR")},
      {QStringLiteral("ukrainian"), QStringLiteral("UK")},
  };

  static const QRegularExpression kWordBoundary(QStringLiteral("[\\s(]"));
  const QString first_word = normalized.section(kWordBoundary, 0, 0);
  const auto known = kKnownCodes.constFind(first_word);
  if (known != kKnownCodes.constEnd()) {
    return *known;
  }

  QString letters;
  for (const QChar chr : normalized) {
    if (chr.isLetter()) {
      letters.append(chr);
      if (letters.size() == 2) {
        return letters.toUpper();
      }
    }
  }
  return normalized.left(2).toUpper();
}

std::optional<int> parseHyprlandWorkspaceEvent(const QByteArray& line) {
  constexpr QByteArrayView kPrefix{"workspace>>"};
  if (!line.startsWith(kPrefix)) {
    return std::nullopt;
  }

  bool parsed_ok = false;
  const int workspace_id = QString::fromUtf8(line.sliced(kPrefix.size())).toInt(&parsed_ok);
  if (!parsed_ok) {
    return std::nullopt;
  }
  return workspace_id;
}

std::optional<HyprlandFocusedMonitor> parseHyprlandFocusedMonitorEvent(const QByteArray& line) {
  constexpr QByteArrayView kPrefix{"focusedmon>>"};
  if (!line.startsWith(kPrefix)) {
    return std::nullopt;
  }

  const QByteArray payload = line.sliced(kPrefix.size());
  const qsizetype comma = payload.indexOf(',');
  if (comma < 0) {
    return std::nullopt;
  }

  return HyprlandFocusedMonitor{
      .monitor_name = QString::fromUtf8(payload.left(comma)).trimmed(),
      .workspace_name = QString::fromUtf8(payload.sliced(comma + 1)).trimmed(),
  };
}

std::optional<QString> parseHyprlandUrgentWindowEvent(const QByteArray& line) {
  constexpr QByteArrayView kPrefix{"urgent>>"};
  if (!line.startsWith(kPrefix)) {
    return std::nullopt;
  }

  QString address = QString::fromUtf8(line.sliced(kPrefix.size())).trimmed();
  if (address.isEmpty()) {
    return std::nullopt;
  }
  return address;
}

std::optional<int> parseHyprlandActiveWorkspaceJson(const QByteArray& response) {
  const QJsonDocument doc = QJsonDocument::fromJson(response);
  if (!doc.isObject()) {
    qCWarning(lcHyprlandIpc) << "parseHyprlandActiveWorkspaceJson: expected JSON object";
    return std::nullopt;
  }

  const int workspace_id = doc.object().value(QStringLiteral("id")).toInt(0);
  if (workspace_id <= 0) {
    return std::nullopt;
  }
  return workspace_id;
}

std::optional<HyprlandWorkspaceSnapshot> parseHyprlandWorkspacesJson(const QByteArray& response) {
  const QJsonDocument doc = QJsonDocument::fromJson(response);
  if (!doc.isArray()) {
    qCWarning(lcHyprlandIpc) << "parseHyprlandWorkspacesJson: expected JSON array";
    return std::nullopt;
  }

  HyprlandWorkspaceSnapshot snapshot;
  const QJsonArray workspaces = doc.array();
  for (const auto& value : workspaces) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject object = value.toObject();
    const int workspace_id = object.value(QStringLiteral("id")).toInt(0);
    const int windows = object.value(QStringLiteral("windows")).toInt(0);
    if (workspace_id > 0 && windows > 0) {
      snapshot.occupied_workspace_ids.insert(workspace_id);
    }
  }
  return snapshot;
}

bool isHyprlandWorkspaceRefreshEvent(const QByteArray& line) {
  return line.startsWith("workspace>>") || line.startsWith("focusedmon>>") || line.startsWith("openwindow>>") ||
         line.startsWith("closewindow>>") || line.startsWith("movewindow>>") || line.startsWith("createworkspace>>") ||
         line.startsWith("destroyworkspace>>") || line.startsWith("renameworkspace>>");
}

std::optional<HyprlandOpenWindow> parseHyprlandOpenWindowEvent(const QByteArray& line) {
  constexpr QByteArrayView kPrefix{"openwindow>>"};
  if (!line.startsWith(kPrefix)) {
    return std::nullopt;
  }

  const QByteArray payload = line.sliced(kPrefix.size());
  const qsizetype comma1 = payload.indexOf(',');
  if (comma1 < 0) {
    return std::nullopt;
  }
  const qsizetype comma2 = payload.indexOf(',', comma1 + 1);
  if (comma2 < 0) {
    return std::nullopt;
  }
  const qsizetype comma3 = payload.indexOf(',', comma2 + 1);
  if (comma3 < 0) {
    return std::nullopt;
  }

  return HyprlandOpenWindow{
      .address = QString::fromUtf8(payload.left(comma1)),
      .workspace_name = QString::fromUtf8(payload.sliced(comma1 + 1, comma2 - comma1 - 1)),
      .app_class = QString::fromUtf8(payload.sliced(comma2 + 1, comma3 - comma2 - 1)),
      .title = QString::fromUtf8(payload.sliced(comma3 + 1)),
  };
}

std::optional<QHash<QString, int>> parseHyprlandMonitorsJson(const QByteArray& response) {
  const QJsonDocument doc = QJsonDocument::fromJson(response);
  if (!doc.isArray()) {
    qCWarning(lcHyprlandIpc) << "parseHyprlandMonitorsJson: expected JSON array";
    return std::nullopt;
  }

  QHash<QString, int> result;
  for (const auto& val : doc.array()) {
    if (!val.isObject()) {
      continue;
    }
    const QJsonObject obj = val.toObject();
    QString name = obj.value(QStringLiteral("name")).toString();
    const int ws_id = obj.value(QStringLiteral("activeWorkspace")).toObject().value(QStringLiteral("id")).toInt(0);
    if (!name.isEmpty() && ws_id > 0) {
      result.insert(name, ws_id);
    }
  }
  return result;
}

std::optional<QString> parseHyprlandFocusedMonitorNameJson(const QByteArray& response) {
  const QJsonDocument doc = QJsonDocument::fromJson(response);
  if (!doc.isArray()) {
    qCWarning(lcHyprlandIpc) << "parseHyprlandFocusedMonitorNameJson: expected JSON array";
    return std::nullopt;
  }

  for (const auto& val : doc.array()) {
    if (!val.isObject()) {
      continue;
    }
    const QJsonObject obj = val.toObject();
    if (QString name = obj.value(QStringLiteral("name")).toString();
        !name.isEmpty() && obj.value(QStringLiteral("focused")).toBool(false)) {
      return name;
    }
  }
  return QString{};
}

std::optional<QList<HyprlandClientInfo>> parseHyprlandClientsJson(const QByteArray& response) {
  const QJsonDocument doc = QJsonDocument::fromJson(response);
  if (!doc.isArray()) {
    qCWarning(lcHyprlandIpc) << "parseHyprlandClientsJson: expected JSON array";
    return std::nullopt;
  }

  QList<HyprlandClientInfo> result;
  for (const auto& val : doc.array()) {
    if (!val.isObject()) {
      continue;
    }
    const QJsonObject obj = val.toObject();
    const QString app_class = obj.value(QStringLiteral("class")).toString();
    const QString title = obj.value(QStringLiteral("title")).toString();
    if (app_class.isEmpty() || title.isEmpty()) {
      continue;
    }
    const QString address = obj.value(QStringLiteral("address")).toString();
    const int ws_id = obj.value(QStringLiteral("workspace")).toObject().value(QStringLiteral("id")).toInt(0);
    const int focus_id = obj.value(QStringLiteral("focusHistoryID")).toInt(std::numeric_limits<int>::max());
    result.append(HyprlandClientInfo{
        .address = address,
        .app_class = app_class,
        .title = title,
        .workspace_id = ws_id,
        .focus_history_id = focus_id,
    });
  }
  return result;
}

std::optional<int> workspaceIdForHyprlandClientAddress(const QList<HyprlandClientInfo>& clients,
                                                       const QString& address) {
  const QString window_address = address.trimmed();
  if (window_address.isEmpty()) {
    return std::nullopt;
  }

  for (const HyprlandClientInfo& client : clients) {
    if (client.address.endsWith(window_address, Qt::CaseInsensitive) && client.workspace_id > 0) {
      return client.workspace_id;
    }
  }
  return std::nullopt;
}

std::optional<int> workspaceIdForHyprlandClientAddressJson(const QByteArray& response, const QString& address) {
  const QString window_address = address.trimmed();
  if (window_address.isEmpty()) {
    return std::nullopt;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(response);
  if (!doc.isArray()) {
    qCWarning(lcHyprlandIpc) << "workspaceIdForHyprlandClientAddressJson: expected JSON array";
    return std::nullopt;
  }

  for (const auto& val : doc.array()) {
    if (!val.isObject()) {
      continue;
    }
    const QJsonObject obj = val.toObject();
    const QString client_address = obj.value(QStringLiteral("address")).toString();
    const int workspace_id = obj.value(QStringLiteral("workspace")).toObject().value(QStringLiteral("id")).toInt(0);
    if (client_address.endsWith(window_address, Qt::CaseInsensitive) && workspace_id > 0) {
      return workspace_id;
    }
  }
  return std::nullopt;
}

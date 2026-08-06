#include "DesktopEntryScanner.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <ranges>
#include <utility>

namespace {
constexpr QLatin1StringView kDesktopEntrySection{"[Desktop Entry]"};
constexpr QLatin1StringView kDesktopActionPrefix{"[Desktop Action "};

void addUniqueDir(QStringList* dirs, const QString& dir_path) {
  if (dir_path.isEmpty()) {
    return;
  }
  const QString clean_path = QDir::cleanPath(dir_path);
  if (!dirs->contains(clean_path)) {
    dirs->append(clean_path);
  }
}

bool isFieldCode(QChar code_char) {
  switch (code_char.toLatin1()) {
    case 'f':
    case 'F':
    case 'u':
    case 'U':
    case 'd':
    case 'D':
    case 'n':
    case 'N':
    case 'i':
    case 'c':
    case 'k':
    case 'v':
    case 'm':
      return true;
    default:
      return false;
  }
}

QString unescapeDesktopString(const QString& value) {
  QString result;
  result.reserve(value.size());
  for (int i = 0; i < value.size(); ++i) {
    if (value.at(i) == QLatin1Char('\\') && i + 1 < value.size()) {
      const QChar next = value.at(i + 1);
      if (next == QLatin1Char('n')) {
        result.append(QLatin1Char('\n'));
        ++i;
      } else if (next == QLatin1Char('t')) {
        result.append(QLatin1Char('\t'));
        ++i;
      } else if (next == QLatin1Char('r')) {
        result.append(QLatin1Char('\r'));
        ++i;
      } else if (next == QLatin1Char('\\')) {
        result.append(QLatin1Char('\\'));
        ++i;
      } else if (next == QLatin1Char('s')) {
        result.append(QLatin1Char(' '));
        ++i;
      } else {
        result.append(QLatin1Char('\\'));
      }
    } else {
      result.append(value.at(i));
    }
  }
  return result.trimmed();
}

struct DesktopEntryParseState {
  DesktopEntry parsed;
  QString type{QStringLiteral("Application")};
  bool in_desktop_entry{false};
  bool saw_desktop_entry{false};
  bool hidden{false};
  bool no_display{false};
  bool in_action{false};
  DesktopAction current_action;
};

void flushCurrentAction(DesktopEntryParseState* state) {
  if (state->in_action && !state->current_action.name.isEmpty() && !state->current_action.exec.isEmpty()) {
    state->parsed.actions.append(state->current_action);
  }
  state->current_action = {};
  state->in_action = false;
}

// Returns true if the section line signals we should stop parsing key/value pairs
// for [Desktop Entry] (but we keep reading for action sections).
bool handleDesktopSection(const QString& line, DesktopEntryParseState* state) {
  if (!line.startsWith(QLatin1Char('[')) || !line.endsWith(QLatin1Char(']'))) {
    return false;
  }
  flushCurrentAction(state);
  if (line == kDesktopEntrySection) {
    state->in_desktop_entry = true;
    state->saw_desktop_entry = true;
    return false;
  }
  state->in_desktop_entry = false;
  if (line.startsWith(kDesktopActionPrefix)) {
    state->in_action = true;
    state->current_action = {};
  }
  return false;
}

void applyDesktopEntryField(const QString& key, const QString& value, DesktopEntryParseState* state) {
  if (key == QStringLiteral("Type")) {
    state->type = value;
  } else if (key == QStringLiteral("NoDisplay")) {
    state->no_display = desktopEntryTruthy(value);
    state->parsed.no_display = state->no_display;
  } else if (key == QStringLiteral("Hidden")) {
    state->hidden = desktopEntryTruthy(value);
  } else if (key == QStringLiteral("Terminal")) {
    state->parsed.terminal = desktopEntryTruthy(value);
  } else if (key == QStringLiteral("MimeType")) {
    const QStringList parts = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
      const QString trimmed = part.trimmed();
      if (!trimmed.isEmpty()) {
        state->parsed.mime_types.append(trimmed);
      }
    }
  } else {
    for (const DesktopEntryTextField& field : kDesktopEntryTextFields) {
      if (!field.desktop_key.isEmpty() && key == field.desktop_key) {
        state->parsed.*(field.member) = value;
        return;
      }
    }
  }
}

bool isLaunchableDesktopEntry(const DesktopEntryParseState& state, bool include_no_display) {
  return state.saw_desktop_entry && state.type == QStringLiteral("Application") && !state.hidden &&
         (include_no_display || !state.no_display) && !state.parsed.name.isEmpty() && !state.parsed.exec.isEmpty();
}
}  // namespace

bool desktopEntryTruthy(const QString& value) {
  return value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 ||
         value.compare(QStringLiteral("1"), Qt::CaseInsensitive) == 0 ||
         value.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0;
}

QString desktopEntryValue(const QString& line) {
  const qsizetype separator = line.indexOf(QLatin1Char('='));
  if (separator < 0) {
    return {};
  }
  return unescapeDesktopString(line.mid(separator + 1));
}

QString stripDesktopExecFieldCodes(const QString& exec) {
  QString stripped;
  stripped.reserve(exec.size());
  for (int index = 0; index < exec.size(); ++index) {
    const QChar current = exec.at(index);
    if (current != QLatin1Char('%')) {
      stripped.append(current);
      continue;
    }

    if (index + 1 >= exec.size()) {
      stripped.append(current);
      continue;
    }

    const QChar code = exec.at(index + 1);
    if (code == QLatin1Char('%')) {
      stripped.append(QLatin1Char('%'));
      ++index;
    } else if (isFieldCode(code)) {
      ++index;
    } else {
      stripped.append(QLatin1Char('%'));
    }
  }
  return stripped.simplified();
}

DesktopEntryScanner::DesktopEntryScanner() : application_dirs_(defaultApplicationDirs()) {}

DesktopEntryScanner::DesktopEntryScanner(QStringList application_dirs)
    : application_dirs_(std::move(application_dirs)) {}

QStringList DesktopEntryScanner::defaultApplicationDirs() {
  QStringList dirs;

  const QString data_home = QString::fromLocal8Bit(qgetenv("XDG_DATA_HOME")).trimmed().isEmpty()
                                ? QDir::homePath() + QStringLiteral("/.local/share")
                                : QString::fromLocal8Bit(qgetenv("XDG_DATA_HOME")).trimmed();
  addUniqueDir(&dirs, data_home + QStringLiteral("/applications"));

  const QByteArray xdg_data_dirs_env = qgetenv("XDG_DATA_DIRS");
  const QStringList data_dirs =
      xdg_data_dirs_env.isEmpty()
          ? QStringList{QStringLiteral("/usr/local/share"), QStringLiteral("/usr/share")}
          : QString::fromLocal8Bit(xdg_data_dirs_env).split(QLatin1Char(':'), Qt::SkipEmptyParts);
  for (const QString& data_dir : data_dirs) {
    addUniqueDir(&dirs, data_dir + QStringLiteral("/applications"));
  }
  addUniqueDir(&dirs, QStringLiteral("/usr/share/applications"));

  // Flatpak (system and user)
  addUniqueDir(&dirs, QStringLiteral("/var/lib/flatpak/exports/share/applications"));
  addUniqueDir(&dirs, QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share/applications"));

  // Snap
  addUniqueDir(&dirs, QStringLiteral("/var/lib/snapd/desktop/applications"));

  return dirs;
}

bool DesktopEntryScanner::parseDesktopEntryFile(const QString& path, DesktopEntry* entry) {
  return parseDesktopEntryFile(path, entry, false);
}

bool DesktopEntryScanner::parseDesktopEntryFile(const QString& path, DesktopEntry* entry, bool include_no_display) {
  if (entry == nullptr) {
    return false;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }

  DesktopEntryParseState state;
  state.parsed.desktop_file = path;

  QTextStream stream(&file);
  while (!stream.atEnd()) {
    const QString line = stream.readLine().trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
      continue;
    }

    handleDesktopSection(line, &state);
    if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
      continue;
    }

    const qsizetype separator = line.indexOf(QLatin1Char('='));
    if (separator < 0) {
      continue;
    }
    const QString key = line.left(separator).trimmed();
    const QString value = desktopEntryValue(line);

    if (state.in_desktop_entry) {
      applyDesktopEntryField(key, value, &state);
    } else if (state.in_action) {
      if (key == QStringLiteral("Name")) {
        state.current_action.name = value;
      } else if (key == QStringLiteral("Exec")) {
        state.current_action.exec = value;
      }
    }
  }
  flushCurrentAction(&state);

  if (!isLaunchableDesktopEntry(state, include_no_display)) {
    return false;
  }

  *entry = state.parsed;
  return true;
}

QVector<DesktopEntry> DesktopEntryScanner::scan() const { return scanWithDirs().entries; }

QVector<DesktopEntry> DesktopEntryScanner::scanForDefaultApps() const { return scanForDefaultApps(true); }

QVector<DesktopEntry> DesktopEntryScanner::scanForVisibleDefaultApps() const { return scanForDefaultApps(false); }

QVector<DesktopEntry> DesktopEntryScanner::scanForDefaultApps(bool include_no_display) const {
  QVector<DesktopEntry> entries;
  QSet<QString> seen_desktop_ids;

  for (const QString& dir_path : application_dirs_) {
    QDir dir(dir_path);
    if (!dir.exists()) {
      continue;
    }

    QDirIterator iterator(dir_path, {QStringLiteral("*.desktop")}, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
      const QString path = iterator.next();
      const QString desktop_id = QFileInfo(path).fileName();
      if (seen_desktop_ids.contains(desktop_id)) {
        continue;
      }

      DesktopEntry entry;
      if (parseDesktopEntryFile(path, &entry, include_no_display)) {
        entries.append(entry);
        seen_desktop_ids.insert(desktop_id);
      }
    }
  }

  std::ranges::sort(entries, [](const DesktopEntry& left, const DesktopEntry& right) {
    return QString::localeAwareCompare(left.name, right.name) < 0;
  });
  return entries;
}

ScanResult DesktopEntryScanner::scanWithDirs() const {
  QVector<DesktopEntry> entries;
  QSet<QString> seen_desktop_ids;
  QSet<QString> seen_dirs;

  for (const QString& dir_path : application_dirs_) {
    QDir dir(dir_path);
    if (!dir.exists()) {
      continue;
    }
    seen_dirs.insert(QDir::cleanPath(dir_path));

    QDirIterator iterator(dir_path, {QStringLiteral("*.desktop")}, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
      const QString path = iterator.next();
      seen_dirs.insert(QFileInfo(path).absolutePath());

      const QString desktop_id = QFileInfo(path).fileName();
      if (seen_desktop_ids.contains(desktop_id)) {
        continue;
      }

      DesktopEntry entry;
      if (parseDesktopEntryFile(path, &entry)) {
        entries.append(entry);
        seen_desktop_ids.insert(desktop_id);
      }
    }
  }

  std::ranges::sort(entries, [](const DesktopEntry& left, const DesktopEntry& right) {
    return QString::localeAwareCompare(left.name, right.name) < 0;
  });

  return {.entries = std::move(entries), .watched_dirs = QStringList(seen_dirs.begin(), seen_dirs.end())};
}

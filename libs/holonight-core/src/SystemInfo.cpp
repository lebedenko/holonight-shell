#include "SystemInfo.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

#include <array>
#include <initializer_list>
#include <optional>

namespace {

QString unquoteOsReleaseValue(QStringView value) {
  if (value.size() < 2) {
    return value.toString();
  }

  const QChar quote = value.front();
  if ((quote != QLatin1Char('"') && quote != QLatin1Char('\'')) || value.back() != quote) {
    return value.toString();
  }

  QString result;
  result.reserve(value.size() - 2);

  for (qsizetype i = 1; i < value.size() - 1; ++i) {
    const QChar cur = value.at(i);
    if (quote == QLatin1Char('"') && cur == QLatin1Char('\\') && i + 1 < value.size() - 1) {
      const QChar escaped = value.at(++i);
      switch (escaped.unicode()) {
        case '"':
        case '\\':
        case '$':
        case '`':
          result.append(escaped);
          break;
        case 'n':
          result.append(QLatin1Char('\n'));
          break;
        case 't':
          result.append(QLatin1Char('\t'));
          break;
        default:
          result.append(escaped);
          break;
      }
      continue;
    }

    result.append(cur);
  }

  return result;
}

QString firstNonEmpty(const QHash<QString, QString>& values, std::initializer_list<const char*> keys) {
  for (const char* key : keys) {
    const QString value = values.value(QString::fromLatin1(key)).trimmed();
    if (!value.isEmpty()) {
      return value;
    }
  }

  return {};
}

QString normalizeLogoToken(QString value) {
  value = value.toLower().trimmed();
  value.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
  value.replace(QRegularExpression(QStringLiteral("^-+|-+$")), QString());
  return value;
}

void appendUnique(QStringList* values, const QString& value) {
  const QString normalized = normalizeLogoToken(value);
  if (!normalized.isEmpty() && !values->contains(normalized)) {
    values->append(normalized);
  }
}

QString shortDisplayName(QString name, const QString& dist_id) {
  name = name.simplified();
  name.remove(QRegularExpression(QStringLiteral("\\s+GNU/Linux$"), QRegularExpression::CaseInsensitiveOption));
  name.remove(QRegularExpression(QStringLiteral("\\s+Linux$"), QRegularExpression::CaseInsensitiveOption));
  name.remove(QRegularExpression(QStringLiteral("^Linux\\s+"), QRegularExpression::CaseInsensitiveOption));
  name = name.simplified();
  return name.isEmpty() ? dist_id : name;
}

QStringList logoCandidates(const QHash<QString, QString>& values) {
  QStringList candidates;
  const QString logo = values.value(QStringLiteral("LOGO"));
  const QString dist_id = values.value(QStringLiteral("ID"));
  const QString display_name = shortDisplayName(firstNonEmpty(values, {"NAME", "PRETTY_NAME", "ID"}), dist_id);

  appendUnique(&candidates, logo);
  appendUnique(&candidates, QStringLiteral("%1-logo").arg(dist_id));
  appendUnique(&candidates, QStringLiteral("%1linux-logo").arg(dist_id));
  appendUnique(&candidates, QStringLiteral("%1-logo").arg(display_name));
  appendUnique(&candidates, QStringLiteral("%1linux-logo").arg(display_name));
  appendUnique(&candidates, dist_id);

  const QStringList id_like_values =
      values.value(QStringLiteral("ID_LIKE")).split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (const QString& id_like : id_like_values) {
    appendUnique(&candidates, QStringLiteral("%1-logo").arg(id_like));
    appendUnique(&candidates, QStringLiteral("%1linux-logo").arg(id_like));
  }

  return candidates;
}

int extensionScore(const QString& suffix) {
  if (suffix == QLatin1String("svg")) {
    return 0;
  }
  if (suffix == QLatin1String("png")) {
    return 1;
  }
  return 2;
}

bool isSupportedLogoExtension(const QString& suffix) {
  return suffix == QLatin1String("svg") || suffix == QLatin1String("png") || suffix == QLatin1String("xpm");
}

std::optional<int> computeFileCandidateScore(const QString& base_name, const QStringList& candidates) {
  std::optional<int> best;
  for (qsizetype index = 0; index < candidates.size(); ++index) {
    const QString& candidate = candidates.at(index);
    int score = 0;
    if (base_name == candidate) {
      score = static_cast<int>(index);
    } else if (base_name.startsWith(candidate)) {
      score = 100 + static_cast<int>(index);
    } else if (base_name.contains(candidate)) {
      score = 200 + static_cast<int>(index);
    } else {
      continue;
    }
    if (base_name.contains(QStringLiteral("text"))) {
      score += 50;
    }
    if (base_name.contains(QStringLiteral("dark"))) {
      score += 50;
    }
    if (!best.has_value() || score < *best) {
      best = score;
    }
  }
  return best;
}

QString findFuzzyLogoPath(const QStringList& candidates, const QStringList& search_dirs) {
  struct Match {
    QString path;
    int score;
  };
  std::optional<Match> best_match;

  for (const QString& dir_path : search_dirs) {
    const QDir dir(dir_path);
    if (!dir.exists()) {
      continue;
    }
    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Readable, QDir::Name);
    for (const QFileInfo& file : files) {
      const QString suffix = file.suffix().toLower();
      if (!isSupportedLogoExtension(suffix)) {
        continue;
      }
      const QString base_name = normalizeLogoToken(file.completeBaseName());
      const std::optional<int> raw_score = computeFileCandidateScore(base_name, candidates);
      if (!raw_score.has_value()) {
        continue;
      }
      const int score = ((*raw_score) * 10) + extensionScore(suffix);
      if (!best_match.has_value() || score < best_match->score) {
        best_match = Match{.path = file.absoluteFilePath(), .score = score};
      }
    }
  }

  return best_match.has_value() ? best_match->path : QString();
}

const QHash<QString, QString>& distroLogoAliasTable() {
  static const QHash<QString, QString> table{
      // 1:1 matches
      {QStringLiteral("arch"), QStringLiteral("archlinux")},
      {QStringLiteral("ubuntu"), QStringLiteral("ubuntu")},
      {QStringLiteral("debian"), QStringLiteral("debian")},
      {QStringLiteral("fedora"), QStringLiteral("fedora")},
      {QStringLiteral("centos"), QStringLiteral("centos")},
      {QStringLiteral("almalinux"), QStringLiteral("almalinux")},
      {QStringLiteral("gentoo"), QStringLiteral("gentoo")},
      {QStringLiteral("manjaro"), QStringLiteral("manjaro")},
      {QStringLiteral("endeavouros"), QStringLiteral("endeavouros")},
      {QStringLiteral("cachyos"), QStringLiteral("cachyos")},
      {QStringLiteral("linuxmint"), QStringLiteral("linuxmint")},
      {QStringLiteral("zorin"), QStringLiteral("zorin")},
      {QStringLiteral("solus"), QStringLiteral("solus")},
      {QStringLiteral("slackware"), QStringLiteral("slackware")},
      {QStringLiteral("deepin"), QStringLiteral("deepin")},
      {QStringLiteral("devuan"), QStringLiteral("devuan")},
      {QStringLiteral("elementary"), QStringLiteral("elementary")},
      {QStringLiteral("openwrt"), QStringLiteral("openwrt")},
      {QStringLiteral("tails"), QStringLiteral("tails")},
      // renamed / longer-ID aliases (the asymmetry cases the fuzzy matcher gets wrong)
      {QStringLiteral("opensuse-leap"), QStringLiteral("opensuse")},
      {QStringLiteral("opensuse-tumbleweed"), QStringLiteral("opensuse")},
      {QStringLiteral("sles"), QStringLiteral("opensuse")},  // follow-up from spec review: sles gap
      {QStringLiteral("suse"), QStringLiteral("opensuse")},  // ID_LIKE token on SLES/openSUSE derivatives
      {QStringLiteral("fedora-asahi-remix"), QStringLiteral("asahilinux")},
      {QStringLiteral("rhel"), QStringLiteral("redhat")},
      {QStringLiteral("pop"), QStringLiteral("popos")},
      {QStringLiteral("mx"), QStringLiteral("mxlinux")},
      {QStringLiteral("neon"), QStringLiteral("kdeneon")},
      {QStringLiteral("void"), QStringLiteral("voidlinux")},
      {QStringLiteral("alpine"), QStringLiteral("alpinelinux")},
      {QStringLiteral("kali"), QStringLiteral("kalilinux")},
      {QStringLiteral("rocky"), QStringLiteral("rockylinux")},
      {QStringLiteral("garuda"), QStringLiteral("garudalinux")},
      {QStringLiteral("artix"), QStringLiteral("artixlinux")},
      {QStringLiteral("parrot"), QStringLiteral("parrotsecurity")},
      {QStringLiteral("qubes"), QStringLiteral("qubesos")},
      {QStringLiteral("nobara"), QStringLiteral("nobaralinux")},
  };
  return table;
}

}  // namespace

QString mapDistroIdToLogoName(const QString& dist_id) {
  if (dist_id.isEmpty()) {
    return {};
  }
  return distroLogoAliasTable().value(dist_id.trimmed().toLower());
}

QHash<QString, QString> parseOsRelease(const QString& content) {
  QHash<QString, QString> values;

  const QStringList lines = content.split(QLatin1Char('\n'));
  for (QStringView line : lines) {
    line = line.trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
      continue;
    }

    const qsizetype separator = line.indexOf(QLatin1Char('='));
    if (separator <= 0) {
      continue;
    }

    const QString key = line.left(separator).trimmed().toString();
    const QString value = unquoteOsReleaseValue(line.sliced(separator + 1).trimmed());
    values.insert(key, value);
  }

  return values;
}

SystemInfoSnapshot systemInfoFromOsRelease(const QHash<QString, QString>& values) {
  SystemInfoSnapshot snapshot;
  snapshot.name = firstNonEmpty(values, {"PRETTY_NAME", "NAME", "ID"});
  const QString dist_id = values.value(QStringLiteral("ID")).trimmed();
  snapshot.display_name = shortDisplayName(firstNonEmpty(values, {"NAME", "PRETTY_NAME", "ID"}), dist_id);

  const QString logo = values.value(QStringLiteral("LOGO")).trimmed();
  if (!logo.isEmpty()) {
    snapshot.logo_icon_name = logo;
  } else {
    snapshot.logo_icon_name =
        dist_id.isEmpty() ? QStringLiteral("computer-symbolic") : QStringLiteral("%1-logo").arg(dist_id);
  }

  if (snapshot.name.isEmpty()) {
    snapshot.name = QStringLiteral("Linux");
  }
  if (snapshot.display_name.isEmpty()) {
    snapshot.display_name = snapshot.name;
  }

  return snapshot;
}

QString findSystemLogoPath(const QHash<QString, QString>& values, const QStringList& search_dirs) {
  static constexpr std::array extensions = {"svg", "png", "xpm"};

  const QStringList candidates = logoCandidates(values);
  for (const QString& dir_path : search_dirs) {
    const QDir dir(dir_path);
    if (!dir.exists()) {
      continue;
    }
    for (const QString& candidate : candidates) {
      for (const char* extension : extensions) {
        const QString path = dir.filePath(QStringLiteral("%1.%2").arg(candidate, QString::fromLatin1(extension)));
        if (QFileInfo::exists(path)) {
          return path;
        }
      }
    }
  }

  return findFuzzyLogoPath(candidates, search_dirs);
}

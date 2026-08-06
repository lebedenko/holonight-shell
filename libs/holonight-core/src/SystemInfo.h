#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

struct SystemInfoSnapshot {
  QString name;
  QString display_name;
  QString logo_icon_name;
  QString logo_path;
};

[[nodiscard]] QHash<QString, QString> parseOsRelease(const QString& content);
[[nodiscard]] SystemInfoSnapshot systemInfoFromOsRelease(const QHash<QString, QString>& values);
[[nodiscard]] QString findSystemLogoPath(const QHash<QString, QString>& values, const QStringList& search_dirs);

// Maps an os-release ID (or ID_LIKE token) to a bundled assets/linux-logo/<basename>.svg name.
// Returns an empty string if id is not in the table. Distinct from findSystemLogoPath()'s
// pixmaps fuzzy matcher (REQ-C-004) — this is an explicit, exact-match static table because the
// fuzzy matcher's startsWith/contains scoring assumes the search *candidate* is no longer than the
// file basename it's compared against, which breaks for IDs like "opensuse-leap" (14 chars) being
// mapped to the asset basename "opensuse" (8 chars) — see DESIGN.md §4.1.
[[nodiscard]] QString mapDistroIdToLogoName(const QString& dist_id);

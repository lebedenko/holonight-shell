#pragma once

#include <QLatin1StringView>
#include <QString>
#include <QStringList>
#include <QVector>

#include <array>

struct DesktopAction {
  QString name;
  QString exec;
};

struct DesktopEntry {
  QString name;
  QString generic_name;
  QString comment;
  QString exec;
  QString icon;
  QString categories;
  QString path;
  QString desktop_file;
  QString startup_wm_class;
  bool terminal{false};
  bool no_display{false};
  QVector<DesktopAction> actions;
  QStringList mime_types;
};

struct DesktopEntryTextField {
  QLatin1StringView json_key{};
  QLatin1StringView desktop_key{};
  QString DesktopEntry::* member{};
};

inline constexpr std::array kDesktopEntryTextFields{
    DesktopEntryTextField{
        .json_key = QLatin1StringView("name"), .desktop_key = QLatin1StringView("Name"), .member = &DesktopEntry::name},
    DesktopEntryTextField{.json_key = QLatin1StringView("generic_name"),
                          .desktop_key = QLatin1StringView("GenericName"),
                          .member = &DesktopEntry::generic_name},
    DesktopEntryTextField{.json_key = QLatin1StringView("comment"),
                          .desktop_key = QLatin1StringView("Comment"),
                          .member = &DesktopEntry::comment},
    DesktopEntryTextField{
        .json_key = QLatin1StringView("exec"), .desktop_key = QLatin1StringView("Exec"), .member = &DesktopEntry::exec},
    DesktopEntryTextField{
        .json_key = QLatin1StringView("icon"), .desktop_key = QLatin1StringView("Icon"), .member = &DesktopEntry::icon},
    DesktopEntryTextField{.json_key = QLatin1StringView("categories"),
                          .desktop_key = QLatin1StringView("Categories"),
                          .member = &DesktopEntry::categories},
    DesktopEntryTextField{
        .json_key = QLatin1StringView("path"), .desktop_key = QLatin1StringView("Path"), .member = &DesktopEntry::path},
    DesktopEntryTextField{
        .json_key = QLatin1StringView("desktop_file"), .desktop_key = {}, .member = &DesktopEntry::desktop_file},
    DesktopEntryTextField{.json_key = QLatin1StringView("startup_wm_class"),
                          .desktop_key = QLatin1StringView("StartupWMClass"),
                          .member = &DesktopEntry::startup_wm_class},
};

[[nodiscard]] bool desktopEntryTruthy(const QString& value);
[[nodiscard]] QString desktopEntryValue(const QString& line);
[[nodiscard]] QString stripDesktopExecFieldCodes(const QString& exec);

struct ScanResult {
  QVector<DesktopEntry> entries;
  QStringList watched_dirs;
};

class DesktopEntryScanner {
 public:
  DesktopEntryScanner();
  explicit DesktopEntryScanner(QStringList application_dirs);

  [[nodiscard]] QVector<DesktopEntry> scan() const;
  [[nodiscard]] QVector<DesktopEntry> scanForDefaultApps() const;
  [[nodiscard]] QVector<DesktopEntry> scanForVisibleDefaultApps() const;
  [[nodiscard]] ScanResult scanWithDirs() const;
  [[nodiscard]] const QStringList& applicationDirs() const { return application_dirs_; }

  [[nodiscard]] static QStringList defaultApplicationDirs();
  [[nodiscard]] static bool parseDesktopEntryFile(const QString& path, DesktopEntry* entry);
  [[nodiscard]] static bool parseDesktopEntryFile(const QString& path, DesktopEntry* entry, bool include_no_display);

 private:
  [[nodiscard]] QVector<DesktopEntry> scanForDefaultApps(bool include_no_display) const;

  QStringList application_dirs_;
};

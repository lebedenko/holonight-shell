#include "DesktopEntryCache.h"
#include "DesktopEntrySerializer.h"
#include "LauncherCommand.h"
#include "LauncherService.h"
#include "RecentAppsTracker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include <gtest/gtest.h>

namespace {
bool writeFile(const QString& path, const QByteArray& content) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  file.write(content);
  return true;
}

class FakeLauncherBackend final : public LauncherBackend {
 public:
  [[nodiscard]] bool launch(const DesktopEntry& entry) override {
    launched_entries.append(entry);
    return launch_result;
  }

  [[nodiscard]] bool launchExec(const QString& exec, const QString& /*working_dir*/) override {
    launched_execs.append(exec);
    return launch_result;
  }

  QVector<DesktopEntry> launched_entries;
  QStringList launched_execs;
  bool launch_result{true};
};

class EnvVarGuard {
 public:
  explicit EnvVarGuard(const char* name)
      : name_(name), old_value_(qgetenv(name)), had_value_(qEnvironmentVariableIsSet(name)) {}

  ~EnvVarGuard() {
    if (had_value_) {
      qputenv(name_, old_value_);
    } else {
      qunsetenv(name_);
    }
  }

  EnvVarGuard(const EnvVarGuard&) = delete;
  EnvVarGuard& operator=(const EnvVarGuard&) = delete;
  EnvVarGuard(EnvVarGuard&&) = delete;
  EnvVarGuard& operator=(EnvVarGuard&&) = delete;

 private:
  const char* name_;
  QByteArray old_value_;
  bool had_value_{false};
};

DesktopEntry makeEntry(const QString& name, const QString& exec, const QString& desktop_file) {
  return {.name = name,
          .generic_name = QStringLiteral("Utility"),
          .comment = QStringLiteral("Test entry"),
          .exec = exec,
          .icon = QStringLiteral("test-icon"),
          .categories = QStringLiteral("Utility;"),
          .path = QStringLiteral("/tmp"),
          .desktop_file = desktop_file,
          .startup_wm_class = QStringLiteral("org.example.TestApp"),
          .terminal = false,
          .actions = {{.name = QStringLiteral("New Window"), .exec = exec + QStringLiteral(" --new-window")}}};
}

QString launcherDbPath(const QTemporaryDir& dir) { return dir.path() + QStringLiteral("/launcher.db"); }

bool replaceDesktopEntriesWithIncompleteSchema(const QString& db_path) {
  const QString connection_name =
      QStringLiteral("launcher_cache_tamper_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
  bool succeeded = false;
  {
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection_name);
    database.setDatabaseName(db_path);
    if (database.open()) {
      QSqlQuery query(database);
      succeeded = query.exec(QStringLiteral("DROP TABLE desktop_entries")) &&
                  query.exec(QStringLiteral("CREATE TABLE desktop_entries (path TEXT PRIMARY KEY)"));
    }
  }
  QSqlDatabase::removeDatabase(connection_name);
  return succeeded;
}

}  // namespace

TEST(DesktopEntrySerializer, RoundTripsEntryWithActions) {
  DesktopEntry original =
      makeEntry(QStringLiteral("Test App"), QStringLiteral("test-app"), QStringLiteral("/tmp/test-app.desktop"));
  original.no_display = true;
  original.mime_types = {QStringLiteral("application/x-test"), QStringLiteral("text/plain")};

  const QJsonObject json = DesktopEntrySerializer::toJson(original);
  const auto restored = DesktopEntrySerializer::fromJson(json);

  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(restored->name, original.name);
  EXPECT_EQ(restored->generic_name, original.generic_name);
  EXPECT_EQ(restored->comment, original.comment);
  EXPECT_EQ(restored->exec, original.exec);
  EXPECT_EQ(restored->icon, original.icon);
  EXPECT_EQ(restored->categories, original.categories);
  EXPECT_EQ(restored->path, original.path);
  EXPECT_EQ(restored->desktop_file, original.desktop_file);
  EXPECT_EQ(restored->startup_wm_class, original.startup_wm_class);
  EXPECT_EQ(restored->terminal, original.terminal);
  EXPECT_EQ(restored->no_display, original.no_display);
  ASSERT_EQ(restored->actions.size(), 1);
  EXPECT_EQ(restored->actions.first().name, original.actions.first().name);
  EXPECT_EQ(restored->actions.first().exec, original.actions.first().exec);
  EXPECT_EQ(restored->mime_types, original.mime_types);
}

TEST(DesktopEntryCache, PersistsRowsAndRollsBackTransactions) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString db_path = dir.path() + QStringLiteral("/launcher.db");
  const DesktopEntry first =
      makeEntry(QStringLiteral("First"), QStringLiteral("first"), dir.path() + QStringLiteral("/first.desktop"));
  const DesktopEntry second =
      makeEntry(QStringLiteral("Second"), QStringLiteral("second"), dir.path() + QStringLiteral("/second.desktop"));

  DesktopEntryCache cache;
  ASSERT_TRUE(cache.open(db_path));
  EXPECT_TRUE(cache.upsert(first, 100, 10));
  ASSERT_TRUE(cache.beginTransaction());
  EXPECT_TRUE(cache.upsert(second, 200, 20));
  cache.rollbackTransaction();
  cache.close();

  ASSERT_TRUE(cache.open(db_path));
  const QVector<DesktopEntry> entries = cache.loadAll();
  cache.close();

  ASSERT_EQ(entries.size(), 1);
  EXPECT_EQ(entries.first().name, QStringLiteral("First"));
}

TEST(DesktopEntryCache, CommitsTransactionalRows) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString db_path = dir.path() + QStringLiteral("/launcher.db");
  const DesktopEntry entry = makeEntry(QStringLiteral("Committed"), QStringLiteral("committed"),
                                       dir.path() + QStringLiteral("/committed.desktop"));

  DesktopEntryCache cache;
  ASSERT_TRUE(cache.open(db_path));
  ASSERT_TRUE(cache.beginTransaction());
  EXPECT_TRUE(cache.upsert(entry, 100, 10));
  EXPECT_TRUE(cache.commitTransaction());
  cache.close();

  ASSERT_TRUE(cache.open(db_path));
  const QVector<DesktopEntry> entries = cache.loadAll();
  cache.close();

  ASSERT_EQ(entries.size(), 1);
  EXPECT_EQ(entries.first().name, QStringLiteral("Committed"));
}

TEST(DesktopEntryCache, PersistsEntryWithEmptyCategories) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString db_path = dir.path() + QStringLiteral("/launcher.db");
  DesktopEntry entry = makeEntry(QStringLiteral("No Categories"), QStringLiteral("no-categories"),
                                 dir.path() + QStringLiteral("/no-categories.desktop"));
  entry.categories.clear();

  DesktopEntryCache cache;
  ASSERT_TRUE(cache.open(db_path));
  EXPECT_TRUE(cache.upsert(entry, 100, 10));
  cache.close();

  ASSERT_TRUE(cache.open(db_path));
  const QVector<DesktopEntry> entries = cache.loadAll();
  cache.close();

  ASSERT_EQ(entries.size(), 1);
  EXPECT_EQ(entries.first().name, QStringLiteral("No Categories"));
  EXPECT_TRUE(entries.first().categories.isEmpty());
}

TEST(DesktopEntryCache, PersistsMultipleEntriesAndMetadataAcrossReopen) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString db_path = launcherDbPath(dir);
  DesktopEntry first =
      makeEntry(QStringLiteral("First"), QStringLiteral("first"), dir.path() + QStringLiteral("/first.desktop"));
  first.no_display = true;
  const DesktopEntry second =
      makeEntry(QStringLiteral("Second"), QStringLiteral("second"), dir.path() + QStringLiteral("/second.desktop"));

  DesktopEntryCache cache;
  ASSERT_TRUE(cache.open(db_path));
  EXPECT_TRUE(cache.upsert(first, 100, 10));
  EXPECT_TRUE(cache.upsert(second, 200, 20));
  const auto first_metadata = cache.metadata(first.desktop_file);
  const auto second_metadata = cache.metadata(second.desktop_file);
  ASSERT_TRUE(first_metadata.has_value());
  ASSERT_TRUE(second_metadata.has_value());
  EXPECT_EQ(first_metadata->mtime, 100);
  EXPECT_EQ(second_metadata->size, 20);
  cache.close();

  ASSERT_TRUE(cache.open(db_path));
  const QVector<DesktopEntry> entries = cache.loadAll();
  cache.close();

  ASSERT_EQ(entries.size(), 2);
  EXPECT_EQ(entries.at(0).name, QStringLiteral("First"));
  EXPECT_EQ(entries.at(0).generic_name, first.generic_name);
  EXPECT_EQ(entries.at(0).comment, first.comment);
  EXPECT_EQ(entries.at(0).exec, first.exec);
  EXPECT_EQ(entries.at(0).icon, first.icon);
  EXPECT_EQ(entries.at(0).categories, first.categories);
  EXPECT_EQ(entries.at(0).path, first.path);
  EXPECT_EQ(entries.at(0).desktop_file, first.desktop_file);
  EXPECT_EQ(entries.at(0).startup_wm_class, first.startup_wm_class);
  EXPECT_EQ(entries.at(0).no_display, first.no_display);
  EXPECT_EQ(entries.at(1).name, QStringLiteral("Second"));
}

TEST(DesktopEntryCache, RebuildsCurrentVersionIncompleteSchema) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString db_path = launcherDbPath(dir);
  const DesktopEntry entry = makeEntry(QStringLiteral("Recovered"), QStringLiteral("recovered"),
                                       dir.path() + QStringLiteral("/recovered.desktop"));

  DesktopEntryCache cache;
  ASSERT_TRUE(cache.open(db_path));
  cache.close();
  ASSERT_TRUE(replaceDesktopEntriesWithIncompleteSchema(db_path));

  ASSERT_TRUE(cache.open(db_path));
  EXPECT_TRUE(cache.upsert(entry, 100, 10));
  cache.close();

  ASSERT_TRUE(cache.open(db_path));
  const QVector<DesktopEntry> entries = cache.loadAll();
  cache.close();

  ASSERT_EQ(entries.size(), 1);
  EXPECT_EQ(entries.first().name, QStringLiteral("Recovered"));
}

TEST(DesktopEntryCache, CanReopenAfterCorruptedDatabaseIsRemoved) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString db_path = dir.path() + QStringLiteral("/launcher.db");
  ASSERT_TRUE(writeFile(db_path, QByteArrayLiteral("not a sqlite database")));

  DesktopEntryCache cache;
  EXPECT_FALSE(cache.open(db_path));
  ASSERT_TRUE(QFile::remove(db_path));
  ASSERT_TRUE(cache.open(db_path));
  EXPECT_TRUE(cache.upsert(makeEntry(QStringLiteral("Recovered"), QStringLiteral("recovered"),
                                     dir.path() + QStringLiteral("/recovered.desktop")),
                           100, 10));
  cache.close();

  ASSERT_TRUE(cache.open(db_path));
  const QVector<DesktopEntry> entries = cache.loadAll();
  cache.close();

  ASSERT_EQ(entries.size(), 1);
  EXPECT_EQ(entries.first().name, QStringLiteral("Recovered"));
}

TEST(DesktopEntryCache, DestructorClosesOwnedConnection) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QStringList connections_before = QSqlDatabase::connectionNames();
  QString connection_name;

  {
    DesktopEntryCache cache;
    ASSERT_TRUE(cache.open(dir.path() + QStringLiteral("/launcher.db")));
    for (const QString& name : QSqlDatabase::connectionNames()) {
      if (!connections_before.contains(name) && name.startsWith(QStringLiteral("holonight_launcher_"))) {
        connection_name = name;
        break;
      }
    }
    ASSERT_FALSE(connection_name.isEmpty());
    EXPECT_TRUE(QSqlDatabase::contains(connection_name));
  }

  EXPECT_FALSE(QSqlDatabase::contains(connection_name));
}

TEST(DesktopEntryScanner, ParsesVisibleApplicationEntry) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.path() + QStringLiteral("/firefox.desktop");
  ASSERT_TRUE(writeFile(path, QByteArrayLiteral("[Desktop Entry]\n"
                                                "Type=Application\n"
                                                "Name=Firefox\n"
                                                "GenericName=Web Browser\n"
                                                "Comment=Browse the web\n"
                                                "Exec=firefox %u\n"
                                                "Icon=firefox\n"
                                                "Categories=Network;WebBrowser;\n"
                                                "Path=/tmp\n"
                                                "StartupWMClass=org.mozilla.firefox\n"
                                                "Terminal=false\n")));

  DesktopEntry entry;
  ASSERT_TRUE(DesktopEntryScanner::parseDesktopEntryFile(path, &entry));
  EXPECT_EQ(entry.name, QStringLiteral("Firefox"));
  EXPECT_EQ(entry.generic_name, QStringLiteral("Web Browser"));
  EXPECT_EQ(entry.comment, QStringLiteral("Browse the web"));
  EXPECT_EQ(entry.exec, QStringLiteral("firefox %u"));
  EXPECT_EQ(entry.icon, QStringLiteral("firefox"));
  EXPECT_EQ(entry.categories, QStringLiteral("Network;WebBrowser;"));
  EXPECT_EQ(entry.path, QStringLiteral("/tmp"));
  EXPECT_EQ(entry.desktop_file, path);
  EXPECT_EQ(entry.startup_wm_class, QStringLiteral("org.mozilla.firefox"));
  EXPECT_FALSE(entry.terminal);
}

TEST(DesktopEntryScanner, ExcludesHiddenNoDisplayAndNonApplications) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  const QString hidden = dir.path() + QStringLiteral("/hidden.desktop");
  const QString no_display = dir.path() + QStringLiteral("/nodisplay.desktop");
  const QString link = dir.path() + QStringLiteral("/link.desktop");
  ASSERT_TRUE(writeFile(
      hidden, QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=Hidden\nExec=hidden\nHidden=true\n")));
  ASSERT_TRUE(writeFile(
      no_display, QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=NoDisplay\nExec=app\nNoDisplay=true\n")));
  ASSERT_TRUE(writeFile(link, QByteArrayLiteral("[Desktop Entry]\nType=Link\nName=Link\nExec=link\n")));

  DesktopEntry entry;
  EXPECT_FALSE(DesktopEntryScanner::parseDesktopEntryFile(hidden, &entry));
  EXPECT_FALSE(DesktopEntryScanner::parseDesktopEntryFile(no_display, &entry));
  ASSERT_TRUE(DesktopEntryScanner::parseDesktopEntryFile(no_display, &entry, true));
  EXPECT_TRUE(entry.no_display);
  EXPECT_FALSE(DesktopEntryScanner::parseDesktopEntryFile(link, &entry));
}

TEST(DesktopEntryScanner, StripsDesktopExecFieldCodes) {
  EXPECT_EQ(stripDesktopExecFieldCodes(QStringLiteral("firefox %u --name %%profile %X")),
            QStringLiteral("firefox --name %profile %X"));
  EXPECT_EQ(stripDesktopExecFieldCodes(QStringLiteral("app %f %F %u %U %i %c %k")), QStringLiteral("app"));
}

TEST(LauncherCommand, ParsesQuotedArguments) {
  const LauncherCommand command =
      commandForExec(QStringLiteral("app --title \"Two Words\" 'single quoted'"), QStringLiteral("/tmp"));

  ASSERT_TRUE(command.isValid());
  EXPECT_EQ(command.program, QStringLiteral("app"));
  EXPECT_EQ(command.arguments,
            (QStringList{QStringLiteral("--title"), QStringLiteral("Two Words"), QStringLiteral("single quoted")}));
  EXPECT_EQ(command.working_dir, QStringLiteral("/tmp"));
}

TEST(LauncherCommand, ParsesEscapedSpaces) {
  const LauncherCommand command =
      commandForExec(QStringLiteral("app /home/user/My\\ File.txt --label escaped\\ value"), {});

  ASSERT_TRUE(command.isValid());
  EXPECT_EQ(command.program, QStringLiteral("app"));
  EXPECT_EQ(command.arguments, (QStringList{QStringLiteral("/home/user/My File.txt"), QStringLiteral("--label"),
                                            QStringLiteral("escaped value")}));
}

TEST(LauncherCommand, PreservesEnvWrapperArguments) {
  const LauncherCommand command =
      commandForExec(QStringLiteral("env FOO=bar GTK_THEME=Adwaita app --flag"), QStringLiteral("/work"));

  ASSERT_TRUE(command.isValid());
  EXPECT_EQ(command.program, QStringLiteral("env"));
  EXPECT_EQ(command.arguments, (QStringList{QStringLiteral("FOO=bar"), QStringLiteral("GTK_THEME=Adwaita"),
                                            QStringLiteral("app"), QStringLiteral("--flag")}));
  EXPECT_EQ(command.working_dir, QStringLiteral("/work"));
}

TEST(LauncherCommand, StripsDesktopFieldCodesBeforeSplitting) {
  const LauncherCommand command =
      commandForExec(QStringLiteral("app %f %F %u %U --profile %%default %X"), QStringLiteral("/tmp"));

  ASSERT_TRUE(command.isValid());
  EXPECT_EQ(command.program, QStringLiteral("app"));
  EXPECT_EQ(command.arguments,
            (QStringList{QStringLiteral("--profile"), QStringLiteral("%default"), QStringLiteral("%X")}));
}

TEST(LauncherCommand, WrapsTerminalApplicationsWhenTerminalIsAvailable) {
  DesktopEntry entry = makeEntry(QStringLiteral("Terminal App"), QStringLiteral("cli --mode \"safe mode\""),
                                 QStringLiteral("/tmp/terminal.desktop"));
  entry.terminal = true;
  entry.path = QStringLiteral("/workdir");

  const LauncherCommand command = commandForDesktopEntry(entry, QStringLiteral("foot"));

  ASSERT_TRUE(command.isValid());
  EXPECT_EQ(command.program, QStringLiteral("foot"));
  EXPECT_EQ(command.arguments, (QStringList{QStringLiteral("-e"), QStringLiteral("cli"), QStringLiteral("--mode"),
                                            QStringLiteral("safe mode")}));
  EXPECT_EQ(command.working_dir, QStringLiteral("/workdir"));
}

TEST(LauncherCommand, FallsBackToDirectLaunchWhenTerminalIsMissing) {
  DesktopEntry entry =
      makeEntry(QStringLiteral("Terminal App"), QStringLiteral("cli --flag"), QStringLiteral("/tmp/terminal.desktop"));
  entry.terminal = true;

  const LauncherCommand command = commandForDesktopEntry(entry);

  ASSERT_TRUE(command.isValid());
  EXPECT_EQ(command.program, QStringLiteral("cli"));
  EXPECT_EQ(command.arguments, QStringList{QStringLiteral("--flag")});
}

TEST(LauncherCommand, RejectsEmptyOrFieldCodeOnlyExecLines) {
  EXPECT_FALSE(commandForExec(QString{}, {}).isValid());
  EXPECT_FALSE(commandForExec(QStringLiteral("%U %F"), {}).isValid());
}

TEST(DesktopEntryScanner, TrimsKeysAndParsesPathAndTerminal) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.path() + QStringLiteral("/app.desktop");
  ASSERT_TRUE(writeFile(path, QByteArrayLiteral(" [Desktop Entry] \n"
                                                "Type = Application\n"
                                                "Name = TestApp\n"
                                                "Exec = testapp\n"
                                                "Path = /opt/testapp\n"
                                                "Terminal = true\n")));

  DesktopEntry entry;
  ASSERT_TRUE(DesktopEntryScanner::parseDesktopEntryFile(path, &entry));
  EXPECT_EQ(entry.name, QStringLiteral("TestApp"));
  EXPECT_EQ(entry.exec, QStringLiteral("testapp"));
  EXPECT_EQ(entry.path, QStringLiteral("/opt/testapp"));
  EXPECT_TRUE(entry.terminal);
}

TEST(DesktopEntryScanner, HandlesSpecialEscapes) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.path() + QStringLiteral("/escape.desktop");
  ASSERT_TRUE(writeFile(path, QByteArrayLiteral("[Desktop Entry]\n"
                                                "Type=Application\n"
                                                "Name=Escape\\sTest\n"
                                                "Comment=Line1\\nLine2\\tTab\\\\Backslash\n"
                                                "Exec=escape\n")));

  DesktopEntry entry;
  ASSERT_TRUE(DesktopEntryScanner::parseDesktopEntryFile(path, &entry));
  EXPECT_EQ(entry.name, QStringLiteral("Escape Test"));
  EXPECT_EQ(entry.comment, QStringLiteral("Line1\nLine2\tTab\\Backslash"));
}

TEST(LauncherService, ScansSearchesAndRanksApplications) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  const QString applications = dir.path() + QStringLiteral("/applications");
  ASSERT_TRUE(QDir().mkpath(applications));
  ASSERT_TRUE(writeFile(
      applications + QStringLiteral("/firefox.desktop"),
      QByteArrayLiteral(
          "[Desktop Entry]\nType=Application\nName=Firefox\nGenericName=Web Browser\nExec=firefox\nIcon=firefox\n")));
  ASSERT_TRUE(writeFile(
      applications + QStringLiteral("/dev-firefox.desktop"),
      QByteArrayLiteral(
          "[Desktop Entry]\nType=Application\nName=Developer Browser\nComment=Firefox tools\nExec=dev-firefox\n")));

  auto backend = std::make_unique<FakeLauncherBackend>();
  auto* backend_ptr = backend.get();
  LauncherService service(DesktopEntryScanner({applications}), std::move(backend), launcherDbPath(cache_dir));
  service.start();
  QTRY_COMPARE_WITH_TIMEOUT(service.resultCount(), 2, 2000);

  service.setQuery(QStringLiteral("fire"));
  ASSERT_EQ(service.resultCount(), 2);
  EXPECT_EQ(service.selectedIndex(), 0);
  EXPECT_EQ(service.results()
                ->data(service.results()->index(0, 0), static_cast<int>(LauncherModel::Role::NameRole))
                .toString(),
            QStringLiteral("Firefox"));

  EXPECT_TRUE(service.launchSelected());
  ASSERT_EQ(backend_ptr->launched_entries.size(), 1);
  EXPECT_EQ(backend_ptr->launched_entries.first().name, QStringLiteral("Firefox"));

  EXPECT_TRUE(service.launchDesktopFile(applications + QStringLiteral("/dev-firefox.desktop")));
  ASSERT_EQ(backend_ptr->launched_entries.size(), 2);
  EXPECT_EQ(backend_ptr->launched_entries.last().name, QStringLiteral("Developer Browser"));
}

TEST(LauncherService, RescanPreservesSelectedEntryByDesktopFile) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  const QString applications = dir.path() + QStringLiteral("/applications");
  ASSERT_TRUE(QDir().mkpath(applications));
  ASSERT_TRUE(writeFile(applications + QStringLiteral("/alpha.desktop"),
                        QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=Alpha\nExec=alpha\n")));
  ASSERT_TRUE(writeFile(applications + QStringLiteral("/bravo.desktop"),
                        QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=Bravo\nExec=bravo\n")));
  ASSERT_TRUE(writeFile(applications + QStringLiteral("/charlie.desktop"),
                        QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=Charlie\nExec=charlie\n")));

  LauncherService service(DesktopEntryScanner({applications}), std::make_unique<FakeLauncherBackend>(),
                          launcherDbPath(cache_dir));
  service.start();
  QTRY_COMPARE_WITH_TIMEOUT(service.resultCount(), 3, 2000);
  service.setSelectedIndex(1);
  ASSERT_EQ(service.selectedEntryName(), QStringLiteral("Bravo"));

  QSignalSpy entries_updated(&service, &LauncherService::entriesUpdated);
  service.reload();
  ASSERT_TRUE(entries_updated.wait(2000));

  EXPECT_EQ(service.selectedIndex(), 1);
  EXPECT_EQ(service.selectedEntryName(), QStringLiteral("Bravo"));
}

// REQ-F-014: launchSelected() previously always called launch(selected_index_), even when the
// selected row was an action row — dispatching the app's default Exec instead of the action's.
TEST(LauncherService, LaunchSelectedDispatchesToActionWhenActionRowSelected) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  const QString applications = dir.path() + QStringLiteral("/applications");
  ASSERT_TRUE(QDir().mkpath(applications));
  ASSERT_TRUE(writeFile(applications + QStringLiteral("/editor.desktop"),
                        QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=Editor\nExec=editor\n"
                                          "Actions=NewWindow;\n\n"
                                          "[Desktop Action NewWindow]\nName=New Window\nExec=editor --new-window\n")));

  auto backend = std::make_unique<FakeLauncherBackend>();
  auto* backend_ptr = backend.get();
  LauncherService service(DesktopEntryScanner({applications}), std::move(backend), launcherDbPath(cache_dir));
  service.start();
  QTRY_COMPARE_WITH_TIMEOUT(service.resultCount(), 1, 2000);

  service.setQuery(QStringLiteral("editor"));
  QTRY_COMPARE_WITH_TIMEOUT(service.actionResultCount(), 1, 2000);
  ASSERT_EQ(service.resultCount(), 2);  // app row + its one action row
  ASSERT_TRUE(service.results()
                  ->data(service.results()->index(1, 0), static_cast<int>(LauncherModel::Role::IsActionRole))
                  .toBool());

  service.setSelectedIndex(1);
  EXPECT_TRUE(service.launchSelected());

  EXPECT_TRUE(backend_ptr->launched_entries.isEmpty());
  ASSERT_EQ(backend_ptr->launched_execs.size(), 1);
  EXPECT_EQ(backend_ptr->launched_execs.first(), QStringLiteral("editor --new-window"));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(LauncherService, FiltersDefaultAppCandidatesByMimeAndCategory) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());

  ASSERT_TRUE(writeFile(dir.path() + QStringLiteral("/firefox.desktop"),
                        QByteArrayLiteral("[Desktop Entry]\n"
                                          "Type=Application\n"
                                          "Name=Firefox\n"
                                          "Exec=firefox\n"
                                          "Categories=Network;WebBrowser;\n"
                                          "MimeType=text/html;image/png;video/mp4;\n")));
  ASSERT_TRUE(
      writeFile(dir.path() + QStringLiteral("/geany.desktop"), QByteArrayLiteral("[Desktop Entry]\n"
                                                                                 "Type=Application\n"
                                                                                 "Name=Geany\n"
                                                                                 "Exec=geany\n"
                                                                                 "Categories=Development;TextEditor;\n"
                                                                                 "MimeType=text/html;text/plain;\n")));
  ASSERT_TRUE(writeFile(dir.path() + QStringLiteral("/gwenview.desktop"),
                        QByteArrayLiteral("[Desktop Entry]\n"
                                          "Type=Application\n"
                                          "Name=Gwenview\n"
                                          "Exec=gwenview\n"
                                          "Categories=Graphics;Viewer;\n"
                                          "MimeType=image/png;image/jpeg;\n")));
  ASSERT_TRUE(
      writeFile(dir.path() + QStringLiteral("/vlc.desktop"), QByteArrayLiteral("[Desktop Entry]\n"
                                                                               "Type=Application\n"
                                                                               "Name=VLC\n"
                                                                               "Exec=vlc\n"
                                                                               "Categories=AudioVideo;Player;\n"
                                                                               "MimeType=video/mp4;image/png;\n")));
  ASSERT_TRUE(writeFile(dir.path() + QStringLiteral("/imv.desktop"),
                        QByteArrayLiteral("[Desktop Entry]\n"
                                          "Type=Application\n"
                                          "Name=imv\n"
                                          "Exec=imv %U\n"
                                          "NoDisplay=true\n"
                                          "Categories=Graphics;Viewer;\n"
                                          "MimeType=image/png;image/jpeg;image/jpg;image/x-png;\n")));
  ASSERT_TRUE(writeFile(dir.path() + QStringLiteral("/hidden-image.desktop"),
                        QByteArrayLiteral("[Desktop Entry]\n"
                                          "Type=Application\n"
                                          "Name=Hidden Image App\n"
                                          "Exec=hidden-image\n"
                                          "Hidden=true\n"
                                          "Categories=Graphics;Viewer;\n"
                                          "MimeType=image/png;image/jpeg;\n")));

  LauncherService service(DesktopEntryScanner({dir.path()}), std::make_unique<FakeLauncherBackend>(),
                          launcherDbPath(cache_dir));
  service.start();
  QTRY_COMPARE_WITH_TIMEOUT(service.resultCount(), 4, 2000);

  const QVariantList browsers = service.entriesForMimeTypesAndCategories(
      {QStringLiteral("text/html"), QStringLiteral("x-scheme-handler/http")}, {QStringLiteral("WebBrowser")});
  ASSERT_EQ(browsers.size(), 1);
  EXPECT_EQ(browsers.first().toMap().value(QStringLiteral("desktopFile")).toString(),
            QStringLiteral("firefox.desktop"));

  const QVariantList image_viewers = service.entriesForMimeTypesAndCategories(
      {QStringLiteral("image/png"), QStringLiteral("image/jpeg")},
      {QStringLiteral("Graphics"), QStringLiteral("Photography"), QStringLiteral("Viewer")});
  ASSERT_EQ(image_viewers.size(), 1);
  EXPECT_EQ(image_viewers.first().toMap().value(QStringLiteral("desktopFile")).toString(),
            QStringLiteral("gwenview.desktop"));

  const QVariantList picker_image_viewers = service.defaultAppEntriesForMimeTypesAndCategories(
      {QStringLiteral("image/png"), QStringLiteral("image/jpeg")},
      {QStringLiteral("Graphics"), QStringLiteral("Photography"), QStringLiteral("Viewer")});
  ASSERT_EQ(picker_image_viewers.size(), 1);
  QStringList picker_desktop_files;
  for (const QVariant& item : picker_image_viewers) {
    picker_desktop_files.append(item.toMap().value(QStringLiteral("desktopFile")).toString());
  }
  EXPECT_TRUE(picker_desktop_files.contains(QStringLiteral("gwenview.desktop")));
  EXPECT_FALSE(picker_desktop_files.contains(QStringLiteral("imv.desktop")));

  const QVariantList video_players = service.entriesForMimeTypesAndCategories(
      {QStringLiteral("video/mp4")}, {QStringLiteral("AudioVideo"), QStringLiteral("Player")});
  ASSERT_EQ(video_players.size(), 1);
  EXPECT_EQ(video_players.first().toMap().value(QStringLiteral("desktopFile")).toString(),
            QStringLiteral("vlc.desktop"));
}

TEST(LauncherService, ExcludesNoDisplayDuplicateFromDefaultAppCandidates) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  ASSERT_TRUE(writeFile(dir.path() + QStringLiteral("/google-chrome.desktop"),
                        QByteArrayLiteral("[Desktop Entry]\n"
                                          "Type=Application\n"
                                          "Name=Google Chrome\n"
                                          "Exec=/usr/bin/google-chrome-stable %U\n"
                                          "Categories=Network;WebBrowser;\n"
                                          "MimeType=x-scheme-handler/http;x-scheme-handler/https;\n")));
  ASSERT_TRUE(writeFile(dir.path() + QStringLiteral("/com.google.Chrome.desktop"),
                        QByteArrayLiteral("[Desktop Entry]\n"
                                          "Type=Application\n"
                                          "Name=Google Chrome\n"
                                          "Exec=/usr/bin/google-chrome-stable %U\n"
                                          "NoDisplay=true\n"
                                          "Categories=Network;WebBrowser;\n"
                                          "MimeType=x-scheme-handler/http;x-scheme-handler/https;\n")));

  LauncherService service(DesktopEntryScanner({dir.path()}), std::make_unique<FakeLauncherBackend>(),
                          launcherDbPath(cache_dir));
  const QVariantList candidates = service.defaultAppEntriesForMimeTypesAndCategories(
      {QStringLiteral("x-scheme-handler/http")}, {QStringLiteral("WebBrowser")});

  ASSERT_EQ(candidates.size(), 1);
  EXPECT_EQ(candidates.first().toMap().value(QStringLiteral("desktopFile")).toString(),
            QStringLiteral("google-chrome.desktop"));
}

TEST(LauncherService, NoDisplayAppsStayOutOfLauncherSearch) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  ASSERT_TRUE(writeFile(dir.path() + QStringLiteral("/visible.desktop"), QByteArrayLiteral("[Desktop Entry]\n"
                                                                                           "Type=Application\n"
                                                                                           "Name=Visible App\n"
                                                                                           "Exec=visible\n")));
  ASSERT_TRUE(
      writeFile(dir.path() + QStringLiteral("/nodisplay.desktop"), QByteArrayLiteral("[Desktop Entry]\n"
                                                                                     "Type=Application\n"
                                                                                     "Name=Hidden Picker Handler\n"
                                                                                     "Exec=hidden-picker\n"
                                                                                     "NoDisplay=true\n"
                                                                                     "MimeType=image/png;\n")));

  LauncherService service(DesktopEntryScanner({dir.path()}), std::make_unique<FakeLauncherBackend>(),
                          launcherDbPath(cache_dir));
  service.start();
  QTRY_COMPARE_WITH_TIMEOUT(service.resultCount(), 1, 2000);

  service.setQuery(QStringLiteral("hidden"));
  EXPECT_EQ(service.resultCount(), 0);
}

TEST(LauncherService, SelectionClampsToResultBounds) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  ASSERT_TRUE(writeFile(dir.path() + QStringLiteral("/files.desktop"),
                        QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=Files\nExec=files\n")));

  LauncherService service(DesktopEntryScanner({dir.path()}), std::make_unique<FakeLauncherBackend>(),
                          launcherDbPath(cache_dir));
  QSignalSpy spy(&service, &LauncherService::resultCountChanged);
  service.start();
  ASSERT_TRUE(spy.wait(2000));
  service.setQuery(QStringLiteral("files"));

  service.moveSelection(10);
  EXPECT_EQ(service.selectedIndex(), 0);
  service.moveSelection(-10);
  EXPECT_EQ(service.selectedIndex(), 0);

  service.setQuery(QStringLiteral("missing"));
  EXPECT_EQ(service.selectedIndex(), -1);
}

TEST(LauncherService, SelectionWrapsAround) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  ASSERT_TRUE(writeFile(dir.path() + QStringLiteral("/app1.desktop"),
                        QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=App1\nExec=app1\n")));
  ASSERT_TRUE(writeFile(dir.path() + QStringLiteral("/app2.desktop"),
                        QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=App2\nExec=app2\n")));

  LauncherService service(DesktopEntryScanner({dir.path()}), std::make_unique<FakeLauncherBackend>(),
                          launcherDbPath(cache_dir));
  QSignalSpy spy(&service, &LauncherService::resultCountChanged);
  service.start();
  ASSERT_TRUE(spy.wait(2000));

  service.setQuery(QStringLiteral("app"));
  ASSERT_EQ(service.resultCount(), 2);
  EXPECT_EQ(service.selectedIndex(), 0);

  // Wrap forward
  service.moveSelection(1);
  EXPECT_EQ(service.selectedIndex(), 1);
  service.moveSelection(1);
  EXPECT_EQ(service.selectedIndex(), 0);

  // Wrap backward
  service.moveSelection(-1);
  EXPECT_EQ(service.selectedIndex(), 1);
}

TEST(LauncherService, RebuildsCorruptedLauncherCacheDatabase) {
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  const QString db_path = cache_dir.path() + QStringLiteral("/holonight-shell/launcher.db");
  ASSERT_TRUE(QDir().mkpath(QFileInfo(db_path).absolutePath()));
  QFile::remove(db_path);
  ASSERT_TRUE(writeFile(db_path, QByteArrayLiteral("not a sqlite database")));

  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString desktop_file = dir.path() + QStringLiteral("/cache-test.desktop");
  ASSERT_TRUE(writeFile(desktop_file,
                        QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=Cache Test\nExec=cache-test\n")));

  LauncherService service(DesktopEntryScanner({dir.path()}), std::make_unique<FakeLauncherBackend>(), db_path);
  service.start();
  QTRY_COMPARE_WITH_TIMEOUT(service.resultCount(), 1, 2000);
  QTRY_COMPARE_WITH_TIMEOUT(service.selectedEntryName(), QStringLiteral("Cache Test"), 2000);
}

TEST(LauncherService, PrunesRecentEntriesForRemovedDesktopFiles) {
  EnvVarGuard cache_home_guard("XDG_CACHE_HOME");
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  qputenv("XDG_CACHE_HOME", cache_dir.path().toUtf8());

  QTemporaryDir apps_dir;
  ASSERT_TRUE(apps_dir.isValid());
  const QString desktop_file = apps_dir.path() + QStringLiteral("/removed.desktop");
  ASSERT_TRUE(writeFile(desktop_file,
                        QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=Removed App\nExec=removed-app\n")));

  RecentAppsTracker tracker;
  auto backend = std::make_unique<FakeLauncherBackend>();
  LauncherService service(DesktopEntryScanner({apps_dir.path()}), std::move(backend),
                          cache_dir.path() + QStringLiteral("/launcher.db"), &tracker);
  service.start();
  QTRY_COMPARE_WITH_TIMEOUT(service.resultCount(), 1, 2000);

  ASSERT_TRUE(service.launchSelected());
  ASSERT_EQ(tracker.recentEntries(5).size(), 1);

  ASSERT_TRUE(QFile::remove(desktop_file));
  service.reload();
  QTRY_COMPARE_WITH_TIMEOUT(service.resultCount(), 0, 2000);
  EXPECT_TRUE(tracker.recentEntries(5).isEmpty());
}

TEST(LauncherService, RecordsRecentLaunchThroughInjectedTracker) {
  EnvVarGuard cache_home_guard("XDG_CACHE_HOME");
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  qputenv("XDG_CACHE_HOME", cache_dir.path().toUtf8());

  QTemporaryDir apps_dir;
  ASSERT_TRUE(apps_dir.isValid());
  const QString desktop_file = apps_dir.path() + QStringLiteral("/recorded.desktop");
  ASSERT_TRUE(writeFile(desktop_file,
                        QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=Recorded App\nExec=recorded\n")));

  RecentAppsTracker tracker;
  LauncherService service(DesktopEntryScanner({apps_dir.path()}), std::make_unique<FakeLauncherBackend>(),
                          cache_dir.path() + QStringLiteral("/launcher.db"), &tracker);
  service.start();
  QTRY_COMPARE_WITH_TIMEOUT(service.resultCount(), 1, 2000);

  ASSERT_TRUE(service.launchSelected());

  const QVariantList entries = tracker.recentEntries(5);
  ASSERT_EQ(entries.size(), 1);
  EXPECT_EQ(entries.first().toMap().value(QStringLiteral("desktopFile")).toString(), desktop_file);
}

TEST(LauncherService, ExplicitNullRecentTrackerDisablesRecording) {
  EnvVarGuard cache_home_guard("XDG_CACHE_HOME");
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  qputenv("XDG_CACHE_HOME", cache_dir.path().toUtf8());

  QTemporaryDir apps_dir;
  ASSERT_TRUE(apps_dir.isValid());
  const QString desktop_file = apps_dir.path() + QStringLiteral("/disabled.desktop");
  ASSERT_TRUE(writeFile(desktop_file,
                        QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=Disabled App\nExec=disabled\n")));

  RecentAppsTracker global_tracker;
  LauncherService service(DesktopEntryScanner({apps_dir.path()}), std::make_unique<FakeLauncherBackend>(),
                          cache_dir.path() + QStringLiteral("/launcher.db"), static_cast<RecentAppsTracker*>(nullptr));
  service.start();
  QTRY_COMPARE_WITH_TIMEOUT(service.resultCount(), 1, 2000);

  ASSERT_TRUE(service.launchSelected());

  EXPECT_TRUE(global_tracker.recentEntries(5).isEmpty());
}

TEST(LauncherService, EmitsEntriesUpdatedAfterValidatorCycle) {
  QTemporaryDir apps_dir;
  ASSERT_TRUE(apps_dir.isValid());
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  ASSERT_TRUE(writeFile(apps_dir.path() + QStringLiteral("/myapp.desktop"),
                        QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=MyApp\nExec=myapp\n")));

  LauncherService service(DesktopEntryScanner({apps_dir.path()}), std::make_unique<FakeLauncherBackend>(),
                          launcherDbPath(cache_dir));
  QSignalSpy spy(&service, &LauncherService::entriesUpdated);
  service.start();

  ASSERT_TRUE(spy.wait(2000));
  EXPECT_GE(spy.count(), 1);
}

// REQ-F-014/REQ-F-015: defaultAppEntriesFor*() must cache scanForDefaultApps() results rather than
// re-scanning the filesystem on every call, invalidating only on onModelEntriesReset().
// DesktopEntryScanner is a concrete class with no virtual seam (by design — see DESIGN.md Item 6,
// which explicitly rejects adding one), so this proves cache hit/miss/invalidation by observing a
// real filesystem side effect instead of a call-count spy: deleting the backing .desktop file
// between calls. A cache hit returns stale (pre-deletion) data; a real rescan reflects the
// deletion.
TEST(LauncherService, DefaultAppEntriesCachesScanResultUntilIndexRebuild) {
  QTemporaryDir apps_dir;
  ASSERT_TRUE(apps_dir.isValid());
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  const QString desktop_file = apps_dir.path() + QStringLiteral("/default-cache-test.desktop");
  ASSERT_TRUE(writeFile(desktop_file, QByteArrayLiteral("[Desktop Entry]\nType=Application\nName=Cache Probe\n"
                                                        "Exec=cache-probe\nMimeType=text/plain;\n")));

  LauncherService service(DesktopEntryScanner({apps_dir.path()}), std::make_unique<FakeLauncherBackend>(),
                          launcherDbPath(cache_dir));

  const QStringList mime_types{QStringLiteral("text/plain")};
  const QVariantList first_call = service.defaultAppEntriesForMimeTypes(mime_types);
  ASSERT_EQ(first_call.size(), 1);

  ASSERT_TRUE(QFile::remove(desktop_file));

  // Cache hit: scanForDefaultApps() is not re-invoked, so the stale (pre-deletion) result persists.
  const QVariantList second_call = service.defaultAppEntriesForMimeTypes(mime_types);
  EXPECT_EQ(second_call.size(), 1);

  QSignalSpy spy(&service, &LauncherService::entriesUpdated);
  service.reload();
  ASSERT_TRUE(spy.wait(2000));

  // Cache invalidated by onModelEntriesReset() (triggered via the reload cycle's modelReset):
  // the next call re-scans and reflects the deleted file.
  const QVariantList third_call = service.defaultAppEntriesForMimeTypes(mime_types);
  EXPECT_EQ(third_call.size(), 0);
}

// REQ-F-008/REQ-NF-002: lookup preserves the previous found/not-found behavior while using the
// model-owned desktop-file index.
TEST(LauncherModel, RoleNamesExposeQmlContract) {
  LauncherModel model;
  const QHash<int, QByteArray> roles = model.roleNames();

  EXPECT_EQ(roles.value(static_cast<int>(LauncherModel::Role::NameRole)), "name");
  EXPECT_EQ(roles.value(static_cast<int>(LauncherModel::Role::SubtitleRole)), "subtitle");
  EXPECT_EQ(roles.value(static_cast<int>(LauncherModel::Role::IconNameRole)), "iconName");
  EXPECT_EQ(roles.value(static_cast<int>(LauncherModel::Role::ExecRole)), "exec");
  EXPECT_EQ(roles.value(static_cast<int>(LauncherModel::Role::DesktopFileRole)), "desktopFile");
  EXPECT_EQ(roles.value(static_cast<int>(LauncherModel::Role::CategoriesRole)), "categories");
  EXPECT_EQ(roles.value(static_cast<int>(LauncherModel::Role::TerminalRole)), "terminal");
  EXPECT_EQ(roles.value(static_cast<int>(LauncherModel::Role::ActionsRole)), "actions");
  EXPECT_EQ(roles.value(static_cast<int>(LauncherModel::Role::IsActionRole)), "isAction");
  EXPECT_EQ(roles.value(static_cast<int>(LauncherModel::Role::ActionParentRole)), "actionParent");
  EXPECT_EQ(roles.value(static_cast<int>(LauncherModel::Role::ActionExecRole)), "actionExec");
  EXPECT_EQ(roles.value(static_cast<int>(LauncherModel::Role::ActionIndexRole)), "actionIndex");
  EXPECT_EQ(roles.value(static_cast<int>(LauncherModel::Role::MappedCategoryRole)), "mappedCategory");
  EXPECT_EQ(roles.value(static_cast<int>(LauncherModel::Role::IsActionSectionRole)), "isActionSection");
  EXPECT_EQ(roles.value(static_cast<int>(LauncherModel::Role::MimeTypesRole)), "mimeTypes");
  EXPECT_EQ(roles.size(), 15);
}

TEST(LauncherModel, UnknownRoleReturnsEmptyValue) {
  LauncherModel model;
  model.setEntries({makeEntry(QStringLiteral("Entry"), QStringLiteral("entry"), QStringLiteral("/tmp/entry.desktop"))});

  EXPECT_FALSE(model.data(model.index(0, 0), Qt::UserRole + 1000).isValid());
}

TEST(LauncherModel, ResultRowsResolveCurrentEntriesAfterReplacement) {
  LauncherModel model;
  model.setEntries(
      {makeEntry(QStringLiteral("Old Entry"), QStringLiteral("old-entry"), QStringLiteral("/tmp/old.desktop"))});
  model.setQuery(QStringLiteral("old"));

  ASSERT_EQ(model.rowCount(), 2);
  ASSERT_NE(model.entryAt(0), nullptr);
  EXPECT_EQ(model.entryAt(0)->desktop_file, QStringLiteral("/tmp/old.desktop"));
  EXPECT_TRUE(model.isActionRow(1));
  EXPECT_EQ(model.data(model.index(1, 0), static_cast<int>(LauncherModel::Role::ActionExecRole)).toString(),
            QStringLiteral("old-entry --new-window"));

  model.setEntries(
      {makeEntry(QStringLiteral("New Entry"), QStringLiteral("new-entry"), QStringLiteral("/tmp/new.desktop"))});
  model.setQuery(QStringLiteral("new"));

  ASSERT_EQ(model.rowCount(), 2);
  ASSERT_NE(model.entryAt(0), nullptr);
  EXPECT_EQ(model.entryAt(0)->desktop_file, QStringLiteral("/tmp/new.desktop"));
  EXPECT_TRUE(model.isActionRow(1));
  EXPECT_EQ(model.data(model.index(1, 0), static_cast<int>(LauncherModel::Role::ActionExecRole)).toString(),
            QStringLiteral("new-entry --new-window"));
}

TEST(LauncherModel, FindEntryByDesktopFileReturnsMatchingEntry) {
  LauncherModel model;
  model.setEntries(
      {makeEntry(QStringLiteral("First"), QStringLiteral("first"), QStringLiteral("/tmp/first.desktop")),
       makeEntry(QStringLiteral("Second"), QStringLiteral("second"), QStringLiteral("/tmp/second.desktop"))});

  const DesktopEntry* entry = model.findEntryByDesktopFile(QStringLiteral("/tmp/second.desktop"));
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->name, QStringLiteral("Second"));

  EXPECT_EQ(model.findEntryByDesktopFile(QStringLiteral("/tmp/does-not-exist.desktop")), nullptr);
}

// The previous linear implementation returned the first matching entry. Preserve that behavior
// even when callers provide duplicate desktop-file paths.
TEST(LauncherModel, FindEntryByDesktopFileReturnsFirstDuplicate) {
  LauncherModel model;
  model.setEntries(
      {makeEntry(QStringLiteral("First"), QStringLiteral("first"), QStringLiteral("/tmp/shared.desktop")),
       makeEntry(QStringLiteral("Second"), QStringLiteral("second"), QStringLiteral("/tmp/shared.desktop"))});

  const DesktopEntry* entry = model.findEntryByDesktopFile(QStringLiteral("/tmp/shared.desktop"));
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->name, QStringLiteral("First"));
}

// REQ-F-010: desktop_file_index_ must be rebuilt every time setEntries() replaces entries_, so
// lookups reflect the current entry set rather than a stale index from a prior call.
TEST(LauncherModel, IndexConsistencyAfterSetEntries) {
  LauncherModel model;

  QVector<DesktopEntry> initial;
  initial.append(makeEntry(QStringLiteral("Kept"), QStringLiteral("kept"), QStringLiteral("/tmp/kept.desktop")));
  initial.append(
      makeEntry(QStringLiteral("Removed"), QStringLiteral("removed"), QStringLiteral("/tmp/removed.desktop")));
  model.setEntries(initial);

  ASSERT_NE(model.findEntryByDesktopFile(QStringLiteral("/tmp/kept.desktop")), nullptr);
  ASSERT_NE(model.findEntryByDesktopFile(QStringLiteral("/tmp/removed.desktop")), nullptr);

  QVector<DesktopEntry> updated;
  updated.append(makeEntry(QStringLiteral("Kept"), QStringLiteral("kept"), QStringLiteral("/tmp/kept.desktop")));
  updated.append(makeEntry(QStringLiteral("Added"), QStringLiteral("added"), QStringLiteral("/tmp/added.desktop")));
  model.setEntries(updated);

  const DesktopEntry* kept = model.findEntryByDesktopFile(QStringLiteral("/tmp/kept.desktop"));
  ASSERT_NE(kept, nullptr);
  EXPECT_EQ(kept->name, QStringLiteral("Kept"));

  const DesktopEntry* added = model.findEntryByDesktopFile(QStringLiteral("/tmp/added.desktop"));
  ASSERT_NE(added, nullptr);
  EXPECT_EQ(added->name, QStringLiteral("Added"));

  EXPECT_EQ(model.findEntryByDesktopFile(QStringLiteral("/tmp/removed.desktop")), nullptr);
}

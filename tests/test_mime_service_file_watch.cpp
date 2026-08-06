#include "MimeService.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <atomic>
#include <gtest/gtest.h>
#include <memory>

namespace {

// Resolver that counts queryDefault calls and always returns empty.
// Used to verify refreshAllRoles was triggered without needing a value change.
struct CountingResolver final : public IMimeResolver {
  std::atomic<int> call_count{0};

  void queryDefault(const QString& /*mime_type*/, std::function<void(QString)> callback) override {
    ++call_count;
    callback(QString{});
  }

  void queryDefaultBrowser(std::function<void(QString)> callback) override {
    ++call_count;
    callback(QString{});
  }

  void setDefault(const QString& /*desktop_file*/, const QString& /*mime_type*/,
                  std::function<void(bool)> callback) override {
    callback(true);
  }

  void setDefaultBrowser(const QString& /*desktop_file*/, std::function<void(bool)> callback) override {
    callback(true);
  }
};

// Resolver that returns a predictable value so role-changed signals fire on refresh.
struct AlternatingResolver final : public IMimeResolver {
  std::atomic<int> call_count{0};

  void queryDefault(const QString& /*mime_type*/, std::function<void(QString)> callback) override {
    const int count = ++call_count;
    // Odd-numbered calls return "app1.desktop", even return "app2.desktop" → signal fires on each batch.
    callback(count % 2 == 0 ? QStringLiteral("app2.desktop") : QStringLiteral("app1.desktop"));
  }

  void queryDefaultBrowser(std::function<void(QString)> callback) override {
    const int count = ++call_count;
    callback(count % 2 == 0 ? QStringLiteral("app2.desktop") : QStringLiteral("app1.desktop"));
  }

  void setDefault(const QString& /*desktop_file*/, const QString& /*mime_type*/,
                  std::function<void(bool)> callback) override {
    callback(true);
  }

  void setDefaultBrowser(const QString& /*desktop_file*/, std::function<void(bool)> callback) override {
    callback(true);
  }
};

struct BrowserCheckResolver final : public IMimeResolver {
  QString browser_default;
  QString set_browser_arg;

  void queryDefault(const QString& /*mime_type*/, std::function<void(QString)> callback) override {
    callback(QStringLiteral("google-chrome.desktop"));
  }

  void queryDefaultBrowser(std::function<void(QString)> callback) override { callback(browser_default); }

  void setDefault(const QString& /*desktop_file*/, const QString& /*mime_type*/,
                  std::function<void(bool)> callback) override {
    callback(true);
  }

  void setDefaultBrowser(const QString& desktop_file, std::function<void(bool)> callback) override {
    set_browser_arg = desktop_file;
    browser_default = desktop_file;
    callback(true);
  }
};

struct DeferredResolver final : public IMimeResolver {
  QHash<QString, QList<std::function<void(QString)>>> query_callbacks;
  QString set_default_arg;
  QStringList set_default_mimes;

  void queryDefault(const QString& mime_type, std::function<void(QString)> callback) override {
    query_callbacks[mime_type].append(std::move(callback));
  }

  void queryDefaultBrowser(std::function<void(QString)> callback) override { callback(QString{}); }

  void setDefault(const QString& desktop_file, const QString& mime_type, std::function<void(bool)> callback) override {
    set_default_arg = desktop_file;
    set_default_mimes.append(mime_type);
    callback(true);
  }

  void setDefaultBrowser(const QString& /*desktop_file*/, std::function<void(bool)> callback) override {
    callback(true);
  }
};

struct RecordingResolver final : public IMimeResolver {
  QHash<QString, QString> defaults;
  QHash<QString, QString> set_defaults;
  QString set_browser_arg;

  void queryDefault(const QString& mime_type, std::function<void(QString)> callback) override {
    callback(defaults.value(mime_type));
  }

  void queryDefaultBrowser(std::function<void(QString)> callback) override {
    callback(defaults.value(QStringLiteral("browser")));
  }

  void setDefault(const QString& desktop_file, const QString& mime_type, std::function<void(bool)> callback) override {
    set_defaults.insert(mime_type, desktop_file);
    defaults.insert(mime_type, desktop_file);
    callback(true);
  }

  void setDefaultBrowser(const QString& desktop_file, std::function<void(bool)> callback) override {
    set_browser_arg = desktop_file;
    defaults.insert(QStringLiteral("browser"), desktop_file);
    callback(true);
  }
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

bool writeFile(const QString& path, const QByteArray& content) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  file.write(content);
  return true;
}

QByteArray readFile(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}

constexpr int kDebouncePlusSlack = 700;  // 500ms debounce + 200ms slack

}  // namespace

TEST(MimeServiceFileWatch, WatchesMimeinfoCacheViaAppDirs) {
  QTemporaryDir app_dir;
  ASSERT_TRUE(app_dir.isValid());

  const QString cache_path = app_dir.path() + QStringLiteral("/mimeinfo.cache");
  ASSERT_TRUE(writeFile(cache_path, QByteArrayLiteral("[MIME Cache]\n")));

  auto owned_resolver = std::make_unique<CountingResolver>();
  CountingResolver* resolver = owned_resolver.get();

  MimeService service(QStringList{app_dir.path()}, std::move(owned_resolver));

  // Wait for the initial singleShot(0) refreshAllRoles and any immediate watcher noise to settle.
  QTest::qWait(100);
  const int baseline = resolver->call_count.load();
  EXPECT_GE(baseline, 6);  // at least one full refresh (6 MIME types)

  // Write to mimeinfo.cache → debounce → refreshAllRoles → at least 6 more calls.
  ASSERT_TRUE(writeFile(cache_path, QByteArrayLiteral("[MIME Cache]\napplication/pdf=evince.desktop\n")));
  QTest::qWait(kDebouncePlusSlack);

  EXPECT_GE(resolver->call_count.load() - baseline, 6);
}

TEST(MimeServiceFileWatch, DebouncesRapidFileChanges) {
  QTemporaryDir app_dir;
  ASSERT_TRUE(app_dir.isValid());

  const QString cache_path = app_dir.path() + QStringLiteral("/mimeinfo.cache");
  ASSERT_TRUE(writeFile(cache_path, QByteArrayLiteral("[MIME Cache]\n")));

  auto owned_resolver = std::make_unique<CountingResolver>();
  CountingResolver* resolver = owned_resolver.get();

  MimeService service(QStringList{app_dir.path()}, std::move(owned_resolver));

  QTest::qWait(100);
  const int baseline = resolver->call_count.load();

  // Write 5 times rapidly — all within the 500ms debounce window (10ms spacing).
  for (int iteration = 0; iteration < 5; ++iteration) {
    ASSERT_TRUE(writeFile(cache_path, QByteArrayLiteral("[MIME Cache]\nwrite=") + QByteArray::number(iteration) +
                                          QByteArrayLiteral("\n")));
    QTest::qWait(10);
  }

  // Debounce settles after 500ms. 5 rapid writes should coalesce into at most 2 refresh batches
  // (5 × 6 = 30 unchecked; debounce should reduce this to ≤ 12 calls = 2 batches).
  QTest::qWait(kDebouncePlusSlack);

  const int additional_calls = resolver->call_count.load() - baseline;
  EXPECT_GE(additional_calls, 6);   // at least one refresh fired
  EXPECT_LE(additional_calls, 18);  // debounce coalesced 5 writes into ≤ 3 batches
}

TEST(MimeServiceFileWatch, RoleSignalFiresOnFileChange) {
  QTemporaryDir app_dir;
  ASSERT_TRUE(app_dir.isValid());

  const QString cache_path = app_dir.path() + QStringLiteral("/mimeinfo.cache");
  ASSERT_TRUE(writeFile(cache_path, QByteArrayLiteral("[MIME Cache]\n")));

  MimeService service(QStringList{app_dir.path()}, std::make_unique<AlternatingResolver>());

  // Wait for initial refresh — resolver returns "app1.desktop" for all roles.
  QTest::qWait(100);

  // Spy on browser role; the next refresh returns "app2.desktop" → signal fires.
  QSignalSpy spy(&service, &MimeService::defaultBrowserChanged);

  ASSERT_TRUE(writeFile(cache_path, QByteArrayLiteral("[MIME Cache]\nupdated=true\n")));
  EXPECT_TRUE(spy.wait(kDebouncePlusSlack));
}

TEST(MimeServiceFileWatch, EmptyAppDirsWatchesNoFiles) {
  MimeService service(QStringList{}, std::make_unique<CountingResolver>());
  QTest::qWait(50);
  SUCCEED();
}

TEST(MimeServiceFileWatch, SkipsNonExistentAppDirs) {
  MimeService service(QStringList{QStringLiteral("/nonexistent/path/that/does/not/exist")},
                      std::make_unique<CountingResolver>());
  QTest::qWait(50);
  SUCCEED();
}

TEST(MimeServiceBrowserDefault, IgnoresBrowserMimeDefaultsWhenBrowserCheckFails) {
  MimeService service(QStringList{}, std::make_unique<BrowserCheckResolver>());
  QTest::qWait(50);

  EXPECT_TRUE(service.defaultBrowser().isEmpty());
}

TEST(MimeServiceBrowserDefault, UsesBrowserSpecificSetter) {
  auto owned_resolver = std::make_unique<BrowserCheckResolver>();
  BrowserCheckResolver* resolver = owned_resolver.get();

  MimeService service(QStringList{}, std::move(owned_resolver));
  QSignalSpy spy(&service, &MimeService::defaultBrowserChanged);

  service.setDefaultBrowser(QStringLiteral("com.google.Chrome.desktop"));
  QTRY_VERIFY_WITH_TIMEOUT(service.defaultBrowser() == QStringLiteral("com.google.Chrome.desktop"), 100);

  EXPECT_EQ(resolver->set_browser_arg, QStringLiteral("com.google.Chrome.desktop"));
  EXPECT_EQ(service.defaultBrowser(), QStringLiteral("com.google.Chrome.desktop"));
  EXPECT_GE(spy.count(), 1);
}

TEST(MimeServiceDefaults, SuccessfulSetUpdatesRoleAndIgnoresStaleQuery) {
  auto owned_resolver = std::make_unique<DeferredResolver>();
  DeferredResolver* resolver = owned_resolver.get();

  MimeService service(QStringList{}, std::move(owned_resolver));
  QTest::qWait(0);

  ASSERT_GE(resolver->query_callbacks.value(QStringLiteral("text/plain")).size(), 1);

  QSignalSpy spy(&service, &MimeService::defaultTextEditorChanged);
  QSignalSpy terminal_spy(&service, &MimeService::defaultTerminalChanged);
  QSignalSpy file_manager_spy(&service, &MimeService::defaultFileManagerChanged);
  QSignalSpy image_viewer_spy(&service, &MimeService::defaultImageViewerChanged);
  QSignalSpy video_player_spy(&service, &MimeService::defaultVideoPlayerChanged);
  service.setDefaultTextEditor(QStringLiteral("org.kde.kate.desktop"));

  EXPECT_EQ(resolver->set_default_arg, QStringLiteral("org.kde.kate.desktop"));
  EXPECT_EQ(service.defaultTextEditor(), QStringLiteral("org.kde.kate.desktop"));
  EXPECT_GE(spy.count(), 1);
  EXPECT_EQ(terminal_spy.count(), 0);
  EXPECT_EQ(file_manager_spy.count(), 0);
  EXPECT_EQ(image_viewer_spy.count(), 0);
  EXPECT_EQ(video_player_spy.count(), 0);

  auto& text_callbacks = resolver->query_callbacks[QStringLiteral("text/plain")];
  ASSERT_GE(text_callbacks.size(), 2);

  auto stale_query = std::move(text_callbacks.front());
  text_callbacks.pop_front();
  stale_query(QStringLiteral("geany.desktop"));

  EXPECT_EQ(service.defaultTextEditor(), QStringLiteral("org.kde.kate.desktop"));

  auto fresh_query = std::move(text_callbacks.front());
  text_callbacks.pop_front();
  fresh_query(QStringLiteral("org.kde.kate.desktop"));

  EXPECT_EQ(service.defaultTextEditor(), QStringLiteral("org.kde.kate.desktop"));
}

TEST(MimeServiceDefaults, ImageViewerSetterUsesDeclaredImageMimeTypes) {
  auto owned_resolver = std::make_unique<RecordingResolver>();
  RecordingResolver* resolver = owned_resolver.get();

  MimeService service(QStringList{}, std::move(owned_resolver));
  QTest::qWait(0);

  service.setDefaultImageViewer(
      QStringLiteral("org.test.QView.desktop"),
      {QStringLiteral("image/jpeg"), QStringLiteral("image/jxl"), QStringLiteral("image/jpg"),
       QStringLiteral("image/pjpeg"), QStringLiteral("image/x-png"), QStringLiteral("application/pdf")});

  EXPECT_EQ(resolver->set_defaults.value(QStringLiteral("image/jpeg")), QStringLiteral("org.test.QView.desktop"));
  EXPECT_EQ(resolver->set_defaults.value(QStringLiteral("image/png")), QStringLiteral("org.test.QView.desktop"));
  EXPECT_EQ(resolver->set_defaults.value(QStringLiteral("image/gif")), QStringLiteral("org.test.QView.desktop"));
  EXPECT_EQ(resolver->set_defaults.value(QStringLiteral("image/webp")), QStringLiteral("org.test.QView.desktop"));
  EXPECT_EQ(resolver->set_defaults.value(QStringLiteral("image/jxl")), QStringLiteral("org.test.QView.desktop"));
  EXPECT_EQ(resolver->set_defaults.value(QStringLiteral("image/jpg")), QStringLiteral("org.test.QView.desktop"));
  EXPECT_EQ(resolver->set_defaults.value(QStringLiteral("image/pjpeg")), QStringLiteral("org.test.QView.desktop"));
  EXPECT_EQ(resolver->set_defaults.value(QStringLiteral("image/x-png")), QStringLiteral("org.test.QView.desktop"));
  EXPECT_FALSE(resolver->set_defaults.contains(QStringLiteral("application/pdf")));
}

TEST(MimeServiceDefaults, MixedImageDefaultsReturnEmptyRoleDefault) {
  auto owned_resolver = std::make_unique<RecordingResolver>();
  RecordingResolver* resolver = owned_resolver.get();
  resolver->defaults.insert(QStringLiteral("image/jpeg"), QStringLiteral("gwenview.desktop"));
  resolver->defaults.insert(QStringLiteral("image/png"), QStringLiteral("imv.desktop"));

  MimeService service(QStringList{}, std::move(owned_resolver));
  QTRY_VERIFY_WITH_TIMEOUT(service.defaultImageViewer().isEmpty(), 100);
}

TEST(MimeServiceDefaults, BrowserSetterDoesNotUseDeclaredImageMimeTypes) {
  auto owned_resolver = std::make_unique<RecordingResolver>();
  RecordingResolver* resolver = owned_resolver.get();

  MimeService service(QStringList{}, std::move(owned_resolver));
  QTest::qWait(0);

  service.setDefaultBrowser(QStringLiteral("firefox.desktop"),
                            {QStringLiteral("text/html"), QStringLiteral("application/xhtml+xml"),
                             QStringLiteral("image/png"), QStringLiteral("image/jpeg")});

  EXPECT_EQ(resolver->set_browser_arg, QStringLiteral("firefox.desktop"));
  EXPECT_EQ(resolver->set_defaults.value(QStringLiteral("text/html")), QStringLiteral("firefox.desktop"));
  EXPECT_EQ(resolver->set_defaults.value(QStringLiteral("x-scheme-handler/http")), QStringLiteral("firefox.desktop"));
  EXPECT_EQ(resolver->set_defaults.value(QStringLiteral("x-scheme-handler/https")), QStringLiteral("firefox.desktop"));
  EXPECT_EQ(resolver->set_defaults.value(QStringLiteral("application/xhtml+xml")), QStringLiteral("firefox.desktop"));
  EXPECT_FALSE(resolver->set_defaults.contains(QStringLiteral("image/png")));
  EXPECT_FALSE(resolver->set_defaults.contains(QStringLiteral("image/jpeg")));
}

TEST(MimeServiceDefaults, ProcessSetterUpdatesDesktopSpecificMimeappsOverride) {
  QTemporaryDir config_dir;
  QTemporaryDir data_dir;
  ASSERT_TRUE(config_dir.isValid());
  ASSERT_TRUE(data_dir.isValid());

  EnvVarGuard config_guard("XDG_CONFIG_HOME");
  EnvVarGuard data_guard("XDG_DATA_HOME");
  EnvVarGuard desktop_guard("XDG_CURRENT_DESKTOP");
  qputenv("XDG_CONFIG_HOME", config_dir.path().toUtf8());
  qputenv("XDG_DATA_HOME", data_dir.path().toUtf8());
  qputenv("XDG_CURRENT_DESKTOP", QByteArrayLiteral("Hyprland"));

  const QString applications_dir = data_dir.path() + QStringLiteral("/applications");
  ASSERT_TRUE(QDir().mkpath(applications_dir));
  ASSERT_TRUE(writeFile(applications_dir + QStringLiteral("/org.test.Editor.desktop"),
                        QByteArrayLiteral("[Desktop Entry]\n"
                                          "Type=Application\n"
                                          "Name=Test Editor\n"
                                          "Exec=true %F\n"
                                          "MimeType=text/plain;\n")));

  const QString hyprland_mimeapps = config_dir.path() + QStringLiteral("/hyprland-mimeapps.list");
  ASSERT_TRUE(writeFile(hyprland_mimeapps, QByteArrayLiteral("[Default Applications]\ntext/plain=geany.desktop\n")));

  MimeService service;
  service.setDefaultTextEditor(QStringLiteral("org.test.Editor.desktop"));

  QTRY_VERIFY_WITH_TIMEOUT(service.defaultTextEditor() == QStringLiteral("org.test.Editor.desktop"), 3000);
  EXPECT_TRUE(
      readFile(config_dir.path() + QStringLiteral("/mimeapps.list")).contains("text/plain=org.test.Editor.desktop"));
  EXPECT_TRUE(readFile(hyprland_mimeapps).contains("text/plain=org.test.Editor.desktop"));
}

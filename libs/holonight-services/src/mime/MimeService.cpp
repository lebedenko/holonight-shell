#include "MimeService.h"

#include "process/GuardedProcessRunner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QPointer>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#include <memory>
#include <optional>
#include <utility>

Q_LOGGING_CATEGORY(lcMime, "holonight.mime")

namespace {

constexpr int kMimeProcessTimeoutMs = 5000;

// Hardcoded role → MIME type mappings (REQ-F-007, REQ-C-003).
const QStringList kTerminalMimes{
    QStringLiteral("application/x-terminal-emulator"),
};
const QStringList kFileManagerMimes{
    QStringLiteral("inode/directory"),
};
const QStringList kImageViewerMimes{
    QStringLiteral("image/jpeg"),
    QStringLiteral("image/png"),
    QStringLiteral("image/gif"),
    QStringLiteral("image/webp"),
};
const QStringList kTextEditorMimes{
    QStringLiteral("text/plain"),
};
const QStringList kVideoPlayerMimes{
    QStringLiteral("video/mp4"),
    QStringLiteral("video/x-matroska"),
    QStringLiteral("video/webm"),
};
const QStringList kBrowserDeclaredMimeAllowlist{
    QStringLiteral("text/html"),
    QStringLiteral("application/xhtml+xml"),
    QStringLiteral("application/pdf"),
    QStringLiteral("x-scheme-handler/http"),
    QStringLiteral("x-scheme-handler/https"),
};
const QStringList kTextEditorDeclaredMimeAllowlist{
    QStringLiteral("application/javascript"), QStringLiteral("application/json"),
    QStringLiteral("application/toml"),       QStringLiteral("application/x-shellscript"),
    QStringLiteral("application/x-yaml"),     QStringLiteral("application/xml"),
    QStringLiteral("application/yaml"),
};

void appendUnique(QStringList* list, const QString& value) {
  if (!value.isEmpty() && !list->contains(value)) {
    list->append(value);
  }
}

QStringList uniqueRoleMimes(const QStringList& seeds, const QStringList& declared_mime_types,
                            const std::function<bool(const QString&)>& include_declared) {
  QStringList result;
  for (const QString& mime : seeds) {
    appendUnique(&result, mime);
  }
  for (const QString& mime : declared_mime_types) {
    const QString trimmed = mime.trimmed();
    if (include_declared(trimmed)) {
      appendUnique(&result, trimmed);
    }
  }
  return result;
}

QStringList imageViewerMimesFor(const QStringList& declared_mime_types) {
  return uniqueRoleMimes(kImageViewerMimes, declared_mime_types,
                         [](const QString& mime) { return mime.startsWith(QStringLiteral("image/")); });
}

QStringList videoPlayerMimesFor(const QStringList& declared_mime_types) {
  return uniqueRoleMimes(kVideoPlayerMimes, declared_mime_types,
                         [](const QString& mime) { return mime.startsWith(QStringLiteral("video/")); });
}

QStringList textEditorMimesFor(const QStringList& declared_mime_types) {
  return uniqueRoleMimes(kTextEditorMimes, declared_mime_types, [](const QString& mime) {
    return mime.startsWith(QStringLiteral("text/")) || kTextEditorDeclaredMimeAllowlist.contains(mime);
  });
}

QStringList browserMimesFor(const QStringList& declared_mime_types) {
  return uniqueRoleMimes(
      {QStringLiteral("text/html"), QStringLiteral("x-scheme-handler/http"), QStringLiteral("x-scheme-handler/https")},
      declared_mime_types, [](const QString& mime) { return kBrowserDeclaredMimeAllowlist.contains(mime); });
}

QStringList terminalMimesFor(const QStringList& declared_mime_types) {
  return uniqueRoleMimes(kTerminalMimes, declared_mime_types,
                         [](const QString& mime) { return mime == QStringLiteral("application/x-terminal-emulator"); });
}

QStringList fileManagerMimesFor(const QStringList& declared_mime_types) {
  return uniqueRoleMimes(kFileManagerMimes, declared_mime_types,
                         [](const QString& mime) { return mime == QStringLiteral("inode/directory"); });
}

QString resolveRoleFromCache(const QHash<QString, QString>& cache, const QStringList& role_mimes) {
  std::optional<QString> resolved;
  for (const QString& mime : role_mimes) {
    const QString val = cache.value(mime);
    if (val.isEmpty()) {
      continue;
    }
    if (!resolved.has_value()) {
      resolved = val;
    } else if (resolved.value() != val) {
      return {};
    }
  }
  return resolved.value_or(QString{});
}

QString xdgConfigHome() {
  const QString configured = qEnvironmentVariable("XDG_CONFIG_HOME");
  return configured.isEmpty() ? QDir::homePath() + QStringLiteral("/.config") : configured;
}

QStringList desktopSpecificMimeappsPaths() {
  const QString desktop_env = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
  if (desktop_env.isEmpty()) {
    return {};
  }

  QStringList paths;
  const QString config_home = xdgConfigHome();
  const QStringList desktop_names = desktop_env.split(QLatin1Char(':'), Qt::SkipEmptyParts);
  for (const QString& desktop_name : desktop_names) {
    QString prefix = desktop_name.trimmed().toLower();
    if (prefix.isEmpty() || prefix.contains(QLatin1Char('/')) || prefix.contains(QLatin1Char('\\'))) {
      continue;
    }
    const QString path = QDir(config_home).filePath(prefix + QStringLiteral("-mimeapps.list"));
    if (!paths.contains(path)) {
      paths.append(path);
    }
  }
  return paths;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
bool writeMimeappsDefault(const QString& path, const QString& desktop_file, const QString& mime_type) {
  QDir dir = QFileInfo(path).absoluteDir();
  if (!dir.mkpath(QStringLiteral("."))) {
    qCWarning(lcMime) << "Failed to create mimeapps directory:" << dir.absolutePath();
    return false;
  }

  QStringList lines;
  QFile input(path);
  if (input.exists()) {
    if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qCWarning(lcMime) << "Failed to read mimeapps file:" << path;
      return false;
    }
    QTextStream input_stream(&input);
    while (!input_stream.atEnd()) {
      lines.append(input_stream.readLine());
    }
  }

  const QString section = QStringLiteral("[Default Applications]");
  const QString prefix = mime_type + QLatin1Char('=');
  const QString replacement = prefix + desktop_file;
  QStringList output;
  bool in_default_section = false;
  bool found_default_section = false;
  bool added = false;

  for (const QString& line : lines) {
    if (line == section) {
      in_default_section = true;
      found_default_section = true;
    } else if (line.startsWith(QLatin1Char('['))) {
      if (in_default_section && !added) {
        output.append(replacement);
        added = true;
      }
      in_default_section = false;
    } else if (in_default_section && !added && line.startsWith(prefix)) {
      output.append(replacement);
      added = true;
      continue;
    }

    output.append(line);
  }

  if (!added) {
    if (!found_default_section) {
      if (!output.isEmpty() && !output.constLast().isEmpty()) {
        output.append(QString{});
      }
      output.append(section);
    }
    output.append(replacement);
  }

  QSaveFile output_file(path);
  if (!output_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    qCWarning(lcMime) << "Failed to open mimeapps file for writing:" << path;
    return false;
  }

  QTextStream out(&output_file);
  for (const QString& line : output) {
    out << line << '\n';
  }

  if (!output_file.commit()) {
    qCWarning(lcMime) << "Failed to commit mimeapps file:" << path;
    return false;
  }
  return true;
}

bool writeDesktopSpecificMimeappsDefaults(const QString& desktop_file, const QString& mime_type) {
  bool success = true;
  for (const QString& path : desktopSpecificMimeappsPaths()) {
    success = writeMimeappsDefault(path, desktop_file, mime_type) && success;
  }
  return success;
}

bool writeMimeappsDefaults(const QString& desktop_file, const QString& mime_type) {
  const QString generic_path = QDir(xdgConfigHome()).filePath(QStringLiteral("mimeapps.list"));
  return writeMimeappsDefault(generic_path, desktop_file, mime_type) &&
         writeDesktopSpecificMimeappsDefaults(desktop_file, mime_type);
}

void runXdgSettings(const QStringList& arguments, std::function<void(int, QString, QString)> callback) {
  runGuardedProcess(QStringLiteral("xdg-settings"), arguments, kMimeProcessTimeoutMs,
                    [callback = std::move(callback)](GuardedProcessResult result) mutable {
                      if (result.timed_out) {
                        qCWarning(lcMime) << "xdg-settings timed out, killing process";
                        callback(-1, QString{}, result.stderr_text);
                        return;
                      }
                      if (result.had_error) {
                        qCWarning(lcMime) << "xdg-settings error:" << result.stderr_text;
                        callback(-1, QString{}, result.stderr_text);
                        return;
                      }
                      callback(result.exit_code, std::move(result.stdout_text), std::move(result.stderr_text));
                    });
}

// Production resolver: spawns xdg-mime subprocesses with a 5-second kill guard.
class ProcessMimeResolver final : public IMimeResolver {
 public:
  void queryDefault(const QString& mime_type, std::function<void(QString)> callback) override {
    runGuardedProcess(QStringLiteral("xdg-mime"), {QStringLiteral("query"), QStringLiteral("default"), mime_type},
                      kMimeProcessTimeoutMs,
                      [callback = std::move(callback)](const GuardedProcessResult& result) mutable {
                        if (result.timed_out) {
                          qCWarning(lcMime) << "xdg-mime query timed out, killing process";
                          callback(QString{});
                          return;
                        }
                        if (result.had_error) {
                          qCWarning(lcMime) << "xdg-mime query error:" << result.stderr_text;
                          callback(QString{});
                          return;
                        }
                        callback(result.exit_code == 0 ? result.stdout_text : QString{});
                      });
  }

  void queryDefaultBrowser(std::function<void(QString)> callback) override {
    auto callback_ptr = std::make_shared<std::function<void(QString)>>(std::move(callback));
    runXdgSettings({QStringLiteral("get"), QStringLiteral("default-web-browser")},
                   [callback_ptr](int exit_code, const QString& desktop_file, const QString& stderr_text) {
                     if (exit_code != 0 || desktop_file.isEmpty()) {
                       if (!stderr_text.isEmpty()) {
                         qCWarning(lcMime) << "xdg-settings get default-web-browser failed:" << stderr_text;
                       }
                       (*callback_ptr)(QString{});
                       return;
                     }

                     runXdgSettings(
                         {QStringLiteral("check"), QStringLiteral("default-web-browser"), desktop_file},
                         [callback_ptr, desktop_file](int check_exit_code, const QString& check_result,
                                                      const QString& check_stderr) {
                           if (check_exit_code != 0 && !check_stderr.isEmpty()) {
                             qCWarning(lcMime) << "xdg-settings check default-web-browser failed:" << check_stderr;
                           }
                           (*callback_ptr)(check_exit_code == 0 && check_result == QStringLiteral("yes") ? desktop_file
                                                                                                         : QString{});
                         });
                   });
  }

  void setDefault(const QString& desktop_file, const QString& mime_type, std::function<void(bool)> callback) override {
    runGuardedProcess(
        QStringLiteral("xdg-mime"), {QStringLiteral("default"), desktop_file, mime_type}, kMimeProcessTimeoutMs,
        [callback = std::move(callback), desktop_file, mime_type](const GuardedProcessResult& result) mutable {
          if (result.timed_out) {
            qCWarning(lcMime) << "xdg-mime default timed out, killing process";
            callback(false);
            return;
          }
          if (result.had_error) {
            qCWarning(lcMime) << "xdg-mime default error:" << result.stderr_text;
            callback(false);
            return;
          }
          if (result.exit_code != 0) {
            qCWarning(lcMime) << "xdg-mime default failed:" << result.stderr_text;
            callback(false);
            return;
          }
          callback(writeMimeappsDefaults(desktop_file, mime_type));
        });
  }

  void setDefaultBrowser(const QString& desktop_file, std::function<void(bool)> callback) override {
    runXdgSettings({QStringLiteral("set"), QStringLiteral("default-web-browser"), desktop_file},
                   [callback = std::move(callback)](int exit_code, const QString&, const QString& stderr_text) mutable {
                     if (exit_code != 0 && !stderr_text.isEmpty()) {
                       qCWarning(lcMime) << "xdg-settings set default-web-browser failed:" << stderr_text;
                     }
                     callback(exit_code == 0);
                   });
  }
};

// Test seam: returns canned answers without spawning subprocesses.
class NullMimeResolver final : public IMimeResolver {
 public:
  QHash<QString, QString> canned;

  void queryDefault(const QString& mime_type, std::function<void(QString)> callback) override {
    callback(canned.value(mime_type));
  }

  void queryDefaultBrowser(std::function<void(QString)> callback) override {
    callback(canned.value(QStringLiteral("browser")));
  }

  void setDefault(const QString& desktop_file, const QString& mime_type, std::function<void(bool)> callback) override {
    canned[mime_type] = desktop_file;
    callback(true);
  }

  void setDefaultBrowser(const QString& desktop_file, std::function<void(bool)> callback) override {
    canned[QStringLiteral("browser")] = desktop_file;
    callback(true);
  }
};

}  // namespace

MimeService::MimeService(QObject* parent)
    : MimeService(QStringList{}, std::make_unique<ProcessMimeResolver>(), parent) {}

MimeService::MimeService(std::unique_ptr<IMimeResolver> resolver, QObject* parent)
    : MimeService(QStringList{}, std::move(resolver), parent) {}

MimeService::MimeService(const QStringList& app_dirs, QObject* parent)
    : MimeService(app_dirs, std::make_unique<ProcessMimeResolver>(), parent) {}

MimeService::MimeService(const QStringList& app_dirs, std::unique_ptr<IMimeResolver> resolver, QObject* parent)
    : QObject(parent),
      resolver_(std::move(resolver)),
      terminal_role_mimes_(kTerminalMimes),
      file_manager_role_mimes_(kFileManagerMimes),
      image_viewer_role_mimes_(kImageViewerMimes),
      text_editor_role_mimes_(kTextEditorMimes),
      video_player_role_mimes_(kVideoPlayerMimes) {
  QTimer::singleShot(0, this, &MimeService::refreshAllRoles);

  QStringList watch_files;
  watch_files << QDir(xdgConfigHome()).filePath(QStringLiteral("mimeapps.list"));
  watch_files << desktopSpecificMimeappsPaths();
  watch_files << QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
                     QStringLiteral("/applications/mimeapps.list");
  for (const QString& dir : app_dirs) {
    watch_files << dir + QStringLiteral("/mimeinfo.cache");
  }

  for (const QString& path : watch_files) {
    if (QFileInfo::exists(path)) {
      mime_file_watcher_.addPath(path);
    }
  }

  mime_debounce_timer_.setSingleShot(true);
  mime_debounce_timer_.setInterval(500);

  connect(&mime_file_watcher_, &QFileSystemWatcher::fileChanged, this, [this](const QString& path) {
    // Re-add path after atomic rename replaces the inode (inotify drops the watch on unlink).
    if (QFileInfo::exists(path) && !mime_file_watcher_.files().contains(path)) {
      mime_file_watcher_.addPath(path);
    }
    mime_debounce_timer_.start();
  });
  connect(&mime_debounce_timer_, &QTimer::timeout, this, &MimeService::refreshAllRoles);
}

MimeService::~MimeService() = default;

QString MimeService::defaultBrowser() const { return browser_default_; }
QString MimeService::defaultTerminal() const { return resolveRole(terminal_role_mimes_); }
QString MimeService::defaultFileManager() const { return resolveRole(file_manager_role_mimes_); }
QString MimeService::defaultImageViewer() const { return resolveRole(image_viewer_role_mimes_); }
QString MimeService::defaultTextEditor() const { return resolveRole(text_editor_role_mimes_); }
QString MimeService::defaultVideoPlayer() const { return resolveRole(video_player_role_mimes_); }

void MimeService::setDefaultBrowser(const QString& desktop_file, const QStringList& declared_mime_types) {
  const QPointer<MimeService> self(this);
  resolver_->setDefaultBrowser(desktop_file, [self](bool success) {
    if (self == nullptr) {
      return;
    }
    if (success) {
      self->queryBrowserAsync();
    } else {
      qCWarning(lcMime) << "Failed to set default browser";
    }
  });

  setDefaultAsync(desktop_file, browserMimesFor(declared_mime_types));
}
void MimeService::setDefaultTerminal(const QString& desktop_file, const QStringList& declared_mime_types) {
  terminal_role_mimes_ = terminalMimesFor(declared_mime_types);
  setDefaultAsync(desktop_file, terminal_role_mimes_);
}
void MimeService::setDefaultFileManager(const QString& desktop_file, const QStringList& declared_mime_types) {
  file_manager_role_mimes_ = fileManagerMimesFor(declared_mime_types);
  setDefaultAsync(desktop_file, file_manager_role_mimes_);
}
void MimeService::setDefaultImageViewer(const QString& desktop_file, const QStringList& declared_mime_types) {
  image_viewer_role_mimes_ = imageViewerMimesFor(declared_mime_types);
  setDefaultAsync(desktop_file, image_viewer_role_mimes_);
}
void MimeService::setDefaultTextEditor(const QString& desktop_file, const QStringList& declared_mime_types) {
  text_editor_role_mimes_ = textEditorMimesFor(declared_mime_types);
  setDefaultAsync(desktop_file, text_editor_role_mimes_);
}
void MimeService::setDefaultVideoPlayer(const QString& desktop_file, const QStringList& declared_mime_types) {
  video_player_role_mimes_ = videoPlayerMimesFor(declared_mime_types);
  setDefaultAsync(desktop_file, video_player_role_mimes_);
}

void MimeService::refreshAllRoles() {
  queryBrowserAsync();
  for (const QString& mime : trackedRoleMimes()) {
    queryMimeAsync(mime);
  }
}

void MimeService::queryBrowserAsync() {
  if (browser_pending_) {
    return;
  }
  browser_pending_ = true;

  const QPointer<MimeService> self(this);
  resolver_->queryDefaultBrowser([self](const QString& desktop_file) {
    if (self == nullptr) {
      return;
    }
    self->onBrowserQueryResult(desktop_file);
  });
}

void MimeService::queryMimeAsync(const QString& mime_type) {
  if (pending_query_generations_.contains(mime_type)) {
    return;
  }
  const int generation = mime_query_generations_.value(mime_type);
  pending_query_generations_.insert(mime_type, generation);

  const QPointer<MimeService> self(this);
  resolver_->queryDefault(mime_type, [self, mime_type, generation](const QString& desktop_file) {
    if (self == nullptr) {
      return;
    }
    self->onQueryResult(mime_type, generation, desktop_file);
  });
}

void MimeService::onQueryResult(const QString& mime_type, int generation, const QString& desktop_file) {
  if (pending_query_generations_.value(mime_type, -1) == generation) {
    pending_query_generations_.remove(mime_type);
  }
  if (mime_query_generations_.value(mime_type) != generation) {
    return;
  }
  updateMimeCache(mime_type, desktop_file);
}

void MimeService::onBrowserQueryResult(const QString& desktop_file) {
  browser_pending_ = false;
  if (browser_default_ == desktop_file) {
    return;
  }
  browser_default_ = desktop_file;
  emit defaultBrowserChanged();
}

void MimeService::setDefaultAsync(const QString& desktop_file, const QStringList& role_mimes) {
  for (const QString& mime : role_mimes) {
    const QPointer<MimeService> self(this);
    resolver_->setDefault(desktop_file, mime, [self, desktop_file, mime](bool success) {
      if (self == nullptr) {
        return;
      }
      if (success) {
        self->mime_query_generations_[mime] = self->mime_query_generations_.value(mime) + 1;
        self->pending_query_generations_.remove(mime);
        self->updateMimeCache(mime, desktop_file);
        self->queryMimeAsync(mime);
      } else {
        qCWarning(lcMime) << "Failed to set default for MIME type:" << mime;
      }
    });
  }
}

QString MimeService::resolveRole(const QStringList& role_mimes) const {
  return resolveRoleFromCache(mime_cache_, role_mimes);
}

QStringList MimeService::trackedRoleMimes() const {
  QStringList all;
  for (const QStringList& role_mimes : {terminal_role_mimes_, file_manager_role_mimes_, image_viewer_role_mimes_,
                                        text_editor_role_mimes_, video_player_role_mimes_}) {
    for (const QString& mime : role_mimes) {
      appendUnique(&all, mime);
    }
  }
  return all;
}

void MimeService::updateMimeCache(const QString& mime_type, const QString& desktop_file) {
  const QString old_terminal = defaultTerminal();
  const QString old_file_manager = defaultFileManager();
  const QString old_image_viewer = defaultImageViewer();
  const QString old_text_editor = defaultTextEditor();
  const QString old_video_player = defaultVideoPlayer();

  mime_cache_.insert(mime_type, desktop_file);

  if (defaultTerminal() != old_terminal) {
    emit defaultTerminalChanged();
  }
  if (defaultFileManager() != old_file_manager) {
    emit defaultFileManagerChanged();
  }
  if (defaultImageViewer() != old_image_viewer) {
    emit defaultImageViewerChanged();
  }
  if (defaultTextEditor() != old_text_editor) {
    emit defaultTextEditorChanged();
  }
  if (defaultVideoPlayer() != old_video_player) {
    emit defaultVideoPlayerChanged();
  }
}

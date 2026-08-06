#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <functional>
#include <memory>

// Abstract subprocess seam — injected in tests, real QProcess in production.
class IMimeResolver {
 public:
  virtual ~IMimeResolver() = default;

  IMimeResolver(const IMimeResolver&) = delete;
  IMimeResolver& operator=(const IMimeResolver&) = delete;
  IMimeResolver(IMimeResolver&&) = delete;
  IMimeResolver& operator=(IMimeResolver&&) = delete;

  // Invokes callback(desktopFile) asynchronously when done; empty string on failure.
  virtual void queryDefault(const QString& mime_type, std::function<void(QString)> callback) = 0;
  // Invokes callback(desktopFile) asynchronously when xdg-settings agrees the web browser is fully configured.
  virtual void queryDefaultBrowser(std::function<void(QString)> callback) = 0;
  // Invokes callback(success) asynchronously when done.
  virtual void setDefault(const QString& desktop_file, const QString& mime_type,
                          std::function<void(bool)> callback) = 0;
  // Invokes callback(success) asynchronously when xdg-settings has set the default web browser.
  virtual void setDefaultBrowser(const QString& desktop_file, std::function<void(bool)> callback) = 0;

 protected:
  IMimeResolver() = default;
};

// QML singleton. Queries and sets system MIME type defaults for six preset application roles
// (browser, terminal, file manager, image viewer, text editor, video player) via xdg-mime.
// All subprocess calls are async; role properties update reactively via NOTIFY signals.
class MimeService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(QString defaultBrowser READ defaultBrowser NOTIFY defaultBrowserChanged FINAL)
  Q_PROPERTY(QString defaultTerminal READ defaultTerminal NOTIFY defaultTerminalChanged FINAL)
  Q_PROPERTY(QString defaultFileManager READ defaultFileManager NOTIFY defaultFileManagerChanged FINAL)
  Q_PROPERTY(QString defaultImageViewer READ defaultImageViewer NOTIFY defaultImageViewerChanged FINAL)
  Q_PROPERTY(QString defaultTextEditor READ defaultTextEditor NOTIFY defaultTextEditorChanged FINAL)
  Q_PROPERTY(QString defaultVideoPlayer READ defaultVideoPlayer NOTIFY defaultVideoPlayerChanged FINAL)

 public:
  explicit MimeService(QObject* parent = nullptr);
  explicit MimeService(std::unique_ptr<IMimeResolver> resolver, QObject* parent = nullptr);
  explicit MimeService(const QStringList& app_dirs, QObject* parent = nullptr);
  MimeService(const QStringList& app_dirs, std::unique_ptr<IMimeResolver> resolver, QObject* parent = nullptr);
  ~MimeService() override;

  MimeService(const MimeService&) = delete;
  MimeService& operator=(const MimeService&) = delete;
  MimeService(MimeService&&) = delete;
  MimeService& operator=(MimeService&&) = delete;

  [[nodiscard]] QString defaultBrowser() const;
  [[nodiscard]] QString defaultTerminal() const;
  [[nodiscard]] QString defaultFileManager() const;
  [[nodiscard]] QString defaultImageViewer() const;
  [[nodiscard]] QString defaultTextEditor() const;
  [[nodiscard]] QString defaultVideoPlayer() const;

  Q_INVOKABLE void setDefaultBrowser(const QString& desktop_file, const QStringList& declared_mime_types = {});
  Q_INVOKABLE void setDefaultTerminal(const QString& desktop_file, const QStringList& declared_mime_types = {});
  Q_INVOKABLE void setDefaultFileManager(const QString& desktop_file, const QStringList& declared_mime_types = {});
  Q_INVOKABLE void setDefaultImageViewer(const QString& desktop_file, const QStringList& declared_mime_types = {});
  Q_INVOKABLE void setDefaultTextEditor(const QString& desktop_file, const QStringList& declared_mime_types = {});
  Q_INVOKABLE void setDefaultVideoPlayer(const QString& desktop_file, const QStringList& declared_mime_types = {});

 Q_SIGNALS:
  void defaultBrowserChanged();
  void defaultTerminalChanged();
  void defaultFileManagerChanged();
  void defaultImageViewerChanged();
  void defaultTextEditorChanged();
  void defaultVideoPlayerChanged();

 public Q_SLOTS:
  void refreshAllRoles();

 private:
  void queryMimeAsync(const QString& mime_type);
  void queryBrowserAsync();
  void setDefaultAsync(const QString& desktop_file, const QStringList& role_mimes);
  [[nodiscard]] QString resolveRole(const QStringList& role_mimes) const;
  [[nodiscard]] QStringList trackedRoleMimes() const;
  void onQueryResult(const QString& mime_type, int generation, const QString& desktop_file);
  void onBrowserQueryResult(const QString& desktop_file);
  void updateMimeCache(const QString& mime_type, const QString& desktop_file);

  std::unique_ptr<IMimeResolver> resolver_;
  QHash<QString, QString> mime_cache_;
  QHash<QString, int> mime_query_generations_;
  QHash<QString, int> pending_query_generations_;
  QString browser_default_;
  QStringList terminal_role_mimes_;
  QStringList file_manager_role_mimes_;
  QStringList image_viewer_role_mimes_;
  QStringList text_editor_role_mimes_;
  QStringList video_player_role_mimes_;
  bool browser_pending_{false};
  QFileSystemWatcher mime_file_watcher_;
  QTimer mime_debounce_timer_;
};

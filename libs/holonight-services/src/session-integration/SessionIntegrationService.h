#pragma once

#include <QFutureWatcher>
#include <QMutex>
#include <QObject>
#include <QProcessEnvironment>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <functional>
#include <memory>

struct SessionIntegrationCommandResult {
  int exit_code{-1};
  QString stdout_text;
  QString stderr_text;
};

struct ApplicationCacheRebuildStep {
  QString program;
  QStringList arguments;
  int exit_code{-1};
  QString stderr_text;
  bool success{false};

  [[nodiscard]] QString commandLine() const {
    return arguments.isEmpty() ? program : program + QLatin1Char(' ') + arguments.join(QLatin1Char(' '));
  }
};

class ISessionIntegrationCommandRunner {
 public:
  virtual ~ISessionIntegrationCommandRunner() = default;

  ISessionIntegrationCommandRunner(const ISessionIntegrationCommandRunner&) = delete;
  ISessionIntegrationCommandRunner& operator=(const ISessionIntegrationCommandRunner&) = delete;
  ISessionIntegrationCommandRunner(ISessionIntegrationCommandRunner&&) = delete;
  ISessionIntegrationCommandRunner& operator=(ISessionIntegrationCommandRunner&&) = delete;

  // refresh() invokes diagnostics concurrently, so implementations must support concurrent calls.
  [[nodiscard]] virtual bool executableExists(const QString& program) const = 0;
  [[nodiscard]] virtual SessionIntegrationCommandResult run(const QString& program,
                                                            const QStringList& arguments) const = 0;

 protected:
  ISessionIntegrationCommandRunner() = default;
};

class ISessionIntegrationBusProbe {
 public:
  virtual ~ISessionIntegrationBusProbe() = default;

  ISessionIntegrationBusProbe(const ISessionIntegrationBusProbe&) = delete;
  ISessionIntegrationBusProbe& operator=(const ISessionIntegrationBusProbe&) = delete;
  ISessionIntegrationBusProbe(ISessionIntegrationBusProbe&&) = delete;
  ISessionIntegrationBusProbe& operator=(ISessionIntegrationBusProbe&&) = delete;

  [[nodiscard]] virtual bool isConnected() const = 0;
  [[nodiscard]] virtual QString ownerForName(const QString& name) const = 0;
  [[nodiscard]] virtual QStringList registeredNames() const = 0;

 protected:
  ISessionIntegrationBusProbe() = default;
};

// QML singleton scaffold for desktop-session diagnostics and safe repair actions.
// The diagnostic API is added incrementally by the desktop-session-bootstrap tasks.
class SessionIntegrationService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(QString overallStatus READ overallStatus NOTIFY diagnosticsChanged FINAL)
  Q_PROPERTY(QVariantList diagnostics READ diagnostics NOTIFY diagnosticsChanged FINAL)
  Q_PROPERTY(bool refreshInProgress READ refreshInProgress NOTIFY refreshInProgressChanged FINAL)
  Q_PROPERTY(bool rebuildInProgress READ rebuildInProgress NOTIFY rebuildInProgressChanged FINAL)

 public:
  explicit SessionIntegrationService(QObject* parent = nullptr);
  SessionIntegrationService(std::unique_ptr<ISessionIntegrationCommandRunner> command_runner,
                            std::unique_ptr<ISessionIntegrationBusProbe> bus_probe,
                            const QProcessEnvironment& environment, QStringList application_dirs,
                            QObject* parent = nullptr);
  ~SessionIntegrationService() override;

  SessionIntegrationService(const SessionIntegrationService&) = delete;
  SessionIntegrationService& operator=(const SessionIntegrationService&) = delete;
  SessionIntegrationService(SessionIntegrationService&&) = delete;
  SessionIntegrationService& operator=(SessionIntegrationService&&) = delete;

  [[nodiscard]] QString overallStatus() const { return overall_status_; }
  [[nodiscard]] QVariantList diagnostics() const { return diagnostics_; }
  [[nodiscard]] bool refreshInProgress() const { return refresh_in_progress_; }
  [[nodiscard]] bool rebuildInProgress() const { return rebuild_in_progress_; }

  void setPostRebuildRefreshCallbacks(std::function<void()> refresh_mime_roles, std::function<void()> refresh_launcher);
  void setExpectedCursorTheme(QString cursor_theme);

  Q_INVOKABLE void refresh();
  Q_INVOKABLE void rebuildApplicationCaches();

 Q_SIGNALS:
  void diagnosticsChanged();
  void refreshInProgressChanged();
  void rebuildInProgressChanged();
  void rebuildFinished(bool success);

 private:
  [[nodiscard]] static QVariantMap addDiagnostic(const QString& diagnostic_id, const QString& title,
                                                 const QString& status, const QString& observed,
                                                 const QString& expected, const QString& detail,
                                                 const QString& command = {});
  [[nodiscard]] QVariantList addProcessEnvironmentDiagnostics() const;
  [[nodiscard]] QVariantList addSystemdEnvironmentDiagnostics() const;
  [[nodiscard]] QVariantList addDbusActivationDiagnostics() const;
  [[nodiscard]] QVariantList addXdgMenuDiagnostics() const;
  [[nodiscard]] QVariantList addKdeCacheDiagnostics() const;
  [[nodiscard]] QVariantList addMimeDiagnostics() const;
  [[nodiscard]] QVariantList addPortalAndDesktopServiceDiagnostics() const;
  void addLastRebuildDiagnostics();
  [[nodiscard]] QString envValue(const QString& name) const;
  [[nodiscard]] QStringList xdgMenuSearchPaths() const;
  [[nodiscard]] static QStringList discoverApplicationMenus(const QStringList& menu_dirs);
  [[nodiscard]] QString xdgCacheHome() const;
  [[nodiscard]] QString deriveOverallStatus() const;
  void setRefreshInProgress(bool val);
  void setRebuildInProgress(bool val);

  std::unique_ptr<ISessionIntegrationCommandRunner> command_runner_;
  std::unique_ptr<ISessionIntegrationBusProbe> bus_probe_;
  QProcessEnvironment environment_;
  QStringList application_dirs_;
  QVector<ApplicationCacheRebuildStep> last_rebuild_steps_;
  std::function<void()> refresh_mime_roles_;
  std::function<void()> refresh_launcher_;
  QString overall_status_{QStringLiteral("ok")};
  QVariantList diagnostics_;
  bool refresh_in_progress_{false};
  bool refresh_pending_{false};
  QString expected_cursor_theme_;
  mutable QMutex expected_cursor_mutex_;
  bool rebuild_in_progress_{false};
  QFutureWatcher<QList<QFuture<QVariantList>>>* refresh_watcher_{nullptr};
};

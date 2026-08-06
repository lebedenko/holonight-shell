#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

#include <memory>

class IdleBackend;
class IdleInhibitor;
class NotificationService;

// QML singleton. Owns the idle backend, exposes idle state to QML, and drives the keep-awake
// toggle via a logind inhibitor. Services (WeatherService, CalendarService) connect to idleChanged
// to pause/resume periodic polling.
class IdleService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(bool isIdle READ isIdle NOTIFY idleChanged FINAL)
  Q_PROPERTY(int idleThresholdMs READ idleThresholdMs WRITE setIdleThresholdMs NOTIFY idleThresholdMsChanged FINAL)
  Q_PROPERTY(bool idleBackendAvailable READ idleBackendAvailable NOTIFY idleBackendAvailableChanged FINAL)
  Q_PROPERTY(bool idleDaemonDetected READ idleDaemonDetected NOTIFY idleDaemonDetectedChanged FINAL)
  Q_PROPERTY(bool sessionLocked READ sessionLocked NOTIFY sessionLockedChanged FINAL)
  Q_PROPERTY(bool idleInhibited READ idleInhibited WRITE setIdleInhibited NOTIFY idleInhibitedChanged FINAL)

 public:
  // Production constructor: auto-detects backend and scans for idle daemons.
  explicit IdleService(NotificationService* notif, QObject* parent = nullptr);
  // Test seam: inject backend and daemon-detection result directly.
  explicit IdleService(std::unique_ptr<IdleBackend> backend, bool daemon_detected, QObject* parent = nullptr);
  explicit IdleService(std::unique_ptr<IdleBackend> backend, std::unique_ptr<IdleInhibitor> inhibitor,
                       bool daemon_detected, QObject* parent = nullptr);
  ~IdleService() override;

  IdleService(const IdleService&) = delete;
  IdleService& operator=(const IdleService&) = delete;
  IdleService(IdleService&&) = delete;
  IdleService& operator=(IdleService&&) = delete;

  [[nodiscard]] bool isIdle() const { return is_idle_; }
  [[nodiscard]] int idleThresholdMs() const { return idle_threshold_ms_; }
  [[nodiscard]] bool idleBackendAvailable() const;
  [[nodiscard]] bool idleDaemonDetected() const { return daemon_detected_; }
  [[nodiscard]] bool sessionLocked() const { return session_locked_; }
  [[nodiscard]] bool idleInhibited() const { return user_inhibited_; }

  void setIdleThresholdMs(int threshold_ms);
  void setIdleInhibited(bool inhibited);

  // Used by ScreenSaverAdaptor to answer GetSessionIdleTime().
  [[nodiscard]] uint getIdleTimeSeconds() const;

 Q_SIGNALS:
  void idleChanged(bool idle);
  void idleThresholdMsChanged();
  void idleBackendAvailableChanged();
  void idleDaemonDetectedChanged();
  void sessionLockedChanged();
  void idleInhibitedChanged();

 private Q_SLOTS:
  void onThresholdExceeded(bool idle);
  void onLockedHintChanged(const QString& interface_name, const QVariantMap& changed_properties,
                           const QStringList& invalidated_properties);

 private:
  void init(NotificationService* notif);
  void setIdleBackendAvailable(bool available);
  void detectDaemon(NotificationService* notif);
  void subscribeLockedHint();
  void readInitialLockedHint(const QString& session_path);

  static constexpr int kDefaultIdleThresholdMs{300'000};

  std::unique_ptr<IdleBackend> backend_;
  std::unique_ptr<IdleInhibitor> user_inhibitor_;

  bool is_idle_{false};
  int idle_threshold_ms_{kDefaultIdleThresholdMs};
  bool idle_backend_available_{false};
  bool daemon_detected_{false};
  bool session_locked_{false};
  bool user_inhibited_{false};
  QString session_path_;
};

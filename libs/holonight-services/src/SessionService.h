#pragma once

#include "CompositorSelection.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>

#include <memory>

class ProcessEnvironment;
class CommandRunner;
class SessionBackend;

// Thin QML-facing facade over an auto-detected SessionBackend. Detects the compositor once at
// construction (Hyprland when HYPRLAND_INSTANCE_SIGNATURE is set, otherwise a generic logind
// backend) and delegates every action to it. Capability properties are CONSTANT — they reflect
// the environment at construction and are used by QML to grey out unsupported actions (logout)
// and warn when no locker is installed. Live "is locked" / idle state is intentionally out of
// scope here (handled by the future idle-management work).
class SessionService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(QString backendName READ backendName CONSTANT)
  Q_PROPERTY(bool logoutSupported READ logoutSupported CONSTANT)
  Q_PROPERTY(bool lockerAvailable READ lockerAvailable CONSTANT)
  Q_PROPERTY(QString lockerName READ lockerName CONSTANT)

 public:
  explicit SessionService(QObject* parent = nullptr);
  explicit SessionService(CompositorKind kind, QObject* parent = nullptr);
  // Test seam: takes ownership of a pre-built backend (seams owned by the caller).
  explicit SessionService(std::unique_ptr<SessionBackend> backend, QObject* parent = nullptr);
  ~SessionService() override;

  SessionService(const SessionService&) = delete;
  SessionService& operator=(const SessionService&) = delete;
  SessionService(SessionService&&) = delete;
  SessionService& operator=(SessionService&&) = delete;

  [[nodiscard]] QString backendName() const;
  [[nodiscard]] bool logoutSupported() const;
  [[nodiscard]] bool lockerAvailable() const;
  [[nodiscard]] QString lockerName() const;

  Q_INVOKABLE void lockScreen();
  Q_INVOKABLE void logout();
  Q_INVOKABLE void sleep();
  Q_INVOKABLE void reboot();
  Q_INVOKABLE void shutdown();

 Q_SIGNALS:
  // Emitted when a session command's backend dispatch fails to launch. `action` is one of
  // "lock"/"logout"/"sleep"/"reboot"/"shutdown"; `reason` is a human-readable failure description.
  void commandFailed(const QString& action, const QString& reason);

 private:
  // Declared before backend_ so they are constructed first: the backend holds raw pointers into
  // them. Null in the test-injection path, where the caller owns the seams.
  std::unique_ptr<ProcessEnvironment> env_;
  std::unique_ptr<CommandRunner> runner_;
  const std::unique_ptr<SessionBackend> backend_;
};

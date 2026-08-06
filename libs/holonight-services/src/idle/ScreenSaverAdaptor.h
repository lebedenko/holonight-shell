#pragma once

#include <QDBusContext>
#include <QHash>
#include <QObject>
#include <QString>

#include <memory>

class IdleInhibitor;
class IdleService;

// Claims org.freedesktop.ScreenSaver on the session D-Bus and exposes GetSessionIdleTime(),
// Inhibit(), UnInhibit(), and the ActiveChanged signal. Q_CLASSINFO is required so Qt exports
// slots under org.freedesktop.ScreenSaver in introspection XML (not local.ScreenSaverAdaptor).
class ScreenSaverAdaptor : public QObject, protected QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.ScreenSaver")

 public:
  explicit ScreenSaverAdaptor(IdleService* idle_service, QObject* parent = nullptr);
  ScreenSaverAdaptor(IdleService* idle_service, std::unique_ptr<IdleInhibitor> inhibitor, QObject* parent = nullptr);
  ~ScreenSaverAdaptor() override;

  ScreenSaverAdaptor(const ScreenSaverAdaptor&) = delete;
  ScreenSaverAdaptor& operator=(const ScreenSaverAdaptor&) = delete;
  ScreenSaverAdaptor(ScreenSaverAdaptor&&) = delete;
  ScreenSaverAdaptor& operator=(ScreenSaverAdaptor&&) = delete;

  // Registers the service on the session bus. Returns false (with a warning) if the name is
  // already taken; never blocks or retries.
  bool registerService();

 public Q_SLOTS:
  // Slot and signal names are PascalCase per the freedesktop ScreenSaver spec.
  // Q_SCRIPTABLE limits export to these three — onIdleChanged stays off the bus.
  // NOLINTBEGIN(readability-identifier-naming)
  Q_SCRIPTABLE uint GetSessionIdleTime();
  Q_SCRIPTABLE uint Inhibit(const QString& app_name, const QString& reason);
  Q_SCRIPTABLE void UnInhibit(uint cookie);
  void onIdleChanged(bool idle);
  // NOLINTEND(readability-identifier-naming)

 Q_SIGNALS:
  // NOLINTBEGIN(readability-identifier-naming)
  Q_SCRIPTABLE void ActiveChanged(bool active);
  // NOLINTEND(readability-identifier-naming)

 private:
  IdleService* idle_service_;
  std::unique_ptr<IdleInhibitor> inhibitor_;
  QHash<uint, QString> inhibit_cookies_;
  uint next_cookie_{1};
};

#pragma once

#include <QObject>
#include <QString>

// Manages a single logind inhibitor file descriptor.
// Calls org.freedesktop.login1.Manager.Inhibit(what="idle:sleep") to acquire the fd;
// closing the fd releases the inhibitor. The destructor always releases.
class IdleInhibitor : public QObject {
  Q_OBJECT
 public:
  explicit IdleInhibitor(QObject* parent = nullptr);
  ~IdleInhibitor() override;

  IdleInhibitor(const IdleInhibitor&) = delete;
  IdleInhibitor& operator=(const IdleInhibitor&) = delete;
  IdleInhibitor(IdleInhibitor&&) = delete;
  IdleInhibitor& operator=(IdleInhibitor&&) = delete;

  // Acquires the logind inhibitor with the given reason. No-op if already held.
  [[nodiscard]] virtual bool acquire(const QString& reason);

  // Releases the inhibitor by closing the fd. No-op if not held.
  virtual void release();

  [[nodiscard]] bool isHeld() const { return inhibit_fd_ >= 0; }

 private:
  void releaseFd();

  int inhibit_fd_{-1};
};

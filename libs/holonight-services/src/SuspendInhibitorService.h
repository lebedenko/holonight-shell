#pragma once

#include "IActivityGate.h"
#include "InhibitorModel.h"

#include <QObject>
#include <QQmlEngine>

class QTimer;
class QProcess;

// QML singleton that polls org.freedesktop.login1.Manager.ListInhibitors() every 5 seconds and
// exposes active sleep inhibitors as an InhibitorModel for sidebar display. Implements IActivityGate
// so the poll timer is paused when the lid is closed.
class SuspendInhibitorService : public QObject, public IActivityGate {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QAbstractItemModel* inhibitorModel READ inhibitorModelForQml CONSTANT FINAL)
  Q_PROPERTY(int inhibitorCount READ inhibitorCount NOTIFY inhibitorCountChanged FINAL)

 public:
  struct SkipInitTag {};
  static constexpr SkipInitTag SkipInit{};

  explicit SuspendInhibitorService(QObject* parent = nullptr);
  explicit SuspendInhibitorService(SkipInitTag /*tag*/, QObject* parent = nullptr);
  ~SuspendInhibitorService() override = default;

  SuspendInhibitorService(const SuspendInhibitorService&) = delete;
  SuspendInhibitorService& operator=(const SuspendInhibitorService&) = delete;
  SuspendInhibitorService(SuspendInhibitorService&&) = delete;
  SuspendInhibitorService& operator=(SuspendInhibitorService&&) = delete;

  [[nodiscard]] InhibitorModel* inhibitorModel() { return &model_; }
  [[nodiscard]] const InhibitorModel* inhibitorModel() const { return &model_; }
  // QML model properties are mutable QObject pointers, so this accessor intentionally remains
  // non-const. C++ callers that only inspect inhibitor state use the const overload above.
  [[nodiscard]] QAbstractItemModel* inhibitorModelForQml() { return &model_; }
  [[nodiscard]] int inhibitorCount() const { return model_.count(); }

  void start();

  // IActivityGate — pauses the poll timer on lid close, resumes on lid open.
  void pauseActivity() override;
  void resumeActivity() override;

 protected:
  // Test seam: returns parsed sleep inhibitors. Override to inject fake replies.
  virtual QList<InhibitorEntry> listInhibitors();

 Q_SIGNALS:
  void inhibitorCountChanged();

 private:
  void poll();
  void startAsyncPoll();
  void finishAsyncPoll(QProcess* process, int exit_code);
  [[nodiscard]] static QList<InhibitorEntry> parseBusctlOutput(const QByteArray& output);

  InhibitorModel model_;
  QTimer* poll_timer_{nullptr};
  bool paused_{false};
  bool poll_in_flight_{false};
  bool synchronous_poll_for_tests_{false};
};

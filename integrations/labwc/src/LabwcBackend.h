#pragma once

#include "GenericBackend.h"
#include "qwayland-wlr-foreign-toplevel-management-unstable-v1.h"

class LabwcProtocol;
class LabwcWindow;

class LabwcBackend final : public CompositorBackend {
  Q_OBJECT
 public:
  explicit LabwcBackend(QObject* parent = nullptr);
  ~LabwcBackend() override;
  void start() override;
  void activateWorkspace(const QString& id) override;

 private:
  friend class LabwcProtocol;
  friend class LabwcWindow;
  void connectProtocol();
  void protocolFinished();
  void schedulePublish();
  void publish();
  GenericBackend workspace_;
  CompositorSnapshot workspace_snapshot_;
  std::unique_ptr<LabwcProtocol> protocol_;
  QList<LabwcWindow*> windows_;
  QTimer reconnect_timer_;
  quint64 activation_order_{0};
  bool available_{false};
  bool publish_pending_{false};
};

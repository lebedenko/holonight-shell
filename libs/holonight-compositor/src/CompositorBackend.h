#pragma once

#include "CompositorSnapshot.h"

#include <QObject>

class CompositorBackend : public QObject {
  Q_OBJECT

 public:
  using QObject::QObject;
  ~CompositorBackend() override = default;

  virtual void start() = 0;
  virtual void activateWorkspace(const QString& workspace_id) = 0;

 Q_SIGNALS:
  void snapshotReady(CompositorSnapshot snapshot);
};

#pragma once

#include "IActivityGate.h"

#include <QObject>

#include <vector>

class ActivityGateManager : public QObject {
  Q_OBJECT

 public:
  explicit ActivityGateManager(QObject* parent = nullptr);
  ~ActivityGateManager() override = default;

  ActivityGateManager(const ActivityGateManager&) = delete;
  ActivityGateManager& operator=(const ActivityGateManager&) = delete;
  ActivityGateManager(ActivityGateManager&&) = delete;
  ActivityGateManager& operator=(ActivityGateManager&&) = delete;

  void registerGate(IActivityGate* gate);

 public Q_SLOTS:
  void onLidStateChanged(bool closed);

 private:
  std::vector<IActivityGate*> gates_;
};

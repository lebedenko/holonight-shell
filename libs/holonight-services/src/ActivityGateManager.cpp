#include "ActivityGateManager.h"

ActivityGateManager::ActivityGateManager(QObject* parent) : QObject(parent) {}

void ActivityGateManager::registerGate(IActivityGate* gate) { gates_.push_back(gate); }

void ActivityGateManager::onLidStateChanged(bool closed) {
  for (IActivityGate* gate : gates_) {
    if (closed) {
      gate->pauseActivity();
    } else {
      gate->resumeActivity();
    }
  }
}

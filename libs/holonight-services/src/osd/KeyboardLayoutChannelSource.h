#pragma once

#include "OsdChannelSource.h"

#include <QObject>
#include <QString>

class KeyboardLayoutService;

// REQ-F-011. Adapts KeyboardLayoutService to the OSD's normalized selection channel.
//
// Available layout signals produce events, including a name-only change that
// leaves the code untouched. Suppressing that redundant case is the controller's diff, which
// compares only shortLabel — see DESIGN.md §4. Keeping the policy there rather than here means all
// diffing lives in one place.
class KeyboardLayoutChannelSource : public OsdChannelSource {
  Q_OBJECT

 public:
  explicit KeyboardLayoutChannelSource(KeyboardLayoutService* service, QObject* parent = nullptr);

  [[nodiscard]] QString channel() const override { return QStringLiteral("keyboard-layout"); }

  [[nodiscard]] bool isAvailable() const override { return available_; }

 private:
  void emitCurrentState();

  bool available_{false};
  KeyboardLayoutService* service_;  // non-owning; ShellApplication outlives this adapter
};

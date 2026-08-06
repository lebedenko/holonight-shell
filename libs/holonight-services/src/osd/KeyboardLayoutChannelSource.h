#pragma once

#include "OsdChannelSource.h"

#include <QObject>
#include <QString>

class KeyboardLayoutService;

// REQ-F-011. Adapts KeyboardLayoutService to the OSD's normalized selection channel.
//
// A pure translator: every layout signal produces an event, including a name-only change that
// leaves the code untouched. Suppressing that redundant case is the controller's diff, which
// compares only shortLabel — see DESIGN.md §4. Keeping the policy there rather than here means all
// diffing lives in one place.
class KeyboardLayoutChannelSource : public OsdChannelSource {
  Q_OBJECT

 public:
  explicit KeyboardLayoutChannelSource(KeyboardLayoutService* service, QObject* parent = nullptr);

  [[nodiscard]] QString channel() const override { return QStringLiteral("keyboard-layout"); }

  // Always true: KeyboardLayoutService has no availability concept distinct from "constructed".
  [[nodiscard]] bool isAvailable() const override { return true; }

 private:
  void emitCurrentState();

  KeyboardLayoutService* service_;  // non-owning; ShellApplication outlives this adapter
};

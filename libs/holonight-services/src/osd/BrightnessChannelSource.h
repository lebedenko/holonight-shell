#pragma once

#include "OsdChannelSource.h"

#include <QObject>
#include <QString>

class BrightnessService;

// REQ-F-010. Adapts BrightnessService to the OSD's normalized level channel.
//
// Deliberately never emits availableChanged: BrightnessService reports availability through
// hasBacklight, a CONSTANT property with no notify signal, so there is nothing to observe. An
// emission from this constructor would predate OsdController's connect() and be lost. The
// controller instead seeds availability by calling isAvailable() right after it connects — see
// DESIGN.md §4.
class BrightnessChannelSource : public OsdChannelSource {
  Q_OBJECT

 public:
  explicit BrightnessChannelSource(BrightnessService* service, QObject* parent = nullptr);

  [[nodiscard]] QString channel() const override { return QStringLiteral("screen-brightness"); }
  [[nodiscard]] bool isAvailable() const override;

 private:
  void emitLevel(int percent);

  BrightnessService* service_;  // non-owning; ShellApplication outlives this adapter
};

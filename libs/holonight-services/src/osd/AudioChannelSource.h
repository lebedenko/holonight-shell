#pragma once

#include "OsdChannelSource.h"

#include <QObject>
#include <QString>

class AudioService;

// REQ-F-009. Adapts AudioService to the OSD's normalized level channel. Volume and mute are two
// separate signals on the service but one event here: either one changing re-emits the full
// current state, so the renderer never has to merge partial updates.
class AudioChannelSource : public OsdChannelSource {
  Q_OBJECT

 public:
  explicit AudioChannelSource(AudioService* service, QObject* parent = nullptr);

  [[nodiscard]] QString channel() const override { return QStringLiteral("audio-volume"); }
  [[nodiscard]] bool isAvailable() const override;

 private:
  void emitCurrentState();

  AudioService* service_;  // non-owning; ShellApplication outlives this adapter
};

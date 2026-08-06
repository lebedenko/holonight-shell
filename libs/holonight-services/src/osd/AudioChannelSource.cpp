#include "AudioChannelSource.h"

#include "AudioService.h"

AudioChannelSource::AudioChannelSource(AudioService* service, QObject* parent)
    : OsdChannelSource(parent), service_(service) {
  if (service_ == nullptr) {
    return;
  }

  connect(service_, &AudioService::volumeChanged, this, &AudioChannelSource::emitCurrentState);
  connect(service_, &AudioService::mutedChanged, this, &AudioChannelSource::emitCurrentState);
  // AudioService::availableChanged() carries no argument, so it cannot be chained straight into
  // the base class's availableChanged(bool) — re-read the property and forward it.
  connect(service_, &AudioService::availableChanged, this, [this]() { emit availableChanged(service_->available()); });
}

bool AudioChannelSource::isAvailable() const { return service_ != nullptr && service_->available(); }

void AudioChannelSource::emitCurrentState() {
  emit eventObserved(OsdLevelEvent{.channel = channel(), .value = service_->volume(), .muted = service_->muted()});
}

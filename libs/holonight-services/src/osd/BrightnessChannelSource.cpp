#include "BrightnessChannelSource.h"

#include "BrightnessService.h"

BrightnessChannelSource::BrightnessChannelSource(BrightnessService* service, QObject* parent)
    : OsdChannelSource(parent), service_(service) {
  if (service_ == nullptr) {
    return;
  }

  connect(service_, &BrightnessService::brightnessPercentChanged, this, &BrightnessChannelSource::emitLevel);
}

bool BrightnessChannelSource::isAvailable() const { return service_ != nullptr && service_->hasBacklight(); }

void BrightnessChannelSource::emitLevel(int percent) {
  // Brightness has no mute concept; muted is structurally always false on this channel.
  emit eventObserved(OsdLevelEvent{.channel = channel(), .value = percent, .muted = false});
}

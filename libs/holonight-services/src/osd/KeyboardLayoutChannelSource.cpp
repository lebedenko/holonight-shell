#include "KeyboardLayoutChannelSource.h"

#include "KeyboardLayoutService.h"

KeyboardLayoutChannelSource::KeyboardLayoutChannelSource(KeyboardLayoutService* service, QObject* parent)
    : OsdChannelSource(parent), service_(service) {
  if (service_ == nullptr) {
    return;
  }

  // Both signals feed one handler, so a full layout switch translates twice. The first emission
  // carries the new name beside the still-old code, because KeyboardLayoutService commits the name
  // first; its shortLabel is therefore unchanged and the controller's shortLabel-only diff drops
  // it. The second carries the coherent pair and is the one that displays. Committing the code
  // first would swap which of the two survives the diff and put the wrong name on screen -- see
  // KeyboardLayoutChannelSourceTest.EmissionOrderKeepsTheMismatchedPairDiffable.
  available_ = service_->available();
  connect(service_, &KeyboardLayoutService::layoutCodeChanged, this, [this] {
    if (available_ != service_->available()) {
      available_ = service_->available();
      emit availableChanged(available_);
    }
    emitCurrentState();
  });
  connect(service_, &KeyboardLayoutService::layoutNameChanged, this, &KeyboardLayoutChannelSource::emitCurrentState);
}

void KeyboardLayoutChannelSource::emitCurrentState() {
  if (!service_->available() || service_->layoutName().isEmpty()) {
    return;
  }
  const QString code = service_->layoutCode();
  const QString name = service_->layoutName();

  // The fallback lives here, not in the renderer: fullLabel is never empty downstream, so the
  // renderer has one less case to handle (REQ-F-011).
  emit eventObserved(
      OsdSelectionEvent{.channel = channel(), .short_label = code, .full_label = name.isEmpty() ? code : name});
}
